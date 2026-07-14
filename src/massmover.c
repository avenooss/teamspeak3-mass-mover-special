/*
 * TeamSpeak 3 MassMover Plugin
 * 
 * This plugin adds a "MassMove here" context menu option to TeamSpeak 3 channels.
 * When activated, it moves all users from the target channel and its entire
 * subchannel hierarchy to the right-clicked channel.
 *
 * Key Features:
 * - One-click mass move of all users from a channel and its subchannels
 * - Recursive channel traversal to find all users
 * - Cross-platform support (Windows and Linux)
 * - Safe memory management and error handling
 * - Detailed logging for troubleshooting
 *
 * Author: Generated Plugin
 * Version: 1.5.0
 * License: Free to use and modify
 */

#ifdef _MSC_VER
#pragma warning(disable : 4100) /* Disable Unreferenced parameter warning */
#endif

#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32)
#include <windows.h>
#endif

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* TeamSpeak 3 SDK Headers */
#include "teamspeak/public_definitions.h"
#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "teamspeak/public_rare_definitions.h"
#include "ts3_functions.h"
#include "plugin_definitions.h"

#include "cJSON.h"

#include "massmover.h"

/* Global Variables */
static struct TS3Functions ts3Functions;  /* TeamSpeak 3 API function pointers */
static char* pluginID = NULL;            /* Plugin's unique identifier */

/* Constants */
#define PLUGIN_API_VERSION 26            /* TeamSpeak 3 API version we're using */
#define PATH_BUFSIZE 512                 /* Buffer size for file paths */
#define RETURNCODE_BUFSIZE 128           /* Buffer size for return codes */
#define MENU_ID_MASSMOVE 1              /* ID for our context menu item */
#define MENU_ID_EXPORT 2                /* ID for server structure export */
#define MENU_ID_RESTORE 3               /* ID for server structure restore */
#define MENU_ID_IMPORT_GROUPS 4         /* ID for server group import */
#define MENU_ID_DELETE_CHANNELS 5       /* ID for deleting non-default channels */
#define BACKUP_FORMAT "TS3MassMoverServerBackup"
#define BACKUP_VERSION 1
#define JSON_MAX_FILE_SIZE (16 * 1024 * 1024)
#define PERMISSION_BATCH_SIZE 50
#define DELETE_CONFIRM_SECONDS 15

/* Platform-specific string handling */
#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) \
    { \
        strncpy(dest, src, destSize - 1); \
        (dest)[destSize - 1] = '\0'; \
    }
#endif

/* Function Prototypes */
static struct PluginMenuItem* createMenuItem(enum PluginMenuType type, int id, const char* text, const char* icon);
static void collectSubchannels(uint64 serverConnectionHandlerID, uint64 parentChannelID, uint64** channels, int* channelCount, int* capacity);
static void collectParentChannels(uint64 serverConnectionHandlerID, uint64 channelID, uint64** channels, int* channelCount, int* capacity);
static anyID* collectClientsFromChannels(uint64 serverConnectionHandlerID, uint64* channels, int channelCount, int* clientCount);
static void startServerExport(uint64 serverConnectionHandlerID);
static void startServerRestore(uint64 serverConnectionHandlerID);
static void startServerGroupImport(uint64 serverConnectionHandlerID);
static void confirmOrStartChannelDelete(uint64 serverConnectionHandlerID);

struct ExportState {
    int active;
    uint64 serverConnectionHandlerID;
    uint64* channels;
    int channelCount;
    int descriptionIndex;
    char returnCode[RETURNCODE_BUFSIZE];
};

struct ChannelBackup {
    uint64 originalID;
    uint64 parentID;
    uint64 order;
    uint64 iconID;
    uint64 deleteDelay;
    uint64 restoredID;
    char* name;
    char* topic;
    char* description;
    char* password;
    int codec;
    int codecQuality;
    int maxClients;
    int maxFamilyClients;
    int isPermanent;
    int isSemiPermanent;
    int isDefault;
    int isPasswordProtected;
    int codecIsUnencrypted;
    int maxClientsUnlimited;
    int maxFamilyClientsUnlimited;
    int maxFamilyClientsInherited;
    int state; /* 0 = pending, 1 = creating, 2 = created, 3 = failed */
};

struct RestoreState {
    int active;
    int waitingForChannel;
    uint64 serverConnectionHandlerID;
    anyID ownClientID;
    struct ChannelBackup* channels;
    int channelCount;
    int currentIndex;
    int createdCount;
    int failedCount;
    int temporaryConvertedCount;
    char returnCode[RETURNCODE_BUFSIZE];
};

struct ImportedPermission {
    char* name;
    int value;
    int negated;
    int skip;
};

struct ImportedServerGroup {
    char* name;
    int type;
    struct ImportedPermission* permissions;
    int permissionCount;
    uint64 restoredID;
};

enum GroupImportPhase {
    GROUP_IMPORT_IDLE = 0,
    GROUP_IMPORT_ADDING,
    GROUP_IMPORT_LISTING,
    GROUP_IMPORT_APPLYING
};

struct GroupImportState {
    int active;
    enum GroupImportPhase phase;
    uint64 serverConnectionHandlerID;
    struct ImportedServerGroup* groups;
    int groupCount;
    int currentGroup;
    int permissionOffset;
    int pendingPermissionCount;
    int createdCount;
    int existingSkippedCount;
    int failedGroupCount;
    int appliedPermissionCount;
    int skippedPermissionCount;
    int failedPermissionCount;
    int invalidRecordCount;
    char returnCode[RETURNCODE_BUFSIZE];
};

struct ChannelDeleteItem {
    uint64 channelID;
    int depth;
};

struct ChannelDeleteState {
    int active;
    uint64 serverConnectionHandlerID;
    struct ChannelDeleteItem* channels;
    int channelCount;
    int currentIndex;
    int deletedCount;
    int failedCount;
    char returnCode[RETURNCODE_BUFSIZE];
};

static struct ExportState exportState;
static struct RestoreState restoreState;
static struct GroupImportState groupImportState;
static struct ChannelDeleteState channelDeleteState;
static uint64 deleteArmedServerConnectionHandlerID;
static time_t deleteArmedAt;

#ifdef _WIN32
/* Helper function to convert wchar_T to Utf-8 encoded strings on Windows */
static int wcharToUtf8(const wchar_t* str, char** result)
{
    int outlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
    *result = (char*)malloc(outlen);
    if (WideCharToMultiByte(CP_UTF8, 0, str, -1, *result, outlen, 0, 0) == 0) {
        *result = NULL;
        return -1;
    }
    return 0;
}
#endif

static char* duplicateString(const char* value)
{
    size_t size;
    char* result;

    if (!value) {
        value = "";
    }
    size = strlen(value) + 1;
    result = (char*)malloc(size);
    if (result) {
        memcpy(result, value, size);
    }
    return result;
}

static void logPluginMessage(uint64 serverConnectionHandlerID, enum LogLevel level, const char* message)
{
    ts3Functions.logMessage(message, level, "MassMover", serverConnectionHandlerID);
}

static void notifyCurrentTab(const char* message)
{
    ts3Functions.printMessageToCurrentTab(message);
}

static int buildDesktopNamedPath(char* path, size_t pathSize, const char* fileName)
{
    const char* home;
    int written;

#ifdef _WIN32
    home = getenv("USERPROFILE");
    if (!home || !home[0]) {
        return 0;
    }
    written = snprintf(path, pathSize, "%s\\Desktop\\%s", home, fileName);
#else
    home = getenv("HOME");
    if (!home || !home[0]) {
        return 0;
    }
    written = snprintf(path, pathSize, "%s/Desktop/%s", home, fileName);
#endif
    return written > 0 && (size_t)written < pathSize;
}

static int buildDesktopFilePath(char* path, size_t pathSize)
{
    return buildDesktopNamedPath(path, pathSize, "server.json");
}

static int getChannelString(uint64 serverConnectionHandlerID, uint64 channelID, size_t property, char** result)
{
    char* sdkValue = NULL;
    unsigned int error;

    *result = NULL;
    error = ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, channelID, property, &sdkValue);
    if (error != ERROR_ok || !sdkValue) {
        *result = duplicateString("");
        return *result != NULL;
    }
    *result = duplicateString(sdkValue);
    ts3Functions.freeMemory(sdkValue);
    return *result != NULL;
}

static int getChannelInt(uint64 serverConnectionHandlerID, uint64 channelID, size_t property, int defaultValue)
{
    int value = defaultValue;
    if (ts3Functions.getChannelVariableAsInt(serverConnectionHandlerID, channelID, property, &value) != ERROR_ok) {
        value = defaultValue;
    }
    return value;
}

static uint64 getChannelUInt64(uint64 serverConnectionHandlerID, uint64 channelID, size_t property, uint64 defaultValue)
{
    uint64 value = defaultValue;
    if (ts3Functions.getChannelVariableAsUInt64(serverConnectionHandlerID, channelID, property, &value) != ERROR_ok) {
        value = defaultValue;
    }
    return value;
}

static cJSON* addUInt64String(cJSON* object, const char* name, uint64 value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)value);
    return cJSON_AddStringToObject(object, name, buffer);
}

static cJSON* buildChannelJson(uint64 serverConnectionHandlerID, uint64 channelID)
{
    cJSON* channel;
    char* name = NULL;
    char* topic = NULL;
    char* description = NULL;
    uint64 parentID = 0;

    if (!getChannelString(serverConnectionHandlerID, channelID, CHANNEL_NAME, &name) ||
        !getChannelString(serverConnectionHandlerID, channelID, CHANNEL_TOPIC, &topic) ||
        !getChannelString(serverConnectionHandlerID, channelID, CHANNEL_DESCRIPTION, &description)) {
        free(name);
        free(topic);
        free(description);
        return NULL;
    }

    ts3Functions.getParentChannelOfChannel(serverConnectionHandlerID, channelID, &parentID);
    channel = cJSON_CreateObject();
    if (!channel ||
        !addUInt64String(channel, "id", channelID) ||
        !addUInt64String(channel, "parentId", parentID) ||
        !addUInt64String(channel, "order", getChannelUInt64(serverConnectionHandlerID, channelID, CHANNEL_ORDER, 0)) ||
        !cJSON_AddStringToObject(channel, "name", name) ||
        !cJSON_AddStringToObject(channel, "topic", topic) ||
        !cJSON_AddStringToObject(channel, "description", description) ||
        !cJSON_AddNumberToObject(channel, "codec", getChannelInt(serverConnectionHandlerID, channelID, CHANNEL_CODEC, 0)) ||
        !cJSON_AddNumberToObject(channel, "codecQuality", getChannelInt(serverConnectionHandlerID, channelID, CHANNEL_CODEC_QUALITY, 0)) ||
        !cJSON_AddNumberToObject(channel, "maxClients", getChannelInt(serverConnectionHandlerID, channelID, CHANNEL_MAXCLIENTS, -1)) ||
        !cJSON_AddNumberToObject(channel, "maxFamilyClients", getChannelInt(serverConnectionHandlerID, channelID, CHANNEL_MAXFAMILYCLIENTS, -1)) ||
        !cJSON_AddBoolToObject(channel, "permanent", getChannelInt(serverConnectionHandlerID, channelID, CHANNEL_FLAG_PERMANENT, 0)) ||
        !cJSON_AddBoolToObject(channel, "semiPermanent", getChannelInt(serverConnectionHandlerID, channelID, CHANNEL_FLAG_SEMI_PERMANENT, 0)) ||
        !cJSON_AddBoolToObject(channel, "default", getChannelInt(serverConnectionHandlerID, channelID, CHANNEL_FLAG_DEFAULT, 0)) ||
        !cJSON_AddBoolToObject(channel, "passwordProtected", getChannelInt(serverConnectionHandlerID, channelID, CHANNEL_FLAG_PASSWORD, 0)) ||
        !cJSON_AddNullToObject(channel, "password") ||
        !cJSON_AddBoolToObject(channel, "codecUnencrypted", getChannelInt(serverConnectionHandlerID, channelID, CHANNEL_CODEC_IS_UNENCRYPTED, 0)) ||
        !cJSON_AddBoolToObject(channel, "maxClientsUnlimited", getChannelInt(serverConnectionHandlerID, channelID, CHANNEL_FLAG_MAXCLIENTS_UNLIMITED, 0)) ||
        !cJSON_AddBoolToObject(channel, "maxFamilyClientsUnlimited", getChannelInt(serverConnectionHandlerID, channelID, CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED, 0)) ||
        !cJSON_AddBoolToObject(channel, "maxFamilyClientsInherited", getChannelInt(serverConnectionHandlerID, channelID, CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED, 0)) ||
        !addUInt64String(channel, "deleteDelay", getChannelUInt64(serverConnectionHandlerID, channelID, CHANNEL_DELETE_DELAY, 0)) ||
        !addUInt64String(channel, "iconId", getChannelUInt64(serverConnectionHandlerID, channelID, CHANNEL_ICON_ID, 0))) {
        cJSON_Delete(channel);
        channel = NULL;
    }

    free(name);
    free(topic);
    free(description);
    return channel;
}

static int writeJsonFile(const char* path, cJSON* root)
{
    char* json;
    FILE* file = NULL;
    size_t size;

    json = cJSON_Print(root);
    if (!json) {
        return 0;
    }
#ifdef _WIN32
    if (fopen_s(&file, path, "wb") != 0) {
        file = NULL;
    }
#else
    file = fopen(path, "wb");
#endif
    if (!file) {
        free(json);
        return 0;
    }
    size = strlen(json);
    if (fwrite(json, 1, size, file) != size) {
        fclose(file);
        free(json);
        return 0;
    }
    if (fclose(file) != 0) {
        free(json);
        return 0;
    }
    free(json);
    return 1;
}

static void clearExportState(void)
{
    free(exportState.channels);
    memset(&exportState, 0, sizeof(exportState));
}

static void finishServerExport(void)
{
    cJSON* root = NULL;
    cJSON* channels = NULL;
    cJSON* limitations = NULL;
    char path[PATH_BUFSIZE];
    char message[1024];
    char timestamp[64];
    time_t now;
    struct tm localTime;
    int i;
    int exportedCount = 0;

    root = cJSON_CreateObject();
    channels = cJSON_CreateArray();
    limitations = cJSON_CreateObject();
    if (!root || !channels || !limitations) {
        cJSON_Delete(root);
        cJSON_Delete(channels);
        cJSON_Delete(limitations);
        logPluginMessage(exportState.serverConnectionHandlerID, LogLevel_ERROR, "Export failed: JSON allocation error");
        notifyCurrentTab("[MassMover] Export failed: not enough memory.");
        clearExportState();
        return;
    }

    now = time(NULL);
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S%z", &localTime);

    cJSON_AddStringToObject(root, "format", BACKUP_FORMAT);
    cJSON_AddNumberToObject(root, "version", BACKUP_VERSION);
    cJSON_AddStringToObject(root, "createdAt", timestamp);
    cJSON_AddStringToObject(limitations, "channelPasswords", "TeamSpeak clients cannot read stored channel passwords; password is always null.");
    cJSON_AddStringToObject(limitations, "permissions", "Channel and server permission databases are not exposed by this backup.");
    cJSON_AddItemToObject(root, "limitations", limitations);
    cJSON_AddItemToObject(root, "channels", channels);

    for (i = 0; i < exportState.channelCount; i++) {
        cJSON* channel = buildChannelJson(exportState.serverConnectionHandlerID, exportState.channels[i]);
        if (channel) {
            cJSON_AddItemToArray(channels, channel);
            exportedCount++;
        }
    }

    if (!buildDesktopFilePath(path, sizeof(path)) || !writeJsonFile(path, root)) {
        logPluginMessage(exportState.serverConnectionHandlerID, LogLevel_ERROR, "Export failed: could not write Desktop/server.json");
        notifyCurrentTab("[MassMover] Export failed: could not write Desktop/server.json.");
    } else {
        snprintf(message, sizeof(message), "[MassMover] Export Complete\nChannels exported: %d\nFile: %s", exportedCount, path);
        logPluginMessage(exportState.serverConnectionHandlerID, LogLevel_INFO, message);
        notifyCurrentTab(message);
    }
    cJSON_Delete(root);
    clearExportState();
}

static void requestNextChannelDescription(void)
{
    unsigned int error;

    while (exportState.active && exportState.descriptionIndex < exportState.channelCount) {
        exportState.returnCode[0] = '\0';
        if (pluginID) {
            ts3Functions.createReturnCode(pluginID, exportState.returnCode, sizeof(exportState.returnCode));
        }
        error = ts3Functions.requestChannelDescription(
            exportState.serverConnectionHandlerID,
            exportState.channels[exportState.descriptionIndex],
            exportState.returnCode[0] ? exportState.returnCode : NULL);
        if (error == ERROR_ok) {
            return;
        }
        exportState.descriptionIndex++;
    }
    if (exportState.active) {
        finishServerExport();
    }
}

static void startServerExport(uint64 serverConnectionHandlerID)
{
    uint64* sdkChannels = NULL;
    unsigned int error;
    int count = 0;

    if (exportState.active || restoreState.active || groupImportState.active || channelDeleteState.active) {
        notifyCurrentTab("[MassMover] Another backup or restore operation is already running.");
        return;
    }
    error = ts3Functions.getChannelList(serverConnectionHandlerID, &sdkChannels);
    if (error != ERROR_ok || !sdkChannels) {
        logPluginMessage(serverConnectionHandlerID, LogLevel_ERROR, "Export failed: could not enumerate channels");
        notifyCurrentTab("[MassMover] Export failed: could not enumerate channels.");
        return;
    }
    while (sdkChannels[count] != 0) {
        count++;
    }
    exportState.channels = (uint64*)malloc((size_t)count * sizeof(uint64));
    if (count > 0 && !exportState.channels) {
        ts3Functions.freeMemory(sdkChannels);
        notifyCurrentTab("[MassMover] Export failed: not enough memory.");
        return;
    }
    if (count > 0) {
        memcpy(exportState.channels, sdkChannels, (size_t)count * sizeof(uint64));
    }
    ts3Functions.freeMemory(sdkChannels);
    exportState.active = 1;
    exportState.serverConnectionHandlerID = serverConnectionHandlerID;
    exportState.channelCount = count;
    exportState.descriptionIndex = 0;
    logPluginMessage(serverConnectionHandlerID, LogLevel_INFO, "Export started: requesting channel descriptions");
    requestNextChannelDescription();
}

static void freeChannelBackup(struct ChannelBackup* channel)
{
    free(channel->name);
    free(channel->topic);
    free(channel->description);
    free(channel->password);
}

static void clearRestoreState(void)
{
    int i;
    for (i = 0; i < restoreState.channelCount; i++) {
        freeChannelBackup(&restoreState.channels[i]);
    }
    free(restoreState.channels);
    memset(&restoreState, 0, sizeof(restoreState));
}

static char* readJsonFile(const char* path)
{
    FILE* file = NULL;
    long fileSize;
    char* data;

#ifdef _WIN32
    if (fopen_s(&file, path, "rb") != 0) {
        file = NULL;
    }
#else
    file = fopen(path, "rb");
#endif
    if (!file) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0 || (fileSize = ftell(file)) < 0 ||
        fileSize > JSON_MAX_FILE_SIZE || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    data = (char*)malloc((size_t)fileSize + 1);
    if (!data) {
        fclose(file);
        return NULL;
    }
    if (fread(data, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(data);
        fclose(file);
        return NULL;
    }
    data[fileSize] = '\0';
    fclose(file);
    return data;
}

static int jsonGetUInt64(cJSON* object, const char* name, uint64* value)
{
    cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);
    char* end;
    unsigned long long parsed;

    if (cJSON_IsString(item) && item->valuestring) {
        end = NULL;
        parsed = strtoull(item->valuestring, &end, 10);
        if (!end || end == item->valuestring || *end != '\0') {
            return 0;
        }
        *value = (uint64)parsed;
        return 1;
    }
    if (cJSON_IsNumber(item) && item->valuedouble >= 0) {
        *value = (uint64)item->valuedouble;
        return 1;
    }
    return 0;
}

static int jsonGetInt(cJSON* object, const char* name, int defaultValue)
{
    cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? item->valueint : defaultValue;
}

static int jsonGetBool(cJSON* object, const char* name, int defaultValue)
{
    cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsBool(item) ? cJSON_IsTrue(item) : defaultValue;
}

static char* jsonDuplicateString(cJSON* object, const char* name)
{
    cJSON* item = cJSON_GetObjectItemCaseSensitive(object, name);
    return duplicateString(cJSON_IsString(item) ? item->valuestring : "");
}

static int parseBackupChannels(cJSON* root, struct ChannelBackup** result, int* resultCount)
{
    cJSON* format = cJSON_GetObjectItemCaseSensitive(root, "format");
    cJSON* version = cJSON_GetObjectItemCaseSensitive(root, "version");
    cJSON* channels = cJSON_GetObjectItemCaseSensitive(root, "channels");
    struct ChannelBackup* parsed;
    int count;
    int i;

    *result = NULL;
    *resultCount = 0;
    if (!cJSON_IsString(format) || strcmp(format->valuestring, BACKUP_FORMAT) != 0 ||
        !cJSON_IsNumber(version) || version->valueint != BACKUP_VERSION || !cJSON_IsArray(channels)) {
        return 0;
    }
    count = cJSON_GetArraySize(channels);
    parsed = (struct ChannelBackup*)calloc((size_t)count, sizeof(struct ChannelBackup));
    if (count > 0 && !parsed) {
        return 0;
    }

    for (i = 0; i < count; i++) {
        cJSON* item = cJSON_GetArrayItem(channels, i);
        cJSON* name;
        int j;
        if (!cJSON_IsObject(item) ||
            !jsonGetUInt64(item, "id", &parsed[i].originalID) || parsed[i].originalID == 0 ||
            !jsonGetUInt64(item, "parentId", &parsed[i].parentID) ||
            !jsonGetUInt64(item, "order", &parsed[i].order)) {
            goto parse_error;
        }
        name = cJSON_GetObjectItemCaseSensitive(item, "name");
        if (!cJSON_IsString(name) || !name->valuestring || !name->valuestring[0]) {
            goto parse_error;
        }
        for (j = 0; j < i; j++) {
            if (parsed[j].originalID == parsed[i].originalID) {
                goto parse_error;
            }
        }
        parsed[i].name = duplicateString(name->valuestring);
        parsed[i].topic = jsonDuplicateString(item, "topic");
        parsed[i].description = jsonDuplicateString(item, "description");
        parsed[i].password = jsonDuplicateString(item, "password");
        if (!parsed[i].name || !parsed[i].topic || !parsed[i].description || !parsed[i].password) {
            goto parse_error;
        }
        parsed[i].codec = jsonGetInt(item, "codec", 0);
        parsed[i].codecQuality = jsonGetInt(item, "codecQuality", 0);
        parsed[i].maxClients = jsonGetInt(item, "maxClients", -1);
        parsed[i].maxFamilyClients = jsonGetInt(item, "maxFamilyClients", -1);
        parsed[i].isPermanent = jsonGetBool(item, "permanent", 0);
        parsed[i].isSemiPermanent = jsonGetBool(item, "semiPermanent", 0);
        parsed[i].isDefault = jsonGetBool(item, "default", 0);
        parsed[i].isPasswordProtected = jsonGetBool(item, "passwordProtected", 0);
        parsed[i].codecIsUnencrypted = jsonGetBool(item, "codecUnencrypted", 0);
        parsed[i].maxClientsUnlimited = jsonGetBool(item, "maxClientsUnlimited", 0);
        parsed[i].maxFamilyClientsUnlimited = jsonGetBool(item, "maxFamilyClientsUnlimited", 0);
        parsed[i].maxFamilyClientsInherited = jsonGetBool(item, "maxFamilyClientsInherited", 0);
        jsonGetUInt64(item, "deleteDelay", &parsed[i].deleteDelay);
        jsonGetUInt64(item, "iconId", &parsed[i].iconID);
    }
    *result = parsed;
    *resultCount = count;
    return 1;

parse_error:
    for (i = 0; i < count; i++) {
        freeChannelBackup(&parsed[i]);
    }
    free(parsed);
    return 0;
}

static int findBackupChannel(uint64 originalID)
{
    int i;
    for (i = 0; i < restoreState.channelCount; i++) {
        if (restoreState.channels[i].originalID == originalID) {
            return i;
        }
    }
    return -1;
}

static uint64 restoredIDFor(uint64 originalID)
{
    int index;
    if (originalID == 0) {
        return 0;
    }
    index = findBackupChannel(originalID);
    return index >= 0 && restoreState.channels[index].state == 2 ? restoreState.channels[index].restoredID : 0;
}

static int dependencyIsReady(uint64 originalID)
{
    int index;
    if (originalID == 0) {
        return 1;
    }
    index = findBackupChannel(originalID);
    return index < 0 || restoreState.channels[index].state == 2;
}

static void finishServerRestore(void)
{
    char message[1024];
    int created = restoreState.createdCount;
    int failed = restoreState.failedCount;
    int temporary = restoreState.temporaryConvertedCount;
    uint64 serverConnectionHandlerID = restoreState.serverConnectionHandlerID;

    snprintf(message, sizeof(message),
        "[MassMover] Restore Complete\nChannels created: %d\nFailed/skipped: %d\nTemporary restored as semi-permanent: %d\nStored passwords are unavailable; passwords manually supplied in JSON are applied.",
        created, failed, temporary);
    logPluginMessage(serverConnectionHandlerID, failed ? LogLevel_WARNING : LogLevel_INFO, message);
    notifyCurrentTab(message);
    clearRestoreState();
}

static void restoreNextChannel(void)
{
    struct ChannelBackup* channel;
    uint64 parentID;
    uint64 order;
    unsigned int error;
    int i;
    int index = -1;

    if (!restoreState.active || restoreState.waitingForChannel) {
        return;
    }
    for (i = 0; i < restoreState.channelCount; i++) {
        channel = &restoreState.channels[i];
        if (channel->state == 0 && dependencyIsReady(channel->parentID) && dependencyIsReady(channel->order)) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        int pending = 0;
        for (i = 0; i < restoreState.channelCount; i++) {
            if (restoreState.channels[i].state == 0) {
                restoreState.channels[i].state = 3;
                restoreState.failedCount++;
                pending++;
            }
        }
        if (pending) {
            logPluginMessage(restoreState.serverConnectionHandlerID, LogLevel_ERROR, "Restore skipped channels with missing or failed parent/order dependencies");
        }
        finishServerRestore();
        return;
    }

    channel = &restoreState.channels[index];
    parentID = restoredIDFor(channel->parentID);
    order = restoredIDFor(channel->order);
    restoreState.currentIndex = index;
    channel->state = 1;

    error = ts3Functions.setChannelVariableAsString(restoreState.serverConnectionHandlerID, 0, CHANNEL_NAME, channel->name);
    if (error != ERROR_ok) {
        channel->state = 3;
        restoreState.failedCount++;
        restoreNextChannel();
        return;
    }
    ts3Functions.setChannelVariableAsString(restoreState.serverConnectionHandlerID, 0, CHANNEL_TOPIC, channel->topic);
    ts3Functions.setChannelVariableAsString(restoreState.serverConnectionHandlerID, 0, CHANNEL_DESCRIPTION, channel->description);
    ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_CODEC, channel->codec);
    ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_CODEC_QUALITY, channel->codecQuality);
    ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_MAXCLIENTS, channel->maxClients);
    ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_MAXFAMILYCLIENTS, channel->maxFamilyClients);
    ts3Functions.setChannelVariableAsUInt64(restoreState.serverConnectionHandlerID, 0, CHANNEL_ORDER, order);
    ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_CODEC_IS_UNENCRYPTED, channel->codecIsUnencrypted);
    ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_FLAG_MAXCLIENTS_UNLIMITED, channel->maxClientsUnlimited);
    ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_FLAG_MAXFAMILYCLIENTS_UNLIMITED, channel->maxFamilyClientsUnlimited);
    ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_FLAG_MAXFAMILYCLIENTS_INHERITED, channel->maxFamilyClientsInherited);
    ts3Functions.setChannelVariableAsUInt64(restoreState.serverConnectionHandlerID, 0, CHANNEL_DELETE_DELAY, channel->deleteDelay);
    ts3Functions.setChannelVariableAsUInt64(restoreState.serverConnectionHandlerID, 0, CHANNEL_ICON_ID, channel->iconID);
    if (channel->password[0]) {
        ts3Functions.setChannelVariableAsString(restoreState.serverConnectionHandlerID, 0, CHANNEL_PASSWORD, channel->password);
        ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_FLAG_PASSWORD, 1);
    } else {
        ts3Functions.setChannelVariableAsString(restoreState.serverConnectionHandlerID, 0, CHANNEL_PASSWORD, "");
        ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_FLAG_PASSWORD, 0);
    }

    if (channel->isPermanent || channel->isDefault) {
        ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_FLAG_PERMANENT, 1);
        ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_FLAG_SEMI_PERMANENT, 0);
    } else {
        ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_FLAG_PERMANENT, 0);
        ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_FLAG_SEMI_PERMANENT, 1);
        if (!channel->isSemiPermanent) {
            restoreState.temporaryConvertedCount++;
        }
    }
    ts3Functions.setChannelVariableAsInt(restoreState.serverConnectionHandlerID, 0, CHANNEL_FLAG_DEFAULT, channel->isDefault ? 1 : 0);

    restoreState.returnCode[0] = '\0';
    if (pluginID) {
        ts3Functions.createReturnCode(pluginID, restoreState.returnCode, sizeof(restoreState.returnCode));
    }
    restoreState.waitingForChannel = 1;
    error = ts3Functions.flushChannelCreation(
        restoreState.serverConnectionHandlerID,
        parentID,
        restoreState.returnCode[0] ? restoreState.returnCode : NULL);
    if (error != ERROR_ok) {
        restoreState.waitingForChannel = 0;
        channel->state = 3;
        restoreState.failedCount++;
        restoreNextChannel();
    }
}

static void startServerRestore(uint64 serverConnectionHandlerID)
{
    char path[PATH_BUFSIZE];
    char* jsonData;
    cJSON* root;
    struct ChannelBackup* channels = NULL;
    int channelCount = 0;
    unsigned int error;

    if (exportState.active || restoreState.active || groupImportState.active || channelDeleteState.active) {
        notifyCurrentTab("[MassMover] Another backup or restore operation is already running.");
        return;
    }
    if (!buildDesktopFilePath(path, sizeof(path))) {
        notifyCurrentTab("[MassMover] Restore failed: Desktop path is unavailable.");
        return;
    }
    jsonData = readJsonFile(path);
    if (!jsonData) {
        notifyCurrentTab("[MassMover] Restore failed: Desktop/server.json could not be read or is too large.");
        return;
    }
    root = cJSON_Parse(jsonData);
    free(jsonData);
    if (!root || !parseBackupChannels(root, &channels, &channelCount)) {
        cJSON_Delete(root);
        notifyCurrentTab("[MassMover] Restore failed: server.json is invalid or unsupported.");
        return;
    }
    cJSON_Delete(root);
    error = ts3Functions.getClientID(serverConnectionHandlerID, &restoreState.ownClientID);
    if (error != ERROR_ok) {
        int i;
        for (i = 0; i < channelCount; i++) {
            freeChannelBackup(&channels[i]);
        }
        free(channels);
        notifyCurrentTab("[MassMover] Restore failed: current client ID is unavailable.");
        return;
    }
    restoreState.active = 1;
    restoreState.serverConnectionHandlerID = serverConnectionHandlerID;
    restoreState.channels = channels;
    restoreState.channelCount = channelCount;
    logPluginMessage(serverConnectionHandlerID, LogLevel_INFO, "Restore started from Desktop/server.json");
    restoreNextChannel();
}

static char* trimWhitespace(char* text)
{
    char* end;
    while (*text && isspace((unsigned char)*text)) {
        text++;
    }
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        end--;
    }
    *end = '\0';
    return text;
}

static void freeImportedServerGroup(struct ImportedServerGroup* group)
{
    int i;
    free(group->name);
    for (i = 0; i < group->permissionCount; i++) {
        free(group->permissions[i].name);
    }
    free(group->permissions);
}

static void clearGroupImportState(void)
{
    int i;
    for (i = 0; i < groupImportState.groupCount; i++) {
        freeImportedServerGroup(&groupImportState.groups[i]);
    }
    free(groupImportState.groups);
    memset(&groupImportState, 0, sizeof(groupImportState));
}

static int splitTextLines(char* data, char*** resultLines, int* resultCount)
{
    char** lines;
    char* current;
    char* newline;
    int capacity = 1;
    int count = 0;

    for (current = data; *current; current++) {
        if (*current == '\n') {
            capacity++;
        }
    }
    lines = (char**)malloc((size_t)capacity * sizeof(char*));
    if (!lines) {
        return 0;
    }
    current = data;
    while (*current) {
        lines[count++] = current;
        newline = strchr(current, '\n');
        if (!newline) {
            break;
        }
        if (newline > current && newline[-1] == '\r') {
            newline[-1] = '\0';
        }
        *newline = '\0';
        current = newline + 1;
    }
    *resultLines = lines;
    *resultCount = count;
    return 1;
}

static int parseImportedPermissions(char* permissionLine, struct ImportedServerGroup* group)
{
    char* body;
    char* segment;
    char* separator;
    int count = 1;
    int index = 0;

    if (strncmp(permissionLine, "sgid=", 5) != 0 || !(body = strchr(permissionLine, ' '))) {
        return 0;
    }
    body++;
    for (segment = body; *segment; segment++) {
        if (*segment == '|') {
            count++;
        }
    }
    group->permissions = (struct ImportedPermission*)calloc((size_t)count, sizeof(struct ImportedPermission));
    if (!group->permissions) {
        return 0;
    }
    group->permissionCount = count;

    segment = body;
    while (segment && *segment) {
        char permissionName[256];
        struct ImportedPermission* permission = &group->permissions[index];
        separator = strchr(segment, '|');
        if (separator) {
            *separator = '\0';
        }
        if (sscanf(segment, "permsid=%255s permvalue=%d permnegated=%d permskip=%d",
                permissionName, &permission->value, &permission->negated, &permission->skip) != 4 ||
            (permission->negated != 0 && permission->negated != 1) ||
            (permission->skip != 0 && permission->skip != 1)) {
            return 0;
        }
        permission->name = duplicateString(permissionName);
        if (!permission->name) {
            return 0;
        }
        index++;
        segment = separator ? separator + 1 : NULL;
    }
    group->permissionCount = index;
    return index > 0;
}

static int parseServerGroupImport(char* data, struct ImportedServerGroup** resultGroups,
    int* resultCount, int* invalidRecordCount)
{
    char** lines = NULL;
    int lineCount = 0;
    int recordCount;
    int validCount = 0;
    int invalidCount = 0;
    int i;
    struct ImportedServerGroup* groups;

    *resultGroups = NULL;
    *resultCount = 0;
    *invalidRecordCount = 0;
    if (!splitTextLines(data, &lines, &lineCount)) {
        return 0;
    }
    if (lineCount > 0 && (unsigned char)lines[0][0] == 0xEF &&
        (unsigned char)lines[0][1] == 0xBB && (unsigned char)lines[0][2] == 0xBF) {
        lines[0] += 3;
    }
    recordCount = lineCount / 3;
    invalidCount += lineCount % 3 ? 1 : 0;
    groups = (struct ImportedServerGroup*)calloc((size_t)recordCount, sizeof(struct ImportedServerGroup));
    if (recordCount > 0 && !groups) {
        free(lines);
        return 0;
    }

    for (i = 0; i < recordCount; i++) {
        char* name = trimWhitespace(lines[i * 3]);
        char* typeText = trimWhitespace(lines[i * 3 + 1]);
        char* permissionLine = trimWhitespace(lines[i * 3 + 2]);
        char* typeEnd = NULL;
        long type = strtol(typeText, &typeEnd, 10);
        struct ImportedServerGroup candidate;
        memset(&candidate, 0, sizeof(candidate));

        if (!name[0] || !typeEnd || typeEnd == typeText || *typeEnd != '\0' || type < 0 || type > 2) {
            invalidCount++;
            continue;
        }
        candidate.name = duplicateString(name);
        candidate.type = (int)type;
        if (!candidate.name || !parseImportedPermissions(permissionLine, &candidate)) {
            freeImportedServerGroup(&candidate);
            invalidCount++;
            continue;
        }
        groups[validCount++] = candidate;
    }
    free(lines);
    *resultGroups = groups;
    *resultCount = validCount;
    *invalidRecordCount = invalidCount;
    return validCount > 0;
}

static int groupImportSucceeded(unsigned int error)
{
    return error == ERROR_ok || error == ERROR_ok_no_update;
}

static void importNextServerGroup(void);
static void applyNextPermissionBatch(void);

static void finishServerGroupImport(void)
{
    char message[1024];
    uint64 serverConnectionHandlerID = groupImportState.serverConnectionHandlerID;
    snprintf(message, sizeof(message),
        "[MassMover] Server Group Import Complete\nGroups created: %d\nExisting groups skipped: %d\nGroups failed: %d\nPermissions applied: %d\nPermissions unknown/skipped: %d\nPermission batches failed: %d\nInvalid records skipped: %d\nNo users were assigned automatically.",
        groupImportState.createdCount, groupImportState.existingSkippedCount,
        groupImportState.failedGroupCount, groupImportState.appliedPermissionCount,
        groupImportState.skippedPermissionCount, groupImportState.failedPermissionCount,
        groupImportState.invalidRecordCount);
    logPluginMessage(serverConnectionHandlerID,
        (groupImportState.failedGroupCount || groupImportState.failedPermissionCount) ? LogLevel_WARNING : LogLevel_INFO,
        message);
    notifyCurrentTab(message);
    clearGroupImportState();
}

static void requestImportedGroupList(void)
{
    unsigned int error;
    groupImportState.phase = GROUP_IMPORT_LISTING;
    groupImportState.returnCode[0] = '\0';
    if (pluginID) {
        ts3Functions.createReturnCode(pluginID, groupImportState.returnCode, sizeof(groupImportState.returnCode));
    }
    error = ts3Functions.requestServerGroupList(groupImportState.serverConnectionHandlerID,
        groupImportState.returnCode[0] ? groupImportState.returnCode : NULL);
    if (error != ERROR_ok) {
        groupImportState.failedGroupCount++;
        groupImportState.currentGroup++;
        importNextServerGroup();
    }
}

static void applyNextPermissionBatch(void)
{
    unsigned int permissionIDs[PERMISSION_BATCH_SIZE];
    int values[PERMISSION_BATCH_SIZE];
    int negated[PERMISSION_BATCH_SIZE];
    int skip[PERMISSION_BATCH_SIZE];
    struct ImportedServerGroup* group;
    int batchSize = 0;
    unsigned int error;

    if (!groupImportState.active || groupImportState.currentGroup >= groupImportState.groupCount) {
        return;
    }
    group = &groupImportState.groups[groupImportState.currentGroup];
    while (groupImportState.permissionOffset < group->permissionCount && batchSize < PERMISSION_BATCH_SIZE) {
        struct ImportedPermission* permission = &group->permissions[groupImportState.permissionOffset++];
        unsigned int permissionID = 0;
        if (ts3Functions.getPermissionIDByName(groupImportState.serverConnectionHandlerID,
                permission->name, &permissionID) != ERROR_ok || permissionID == 0) {
            groupImportState.skippedPermissionCount++;
            continue;
        }
        permissionIDs[batchSize] = permissionID;
        values[batchSize] = permission->value;
        negated[batchSize] = permission->negated;
        skip[batchSize] = permission->skip;
        batchSize++;
    }
    if (batchSize == 0) {
        if (groupImportState.permissionOffset < group->permissionCount) {
            applyNextPermissionBatch();
            return;
        }
        groupImportState.currentGroup++;
        importNextServerGroup();
        return;
    }

    groupImportState.phase = GROUP_IMPORT_APPLYING;
    groupImportState.pendingPermissionCount = batchSize;
    groupImportState.returnCode[0] = '\0';
    if (pluginID) {
        ts3Functions.createReturnCode(pluginID, groupImportState.returnCode, sizeof(groupImportState.returnCode));
    }
    error = ts3Functions.requestServerGroupAddPerm(groupImportState.serverConnectionHandlerID,
        group->restoredID, 1, permissionIDs, values, negated, skip, batchSize,
        groupImportState.returnCode[0] ? groupImportState.returnCode : NULL);
    if (error != ERROR_ok) {
        groupImportState.failedPermissionCount += batchSize;
        applyNextPermissionBatch();
    }
}

static void importNextServerGroup(void)
{
    struct ImportedServerGroup* group;
    unsigned int existingID = 0;
    unsigned int error;

    if (!groupImportState.active) {
        return;
    }
    if (groupImportState.currentGroup >= groupImportState.groupCount) {
        finishServerGroupImport();
        return;
    }
    group = &groupImportState.groups[groupImportState.currentGroup];
    if (ts3Functions.getServerGroupIDByName(groupImportState.serverConnectionHandlerID,
            group->name, &existingID) == ERROR_ok && existingID != 0) {
        groupImportState.existingSkippedCount++;
        groupImportState.currentGroup++;
        importNextServerGroup();
        return;
    }

    groupImportState.phase = GROUP_IMPORT_ADDING;
    groupImportState.permissionOffset = 0;
    groupImportState.returnCode[0] = '\0';
    if (pluginID) {
        ts3Functions.createReturnCode(pluginID, groupImportState.returnCode, sizeof(groupImportState.returnCode));
    }
    error = ts3Functions.requestServerGroupAdd(groupImportState.serverConnectionHandlerID,
        group->name, group->type, groupImportState.returnCode[0] ? groupImportState.returnCode : NULL);
    if (error != ERROR_ok) {
        groupImportState.failedGroupCount++;
        groupImportState.currentGroup++;
        importNextServerGroup();
    }
}

static void completeServerGroupImportRequest(unsigned int error, const char* errorMessage)
{
    int succeeded = groupImportSucceeded(error);
    (void)errorMessage;
    if (!groupImportState.active) {
        return;
    }
    if (groupImportState.phase == GROUP_IMPORT_ADDING) {
        if (succeeded) {
            requestImportedGroupList();
        } else {
            groupImportState.failedGroupCount++;
            groupImportState.currentGroup++;
            importNextServerGroup();
        }
    } else if (groupImportState.phase == GROUP_IMPORT_LISTING) {
        if (!succeeded) {
            groupImportState.failedGroupCount++;
            groupImportState.currentGroup++;
            importNextServerGroup();
        }
    } else if (groupImportState.phase == GROUP_IMPORT_APPLYING) {
        if (succeeded) {
            groupImportState.appliedPermissionCount += groupImportState.pendingPermissionCount;
        } else {
            groupImportState.failedPermissionCount += groupImportState.pendingPermissionCount;
        }
        groupImportState.pendingPermissionCount = 0;
        applyNextPermissionBatch();
    }
}

static void startServerGroupImport(uint64 serverConnectionHandlerID)
{
    char path[PATH_BUFSIZE];
    char* data;
    struct ImportedServerGroup* groups = NULL;
    int groupCount = 0;
    int invalidRecordCount = 0;

    if (exportState.active || restoreState.active || groupImportState.active || channelDeleteState.active) {
        notifyCurrentTab("[MassMover] Another backup, restore, or group import operation is already running.");
        return;
    }
    if (!buildDesktopNamedPath(path, sizeof(path), "servergroups.txt")) {
        notifyCurrentTab("[MassMover] Group import failed: Desktop path is unavailable.");
        return;
    }
    data = readJsonFile(path);
    if (!data) {
        notifyCurrentTab("[MassMover] Group import failed: Desktop/servergroups.txt could not be read or is too large.");
        return;
    }
    if (!parseServerGroupImport(data, &groups, &groupCount, &invalidRecordCount)) {
        free(data);
        notifyCurrentTab("[MassMover] Group import failed: servergroups.txt contains no valid groups.");
        return;
    }
    free(data);
    groupImportState.active = 1;
    groupImportState.serverConnectionHandlerID = serverConnectionHandlerID;
    groupImportState.groups = groups;
    groupImportState.groupCount = groupCount;
    groupImportState.invalidRecordCount = invalidRecordCount;
    logPluginMessage(serverConnectionHandlerID, LogLevel_INFO, "Server group import started from Desktop/servergroups.txt");
    importNextServerGroup();
}

static void clearChannelDeleteState(void)
{
    free(channelDeleteState.channels);
    memset(&channelDeleteState, 0, sizeof(channelDeleteState));
}

static int compareChannelDeleteItems(const void* left, const void* right)
{
    const struct ChannelDeleteItem* a = (const struct ChannelDeleteItem*)left;
    const struct ChannelDeleteItem* b = (const struct ChannelDeleteItem*)right;
    if (a->depth != b->depth) {
        return b->depth - a->depth;
    }
    return a->channelID < b->channelID ? 1 : (a->channelID > b->channelID ? -1 : 0);
}

static int getChannelDepth(uint64 serverConnectionHandlerID, uint64 channelID, int maximumDepth)
{
    int depth = 0;
    uint64 current = channelID;
    while (current != 0 && depth <= maximumDepth) {
        uint64 parent = 0;
        if (ts3Functions.getParentChannelOfChannel(serverConnectionHandlerID, current, &parent) != ERROR_ok) {
            break;
        }
        current = parent;
        depth++;
    }
    return depth;
}

static void finishChannelDelete(void)
{
    char message[512];
    uint64 serverConnectionHandlerID = channelDeleteState.serverConnectionHandlerID;
    snprintf(message, sizeof(message),
        "[MassMover] Channel Cleanup Complete\nChannels deleted: %d\nFailed/skipped: %d\nDefault channel preserved.",
        channelDeleteState.deletedCount, channelDeleteState.failedCount);
    logPluginMessage(serverConnectionHandlerID,
        channelDeleteState.failedCount ? LogLevel_WARNING : LogLevel_INFO, message);
    notifyCurrentTab(message);
    clearChannelDeleteState();
}

static void requestNextChannelDelete(void)
{
    unsigned int error;
    while (channelDeleteState.active && channelDeleteState.currentIndex < channelDeleteState.channelCount) {
        channelDeleteState.returnCode[0] = '\0';
        if (pluginID) {
            ts3Functions.createReturnCode(pluginID, channelDeleteState.returnCode, sizeof(channelDeleteState.returnCode));
        }
        error = ts3Functions.requestChannelDelete(
            channelDeleteState.serverConnectionHandlerID,
            channelDeleteState.channels[channelDeleteState.currentIndex].channelID,
            1,
            channelDeleteState.returnCode[0] ? channelDeleteState.returnCode : NULL);
        if (error == ERROR_ok) {
            return;
        }
        channelDeleteState.failedCount++;
        channelDeleteState.currentIndex++;
    }
    if (channelDeleteState.active) {
        finishChannelDelete();
    }
}

static void startChannelDelete(uint64 serverConnectionHandlerID)
{
    uint64* sdkChannels = NULL;
    uint64 defaultChannelID = 0;
    int channelCount = 0;
    int candidateCount = 0;
    int i;
    unsigned int error;

    error = ts3Functions.getChannelList(serverConnectionHandlerID, &sdkChannels);
    if (error != ERROR_ok || !sdkChannels) {
        notifyCurrentTab("[MassMover] Channel cleanup failed: could not enumerate channels.");
        return;
    }
    while (sdkChannels[channelCount] != 0) {
        int isDefault = 0;
        if (ts3Functions.getChannelVariableAsInt(serverConnectionHandlerID, sdkChannels[channelCount],
                CHANNEL_FLAG_DEFAULT, &isDefault) == ERROR_ok && isDefault) {
            defaultChannelID = sdkChannels[channelCount];
        }
        channelCount++;
    }
    if (defaultChannelID == 0) {
        ts3Functions.freeMemory(sdkChannels);
        logPluginMessage(serverConnectionHandlerID, LogLevel_ERROR, "Channel cleanup aborted: default channel could not be identified");
        notifyCurrentTab("[MassMover] Channel cleanup aborted: default channel could not be identified safely.");
        return;
    }
    channelDeleteState.channels = (struct ChannelDeleteItem*)calloc((size_t)channelCount, sizeof(struct ChannelDeleteItem));
    if (channelCount > 0 && !channelDeleteState.channels) {
        ts3Functions.freeMemory(sdkChannels);
        notifyCurrentTab("[MassMover] Channel cleanup failed: not enough memory.");
        return;
    }
    for (i = 0; i < channelCount; i++) {
        if (sdkChannels[i] == defaultChannelID) {
            continue;
        }
        channelDeleteState.channels[candidateCount].channelID = sdkChannels[i];
        channelDeleteState.channels[candidateCount].depth = getChannelDepth(serverConnectionHandlerID, sdkChannels[i], channelCount);
        candidateCount++;
    }
    ts3Functions.freeMemory(sdkChannels);
    qsort(channelDeleteState.channels, (size_t)candidateCount, sizeof(struct ChannelDeleteItem), compareChannelDeleteItems);
    channelDeleteState.active = 1;
    channelDeleteState.serverConnectionHandlerID = serverConnectionHandlerID;
    channelDeleteState.channelCount = candidateCount;
    logPluginMessage(serverConnectionHandlerID, LogLevel_WARNING, "Channel cleanup started: deleting all non-default channels from deepest to shallowest");
    requestNextChannelDelete();
}

static void confirmOrStartChannelDelete(uint64 serverConnectionHandlerID)
{
    time_t now = time(NULL);
    if (exportState.active || restoreState.active || groupImportState.active || channelDeleteState.active) {
        notifyCurrentTab("[MassMover] Another backup, restore, group import, or cleanup operation is already running.");
        return;
    }
    if (deleteArmedServerConnectionHandlerID == serverConnectionHandlerID &&
        deleteArmedAt != 0 && difftime(now, deleteArmedAt) <= DELETE_CONFIRM_SECONDS) {
        deleteArmedServerConnectionHandlerID = 0;
        deleteArmedAt = 0;
        startChannelDelete(serverConnectionHandlerID);
        return;
    }
    deleteArmedServerConnectionHandlerID = serverConnectionHandlerID;
    deleteArmedAt = now;
    notifyCurrentTab("[MassMover] DANGER: Click 'Delete All Non-Default Channels' again within 15 seconds to confirm. This cannot be undone.");
}

/*********************************** Required Plugin Functions ************************************/

/* Unique name identifying this plugin */
const char* ts3plugin_name()
{
#ifdef _WIN32
    static char* result = NULL;
    if (!result) {
        const wchar_t* name = L"MassMover";
        if (wcharToUtf8(name, &result) == -1) {
            result = "MassMover";
        }
    }
    return result;
#else
    return "MassMover";
#endif
}

/* Plugin version */
const char* ts3plugin_version()
{
    return "1.5.0";
}

/* Plugin API version */
int ts3plugin_apiVersion()
{
    return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author()
{
    return "Generated Plugin";
}

/* Plugin description */
const char* ts3plugin_description()
{
    return "Mass move clients, restore channel structures, import groups, and safely clean channels without ServerQuery.";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs)
{
    ts3Functions = funcs;
}

/* Plugin initialization */
int ts3plugin_init()
{
    printf("MASSMOVER: Plugin initialized\n");
    return 0;
}

/* Plugin cleanup */
void ts3plugin_shutdown()
{
    printf("MASSMOVER: Plugin shutdown\n");

    clearExportState();
    clearRestoreState();
    clearGroupImportState();
    clearChannelDeleteState();
    
    if (pluginID) {
        free(pluginID);
        pluginID = NULL;
    }
}

/****************************** Optional Plugin Functions ********************************/

/* Tell client we don't offer a configuration window */
int ts3plugin_offersConfigure()
{
    return PLUGIN_OFFERS_NO_CONFIGURE;
}

/* Register plugin ID */
void ts3plugin_registerPluginID(const char* id)
{
    const size_t sz = strlen(id) + 1;
    pluginID = (char*)malloc(sz * sizeof(char));
    _strcpy(pluginID, sz, id);
    printf("MASSMOVER: registerPluginID: %s\n", pluginID);
}

/* Helper function to create menu item */
static struct PluginMenuItem* createMenuItem(enum PluginMenuType type, int id, const char* text, const char* icon)
{
    struct PluginMenuItem* menuItem = (struct PluginMenuItem*)malloc(sizeof(struct PluginMenuItem));
    if (!menuItem) {
        printf("MASSMOVER: Failed to allocate memory for menu item\n");
        return NULL;
    }
    
    menuItem->type = type;
    menuItem->id = id;
    _strcpy(menuItem->text, PLUGIN_MENU_BUFSZ, text);
    _strcpy(menuItem->icon, PLUGIN_MENU_BUFSZ, icon);
    return menuItem;
}

/* Initialize menus - add our context menu item */
void ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon)
{
    struct PluginMenuItem** items;
    
    /* Allocate memory for 5 menu items plus NULL terminator */
    items = (struct PluginMenuItem**)malloc(6 * sizeof(struct PluginMenuItem*));
    if (!items) {
        printf("MASSMOVER: Failed to allocate memory for menu items\n");
        *menuItems = NULL;
        *menuIcon = NULL;
        return;
    }
    
    /* Create the "MassMove here" menu item for channels */
    items[0] = createMenuItem(PLUGIN_MENU_TYPE_CHANNEL, MENU_ID_MASSMOVE, "MassMove here", "");
    if (!items[0]) {
        free(items);
        *menuItems = NULL;
        *menuIcon = NULL;
        return;
    }
    
    items[1] = createMenuItem(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_EXPORT, "Export Server Structure", "");
    items[2] = createMenuItem(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_RESTORE, "Restore Server Structure", "");
    items[3] = createMenuItem(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_IMPORT_GROUPS, "Import Server Groups", "");
    items[4] = createMenuItem(PLUGIN_MENU_TYPE_GLOBAL, MENU_ID_DELETE_CHANNELS, "Delete All Non-Default Channels", "");
    if (!items[1] || !items[2] || !items[3] || !items[4]) {
        free(items[0]);
        free(items[1]);
        free(items[2]);
        free(items[3]);
        free(items[4]);
        free(items);
        *menuItems = NULL;
        *menuIcon = NULL;
        return;
    }

    /* Terminate array with NULL */
    items[5] = NULL;
    
    /* Return the items */
    *menuItems = items;
    
    /* Optional icon for the plugin menu (not used here) */
    *menuIcon = (char*)malloc(PLUGIN_MENU_BUFSZ * sizeof(char));
    if (*menuIcon) {
        _strcpy(*menuIcon, PLUGIN_MENU_BUFSZ, "");
    }
}

/* Recursive function to collect all subchannels of a given channel */
static void collectSubchannels(uint64 serverConnectionHandlerID, uint64 parentChannelID, uint64** channels, int* channelCount, int* capacity)
{
    uint64* channelList;
    unsigned int error;
    int i;
    
    /* Get list of all channels on the server */
    error = ts3Functions.getChannelList(serverConnectionHandlerID, &channelList);
    if (error != ERROR_ok) {
        printf("MASSMOVER: Error getting channel list: %d\n", error);
        return;
    }
    
    /* Check each channel to see if it's a subchannel of parentChannelID */
    for (i = 0; channelList[i] != 0; i++) {
        uint64 channelParent;
        
        /* Get the parent of this channel */
        error = ts3Functions.getParentChannelOfChannel(serverConnectionHandlerID, channelList[i], &channelParent);
        if (error != ERROR_ok) {
            continue;
        }
        
        /* If this channel's parent is our target parent, add it and recurse */
        if (channelParent == parentChannelID) {
            /* Expand array if needed */
            if (*channelCount >= *capacity) {
                *capacity *= 2;
                *channels = (uint64*)realloc(*channels, *capacity * sizeof(uint64));
                if (!*channels) {
                    printf("MASSMOVER: Failed to reallocate memory for channels\n");
                    ts3Functions.freeMemory(channelList);
                    return;
                }
            }
            
            /* Add this channel to our list */
            (*channels)[*channelCount] = channelList[i];
            (*channelCount)++;
            
            /* Recursively collect subchannels of this channel */
            collectSubchannels(serverConnectionHandlerID, channelList[i], channels, channelCount, capacity);
        }
    }
    
    /* Free the channel list */
    ts3Functions.freeMemory(channelList);
}

/* Function to collect parent channels up to a certain level */
static void collectParentChannels(uint64 serverConnectionHandlerID, uint64 channelID, uint64** channels, int* channelCount, int* capacity)
{
    uint64 currentChannelID = channelID;
    unsigned int error;
    
    while (1) {
        uint64 parentChannelID;
        
        /* Get the parent of current channel */
        error = ts3Functions.getParentChannelOfChannel(serverConnectionHandlerID, currentChannelID, &parentChannelID);
        if (error != ERROR_ok || parentChannelID == 0) {
            break;  /* Stop if we hit an error or reach the root channel */
        }
        
        /* Expand array if needed */
        if (*channelCount >= *capacity) {
            *capacity *= 2;
            *channels = (uint64*)realloc(*channels, *capacity * sizeof(uint64));
            if (!*channels) {
                printf("MASSMOVER: Failed to reallocate memory for parent channels\n");
                return;
            }
        }
        
        /* Add parent channel to our list */
        (*channels)[*channelCount] = parentChannelID;
        (*channelCount)++;
        
        /* Move up to parent channel */
        currentChannelID = parentChannelID;
    }
}

/* Function to collect all clients from a list of channels */
static anyID* collectClientsFromChannels(uint64 serverConnectionHandlerID, uint64* channels, int channelCount, int* clientCount)
{
    anyID* allClients = NULL;
    int totalClients = 0;
    int capacity = 16;
    int i, j;
    
    /* Allocate initial array */
    allClients = (anyID*)malloc(capacity * sizeof(anyID));
    if (!allClients) {
        printf("MASSMOVER: Failed to allocate memory for clients\n");
        return NULL;
    }
    
    /* For each channel, get its clients */
    for (i = 0; i < channelCount; i++) {
        anyID* channelClients;
        unsigned int error;
        
        /* Get clients in this channel */
        error = ts3Functions.getChannelClientList(serverConnectionHandlerID, channels[i], &channelClients);
        if (error != ERROR_ok) {
            printf("MASSMOVER: Error getting clients for channel %llu: %d\n", (unsigned long long)channels[i], error);
            continue;
        }
        
        /* Add each client to our list */
        for (j = 0; channelClients[j] != 0; j++) {
            /* Expand array if needed */
            if (totalClients >= capacity - 1) { /* Leave room for terminator */
                capacity *= 2;
                allClients = (anyID*)realloc(allClients, capacity * sizeof(anyID));
                if (!allClients) {
                    printf("MASSMOVER: Failed to reallocate memory for clients\n");
                    ts3Functions.freeMemory(channelClients);
                    return NULL;
                }
            }
            
            /* Add client to list */
            allClients[totalClients] = channelClients[j];
            totalClients++;
        }
        
        /* Free the channel client list */
        ts3Functions.freeMemory(channelClients);
    }
    
    /* Terminate the array with 0 */
    allClients[totalClients] = 0;
    
    *clientCount = totalClients;
    return allClients;
}

/* Handle menu item events */
void ts3plugin_onMenuItemEvent(uint64 serverConnectionHandlerID, enum PluginMenuType type, int menuItemID, uint64 selectedItemID)
{
    if (type == PLUGIN_MENU_TYPE_GLOBAL && menuItemID == MENU_ID_EXPORT) {
        startServerExport(serverConnectionHandlerID);
        return;
    }
    if (type == PLUGIN_MENU_TYPE_GLOBAL && menuItemID == MENU_ID_RESTORE) {
        startServerRestore(serverConnectionHandlerID);
        return;
    }
    if (type == PLUGIN_MENU_TYPE_GLOBAL && menuItemID == MENU_ID_IMPORT_GROUPS) {
        startServerGroupImport(serverConnectionHandlerID);
        return;
    }
    if (type == PLUGIN_MENU_TYPE_GLOBAL && menuItemID == MENU_ID_DELETE_CHANNELS) {
        confirmOrStartChannelDelete(serverConnectionHandlerID);
        return;
    }
    if (type == PLUGIN_MENU_TYPE_CHANNEL && menuItemID == MENU_ID_MASSMOVE) {
        uint64 targetChannelID = selectedItemID;
        uint64* channels = NULL;
        int channelCount = 1; /* Start with 1 for the target channel itself */
        int capacity = 1024;
        anyID* clientsToMove = NULL;
        int clientCount;
        char returnCode[RETURNCODE_BUFSIZE];
        unsigned int error;
        char msg[512];
        anyID myID;
        int i;
        int myIDFound = 0;
        
        /* Get our own client ID */
        error = ts3Functions.getClientID(serverConnectionHandlerID, &myID);
        if (error != ERROR_ok) {
            ts3Functions.logMessage("MassMover: Failed to get client ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
        
        snprintf(msg, sizeof(msg), "MassMover: Starting mass move operation for channel %llu", (unsigned long long)targetChannelID);
        ts3Functions.logMessage(msg, LogLevel_INFO, "Plugin", serverConnectionHandlerID);
        
        /* Allocate initial channel array and add the target channel */
        channels = (uint64*)malloc(capacity * sizeof(uint64));
        if (!channels) {
            ts3Functions.logMessage("MassMover: Failed to allocate memory for channels", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
        channels[0] = targetChannelID;
        
        /* Collect parent channels */
        collectParentChannels(serverConnectionHandlerID, targetChannelID, &channels, &channelCount, &capacity);
        
        /* For each channel (including target and parents), collect their subchannels */
        for (i = 0; i < channelCount; i++) {
            collectSubchannels(serverConnectionHandlerID, channels[i], &channels, &channelCount, &capacity);
        }
        
        snprintf(msg, sizeof(msg), "MassMover: Found %d channels to move clients from", channelCount);
        ts3Functions.logMessage(msg, LogLevel_INFO, "Plugin", serverConnectionHandlerID);
        
        /* Collect all clients from these channels */
        clientsToMove = collectClientsFromChannels(serverConnectionHandlerID, channels, channelCount, &clientCount);
        if (!clientsToMove) {
            ts3Functions.logMessage("MassMover: Failed to collect clients from channels", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            free(channels);
            return;
        }
        
        snprintf(msg, sizeof(msg), "MassMover: Found %d clients to move", clientCount);
        ts3Functions.logMessage(msg, LogLevel_INFO, "Plugin", serverConnectionHandlerID);
        
        if (clientCount > 0) {
            /* Create return code for the move operation */
            ts3Functions.createReturnCode(pluginID, returnCode, RETURNCODE_BUFSIZE);
            
            /* First move all other clients */
            anyID* otherClients = NULL;
            int otherClientCount = 0;
            int otherClientCapacity = 16;
            
            /* Allocate initial array for other clients */
            otherClients = (anyID*)malloc(otherClientCapacity * sizeof(anyID));
            if (!otherClients) {
                ts3Functions.logMessage("MassMover: Failed to allocate memory for other clients", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                free(channels);
                free(clientsToMove);
                return;
            }
            
            /* Filter out our own ID from the client list */
            for (i = 0; i < clientCount; i++) {
                if (clientsToMove[i] == myID) {
                    myIDFound = 1;
                    continue;
                }
                
                /* Expand array if needed */
                if (otherClientCount >= otherClientCapacity - 1) {
                    otherClientCapacity *= 2;
                    otherClients = (anyID*)realloc(otherClients, otherClientCapacity * sizeof(anyID));
                    if (!otherClients) {
                        ts3Functions.logMessage("MassMover: Failed to reallocate memory for other clients", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                        free(channels);
                        free(clientsToMove);
                        return;
                    }
                }
                
                otherClients[otherClientCount] = clientsToMove[i];
                otherClientCount++;
            }
            
            /* Terminate the array with 0 */
            if (otherClientCount > 0) {
                otherClients[otherClientCount] = 0;
                
                /* Move other clients */
                error = ts3Functions.requestClientsMove(serverConnectionHandlerID, otherClients, targetChannelID, "", returnCode);
                if (error != ERROR_ok) {
                    snprintf(msg, sizeof(msg), "MassMover: Error moving other clients: %d", error);
                    ts3Functions.logMessage(msg, LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                }
            }
            
            /* Move ourselves separately */
            if (myIDFound) {
                error = ts3Functions.requestClientMove(serverConnectionHandlerID, myID, targetChannelID, "", returnCode);
                if (error != ERROR_ok) {
                    snprintf(msg, sizeof(msg), "MassMover: Error moving self: %d", error);
                    ts3Functions.logMessage(msg, LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                }
            }
            
            snprintf(msg, sizeof(msg), "MassMover: Successfully initiated move for %d clients to channel %llu", clientCount, (unsigned long long)targetChannelID);
            ts3Functions.logMessage(msg, LogLevel_INFO, "Plugin", serverConnectionHandlerID);
            
            free(otherClients);
        } else {
            ts3Functions.logMessage("MassMover: No clients found to move", LogLevel_INFO, "Plugin", serverConnectionHandlerID);
        }
        
        /* Cleanup */
        free(channels);
        free(clientsToMove);
    }
}

/* Error handling */
int ts3plugin_onServerErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, const char* extraMessage)
{
    if (channelDeleteState.active && serverConnectionHandlerID == channelDeleteState.serverConnectionHandlerID &&
        returnCode && channelDeleteState.returnCode[0] && strcmp(returnCode, channelDeleteState.returnCode) == 0) {
        if (error != ERROR_ok) {
            channelDeleteState.failedCount++;
            channelDeleteState.currentIndex++;
            requestNextChannelDelete();
        }
        return 1;
    }
    if (groupImportState.active && serverConnectionHandlerID == groupImportState.serverConnectionHandlerID &&
        returnCode && groupImportState.returnCode[0] && strcmp(returnCode, groupImportState.returnCode) == 0) {
        completeServerGroupImportRequest(error, errorMessage);
        return 1;
    }
    if (exportState.active && serverConnectionHandlerID == exportState.serverConnectionHandlerID &&
        returnCode && exportState.returnCode[0] && strcmp(returnCode, exportState.returnCode) == 0 && error != ERROR_ok) {
        exportState.descriptionIndex++;
        requestNextChannelDescription();
        return 1;
    }
    if (restoreState.active && serverConnectionHandlerID == restoreState.serverConnectionHandlerID &&
        returnCode && restoreState.returnCode[0] && strcmp(returnCode, restoreState.returnCode) == 0 && error != ERROR_ok) {
        struct ChannelBackup* channel = &restoreState.channels[restoreState.currentIndex];
        char message[512];
        snprintf(message, sizeof(message), "Restore failed creating channel '%s': %s (%u)", channel->name, errorMessage, error);
        logPluginMessage(serverConnectionHandlerID, LogLevel_ERROR, message);
        restoreState.waitingForChannel = 0;
        channel->state = 3;
        restoreState.failedCount++;
        restoreNextChannel();
        return 1;
    }
    if (returnCode && pluginID && strncmp(returnCode, pluginID, strlen(pluginID)) == 0) {
        printf("MASSMOVER: Server error: %s (%d)\n", errorMessage, error);
    }
    return 0; /* Let other plugins handle this too */
}

void ts3plugin_onUpdateChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID)
{
    if (exportState.active && serverConnectionHandlerID == exportState.serverConnectionHandlerID &&
        exportState.descriptionIndex < exportState.channelCount &&
        exportState.channels[exportState.descriptionIndex] == channelID) {
        exportState.descriptionIndex++;
        requestNextChannelDescription();
    }
}

void ts3plugin_onNewChannelCreatedEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID,
    anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier)
{
    struct ChannelBackup* channel;
    char* channelName = NULL;
    uint64 expectedParent;

    if (!restoreState.active || !restoreState.waitingForChannel ||
        serverConnectionHandlerID != restoreState.serverConnectionHandlerID || invokerID != restoreState.ownClientID) {
        return;
    }
    channel = &restoreState.channels[restoreState.currentIndex];
    expectedParent = restoredIDFor(channel->parentID);
    if (channelParentID != expectedParent ||
        !getChannelString(serverConnectionHandlerID, channelID, CHANNEL_NAME, &channelName) ||
        strcmp(channelName, channel->name) != 0) {
        free(channelName);
        return;
    }
    free(channelName);
    channel->restoredID = channelID;
    channel->state = 2;
    restoreState.createdCount++;
    restoreState.waitingForChannel = 0;
    restoreNextChannel();
}

void ts3plugin_onDelChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID,
    anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier)
{
    (void)invokerID;
    (void)invokerName;
    (void)invokerUniqueIdentifier;
    if (!channelDeleteState.active || serverConnectionHandlerID != channelDeleteState.serverConnectionHandlerID ||
        channelDeleteState.currentIndex >= channelDeleteState.channelCount ||
        channelDeleteState.channels[channelDeleteState.currentIndex].channelID != channelID) {
        return;
    }
    channelDeleteState.deletedCount++;
    channelDeleteState.currentIndex++;
    requestNextChannelDelete();
}

void ts3plugin_onServerGroupListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID,
    const char* name, int type, int iconID, int saveDB)
{
    struct ImportedServerGroup* group;
    (void)type;
    (void)iconID;
    (void)saveDB;
    if (!groupImportState.active || groupImportState.phase != GROUP_IMPORT_LISTING ||
        serverConnectionHandlerID != groupImportState.serverConnectionHandlerID ||
        groupImportState.currentGroup >= groupImportState.groupCount) {
        return;
    }
    group = &groupImportState.groups[groupImportState.currentGroup];
    if (name && strcmp(name, group->name) == 0) {
        group->restoredID = serverGroupID;
    }
}

void ts3plugin_onServerGroupListFinishedEvent(uint64 serverConnectionHandlerID)
{
    struct ImportedServerGroup* group;
    if (!groupImportState.active || groupImportState.phase != GROUP_IMPORT_LISTING ||
        serverConnectionHandlerID != groupImportState.serverConnectionHandlerID ||
        groupImportState.currentGroup >= groupImportState.groupCount) {
        return;
    }
    group = &groupImportState.groups[groupImportState.currentGroup];
    if (group->restoredID == 0) {
        groupImportState.failedGroupCount++;
        groupImportState.currentGroup++;
        importNextServerGroup();
        return;
    }
    groupImportState.createdCount++;
    groupImportState.permissionOffset = 0;
    applyNextPermissionBatch();
}

int ts3plugin_onServerPermissionErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage,
    unsigned int error, const char* returnCode, unsigned int failedPermissionID)
{
    (void)failedPermissionID;
    if (groupImportState.active && serverConnectionHandlerID == groupImportState.serverConnectionHandlerID &&
        returnCode && groupImportState.returnCode[0] && strcmp(returnCode, groupImportState.returnCode) == 0) {
        completeServerGroupImportRequest(error, errorMessage);
        return 1;
    }
    return 0;
}

/****************************** Unused Plugin Callbacks ********************************/

/* All the remaining callback functions that we don't need */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID) {}
const char* ts3plugin_infoTitle() { return NULL; }
void ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, char** data) { *data = NULL; }
void ts3plugin_freeMemory(void* data) { free(data); }
int ts3plugin_requestAutoload() { return 0; }
void ts3plugin_initHotkeys(struct PluginHotkey*** hotkeys) { *hotkeys = NULL; }
