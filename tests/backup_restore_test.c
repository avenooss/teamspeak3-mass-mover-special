#include <direct.h>

#include "../src/massmover.c"

static char stagedName[TS3_MAX_SIZE_CHANNEL_NAME + 1];
static uint64 nextCreatedChannelID = 100;
static int createdChannelCount = 0;
static int returnCodeCounter = 0;
static uint64 createdParents[2];
static char stagedGroupName[128];
static uint64 stagedGroupID = 200;
static int importedGroupCount = 0;
static int importedPermissionCount = 0;
static int deletedChannelCount = 0;
static uint64 deletedChannelIDs[2];

static unsigned int mockFreeMemory(void* pointer)
{
    free(pointer);
    return ERROR_ok;
}

static unsigned int mockLogMessage(const char* message, enum LogLevel level, const char* channel, uint64 logID)
{
    (void)message;
    (void)level;
    (void)channel;
    (void)logID;
    return ERROR_ok;
}

static void mockPrintMessage(const char* message)
{
    (void)message;
}

static void mockCreateReturnCode(const char* id, char* returnCode, size_t maxLen)
{
    snprintf(returnCode, maxLen, "%s-%d", id, ++returnCodeCounter);
}

static unsigned int mockGetChannelList(uint64 serverConnectionHandlerID, uint64** result)
{
    (void)serverConnectionHandlerID;
    *result = (uint64*)malloc(3 * sizeof(uint64));
    if (!*result) {
        return ERROR_out_of_memory;
    }
    (*result)[0] = 10;
    (*result)[1] = 11;
    (*result)[2] = 0;
    return ERROR_ok;
}

static unsigned int mockGetParent(uint64 serverConnectionHandlerID, uint64 channelID, uint64* result)
{
    (void)serverConnectionHandlerID;
    *result = channelID == 11 ? 10 : 0;
    return ERROR_ok;
}

static unsigned int mockGetChannelString(uint64 serverConnectionHandlerID, uint64 channelID, size_t property, char** result)
{
    const char* value = "";
    (void)serverConnectionHandlerID;

    if (channelID >= 100 && property == CHANNEL_NAME) {
        value = stagedName;
    } else if (property == CHANNEL_NAME) {
        value = channelID == 10 ? "Root" : "Child";
    } else if (property == CHANNEL_TOPIC) {
        value = channelID == 10 ? "Root topic" : "Child topic";
    } else if (property == CHANNEL_DESCRIPTION) {
        value = channelID == 10 ? "Root description" : "Child description";
    }
    *result = duplicateString(value);
    return *result ? ERROR_ok : ERROR_out_of_memory;
}

static unsigned int mockGetChannelInt(uint64 serverConnectionHandlerID, uint64 channelID, size_t property, int* result)
{
    (void)serverConnectionHandlerID;
    (void)channelID;
    switch (property) {
        case CHANNEL_CODEC: *result = 4; break;
        case CHANNEL_CODEC_QUALITY: *result = 7; break;
        case CHANNEL_MAXCLIENTS: *result = 25; break;
        case CHANNEL_MAXFAMILYCLIENTS: *result = 50; break;
        case CHANNEL_FLAG_PERMANENT: *result = 1; break;
        case CHANNEL_FLAG_DEFAULT: *result = channelID == 10; break;
        default: *result = 0; break;
    }
    return ERROR_ok;
}

static unsigned int mockGetChannelUInt64(uint64 serverConnectionHandlerID, uint64 channelID, size_t property, uint64* result)
{
    (void)serverConnectionHandlerID;
    (void)channelID;
    *result = property == CHANNEL_ICON_ID ? 123 : 0;
    return ERROR_ok;
}

static unsigned int mockRequestDescription(uint64 serverConnectionHandlerID, uint64 channelID, const char* returnCode)
{
    (void)returnCode;
    ts3plugin_onUpdateChannelEvent(serverConnectionHandlerID, channelID);
    return ERROR_ok;
}

static unsigned int mockGetClientID(uint64 serverConnectionHandlerID, anyID* result)
{
    (void)serverConnectionHandlerID;
    *result = 7;
    return ERROR_ok;
}

static unsigned int mockSetChannelString(uint64 serverConnectionHandlerID, uint64 channelID, size_t property, const char* value)
{
    (void)serverConnectionHandlerID;
    (void)channelID;
    if (property == CHANNEL_NAME) {
        snprintf(stagedName, sizeof(stagedName), "%s", value);
    }
    return ERROR_ok;
}

static unsigned int mockSetChannelInt(uint64 serverConnectionHandlerID, uint64 channelID, size_t property, int value)
{
    (void)serverConnectionHandlerID;
    (void)channelID;
    (void)property;
    (void)value;
    return ERROR_ok;
}

static unsigned int mockSetChannelUInt64(uint64 serverConnectionHandlerID, uint64 channelID, size_t property, uint64 value)
{
    (void)serverConnectionHandlerID;
    (void)channelID;
    (void)property;
    (void)value;
    return ERROR_ok;
}

static unsigned int mockFlushChannelCreation(uint64 serverConnectionHandlerID, uint64 parentID, const char* returnCode)
{
    uint64 createdID = nextCreatedChannelID++;
    (void)returnCode;
    assert(createdChannelCount < 2);
    createdParents[createdChannelCount] = parentID;
    createdChannelCount++;
    ts3plugin_onNewChannelCreatedEvent(serverConnectionHandlerID, createdID, parentID, 7, "Test", "test-uid");
    return ERROR_ok;
}

static unsigned int mockGetServerGroupIDByName(uint64 serverConnectionHandlerID, const char* name, unsigned int* result)
{
    (void)serverConnectionHandlerID;
    (void)name;
    *result = 0;
    return ERROR_undefined;
}

static unsigned int mockRequestServerGroupAdd(uint64 serverConnectionHandlerID, const char* name, int type, const char* returnCode)
{
    (void)type;
    snprintf(stagedGroupName, sizeof(stagedGroupName), "%s", name);
    importedGroupCount++;
    ts3plugin_onServerErrorEvent(serverConnectionHandlerID, "ok", ERROR_ok, returnCode, "");
    return ERROR_ok;
}

static unsigned int mockRequestServerGroupList(uint64 serverConnectionHandlerID, const char* returnCode)
{
    (void)returnCode;
    ts3plugin_onServerGroupListEvent(serverConnectionHandlerID, stagedGroupID++, stagedGroupName, 1, 0, 1);
    ts3plugin_onServerGroupListFinishedEvent(serverConnectionHandlerID);
    return ERROR_ok;
}

static unsigned int mockGetPermissionIDByName(uint64 serverConnectionHandlerID, const char* name, unsigned int* result)
{
    static unsigned int nextPermissionID = 1;
    (void)serverConnectionHandlerID;
    (void)name;
    *result = nextPermissionID++;
    return ERROR_ok;
}

static unsigned int mockRequestServerGroupAddPerm(uint64 serverConnectionHandlerID, uint64 serverGroupID,
    int continueOnError, const unsigned int* permissionIDs, const int* values, const int* negated,
    const int* skip, int arraySize, const char* returnCode)
{
    (void)serverGroupID;
    (void)continueOnError;
    (void)permissionIDs;
    (void)values;
    (void)negated;
    (void)skip;
    importedPermissionCount += arraySize;
    ts3plugin_onServerErrorEvent(serverConnectionHandlerID, "ok", ERROR_ok, returnCode, "");
    return ERROR_ok;
}

static unsigned int mockRequestChannelDelete(uint64 serverConnectionHandlerID, uint64 channelID,
    int force, const char* returnCode)
{
    (void)force;
    (void)returnCode;
    assert(deletedChannelCount < 2);
    deletedChannelIDs[deletedChannelCount++] = channelID;
    ts3plugin_onDelChannelEvent(serverConnectionHandlerID, channelID, 7, "Test", "test-uid");
    return ERROR_ok;
}

int main(void)
{
    char* jsonData;
    cJSON* root;
    cJSON* channels;
    struct TS3Functions functions;
    FILE* groupFile;

    _mkdir("build\\testhome");
    _mkdir("build\\testhome\\Desktop");
    _putenv_s("USERPROFILE", "build\\testhome");

    memset(&functions, 0, sizeof(functions));
    functions.freeMemory = mockFreeMemory;
    functions.logMessage = mockLogMessage;
    functions.printMessageToCurrentTab = mockPrintMessage;
    functions.createReturnCode = mockCreateReturnCode;
    functions.getChannelList = mockGetChannelList;
    functions.getParentChannelOfChannel = mockGetParent;
    functions.getChannelVariableAsString = mockGetChannelString;
    functions.getChannelVariableAsInt = mockGetChannelInt;
    functions.getChannelVariableAsUInt64 = mockGetChannelUInt64;
    functions.requestChannelDescription = mockRequestDescription;
    functions.getClientID = mockGetClientID;
    functions.setChannelVariableAsString = mockSetChannelString;
    functions.setChannelVariableAsInt = mockSetChannelInt;
    functions.setChannelVariableAsUInt64 = mockSetChannelUInt64;
    functions.flushChannelCreation = mockFlushChannelCreation;
    functions.getServerGroupIDByName = mockGetServerGroupIDByName;
    functions.requestServerGroupAdd = mockRequestServerGroupAdd;
    functions.requestServerGroupList = mockRequestServerGroupList;
    functions.getPermissionIDByName = mockGetPermissionIDByName;
    functions.requestServerGroupAddPerm = mockRequestServerGroupAddPerm;
    functions.requestChannelDelete = mockRequestChannelDelete;
    ts3plugin_setFunctionPointers(functions);
    ts3plugin_registerPluginID("backup-test");

    startServerExport(1);
    assert(!exportState.active);
    jsonData = readJsonFile("build\\testhome\\Desktop\\server.json");
    assert(jsonData != NULL);
    root = cJSON_Parse(jsonData);
    free(jsonData);
    assert(root != NULL);
    channels = cJSON_GetObjectItemCaseSensitive(root, "channels");
    assert(cJSON_IsArray(channels));
    assert(cJSON_GetArraySize(channels) == 2);
    assert(strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(channels, 1), "parentId")->valuestring, "10") == 0);
    cJSON_Delete(root);

    startServerRestore(1);
    assert(!restoreState.active);
    assert(createdChannelCount == 2);
    assert(nextCreatedChannelID == 102);
    assert(createdParents[0] == 0);
    assert(createdParents[1] == 100);

    groupFile = fopen("build\\testhome\\Desktop\\servergroups.txt", "wb");
    assert(groupFile != NULL);
    fputs("Test Admin\r\n1\r\nsgid=10 permsid=i_group_sort_id permvalue=10 permnegated=0 permskip=0|permsid=i_client_move_power permvalue=50 permnegated=0 permskip=1\r\n", groupFile);
    fputs("Test Member\r\n1\r\nsgid=11 permsid=b_virtualserver_info_view permvalue=1 permnegated=0 permskip=0|permsid=i_client_poke_power permvalue=25 permnegated=0 permskip=0\r\n", groupFile);
    fclose(groupFile);
    startServerGroupImport(1);
    assert(!groupImportState.active);
    assert(importedGroupCount == 2);
    assert(importedPermissionCount == 4);

    confirmOrStartChannelDelete(1);
    assert(!channelDeleteState.active);
    confirmOrStartChannelDelete(1);
    assert(!channelDeleteState.active);
    assert(deletedChannelCount == 1);
    assert(deletedChannelIDs[0] == 11);

    ts3plugin_shutdown();
    return 0;
}
