/*
 * TeamSpeak 3 MassMover Plugin Header
 *
 * Copyright (c) Generated Plugin
 */

#ifndef MASSMOVER_H
#define MASSMOVER_H

#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32)
#define PLUGINS_EXPORTDLL __declspec(dllexport)
#else
#define PLUGINS_EXPORTDLL __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Required functions */
PLUGINS_EXPORTDLL const char* ts3plugin_name();
PLUGINS_EXPORTDLL const char* ts3plugin_version();
PLUGINS_EXPORTDLL int         ts3plugin_apiVersion();
PLUGINS_EXPORTDLL const char* ts3plugin_author();
PLUGINS_EXPORTDLL const char* ts3plugin_description();
PLUGINS_EXPORTDLL void        ts3plugin_setFunctionPointers(const struct TS3Functions funcs);
PLUGINS_EXPORTDLL int         ts3plugin_init();
PLUGINS_EXPORTDLL void        ts3plugin_shutdown();

/* Optional functions */
PLUGINS_EXPORTDLL int         ts3plugin_offersConfigure();
PLUGINS_EXPORTDLL void        ts3plugin_registerPluginID(const char* id);
PLUGINS_EXPORTDLL void        ts3plugin_initMenus(struct PluginMenuItem*** menuItems, char** menuIcon);
PLUGINS_EXPORTDLL void        ts3plugin_initHotkeys(struct PluginHotkey*** hotkeys);
PLUGINS_EXPORTDLL void        ts3plugin_onMenuItemEvent(uint64 serverConnectionHandlerID, enum PluginMenuType type, int menuItemID, uint64 selectedItemID);
PLUGINS_EXPORTDLL int         ts3plugin_onServerErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, const char* extraMessage);
PLUGINS_EXPORTDLL void        ts3plugin_onUpdateChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID);
PLUGINS_EXPORTDLL void        ts3plugin_onNewChannelCreatedEvent(uint64 serverConnectionHandlerID, uint64 channelID, uint64 channelParentID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier);
PLUGINS_EXPORTDLL void        ts3plugin_onDelChannelEvent(uint64 serverConnectionHandlerID, uint64 channelID, anyID invokerID, const char* invokerName, const char* invokerUniqueIdentifier);
PLUGINS_EXPORTDLL void        ts3plugin_onServerGroupListEvent(uint64 serverConnectionHandlerID, uint64 serverGroupID, const char* name, int type, int iconID, int saveDB);
PLUGINS_EXPORTDLL void        ts3plugin_onServerGroupListFinishedEvent(uint64 serverConnectionHandlerID);
PLUGINS_EXPORTDLL int         ts3plugin_onServerPermissionErrorEvent(uint64 serverConnectionHandlerID, const char* errorMessage, unsigned int error, const char* returnCode, unsigned int failedPermissionID);

/* Stub functions */
PLUGINS_EXPORTDLL void        ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID);
PLUGINS_EXPORTDLL const char* ts3plugin_infoTitle();
PLUGINS_EXPORTDLL void        ts3plugin_infoData(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, char** data);
PLUGINS_EXPORTDLL void        ts3plugin_freeMemory(void* data);
PLUGINS_EXPORTDLL int         ts3plugin_requestAutoload();

#ifdef __cplusplus
}
#endif

#endif
