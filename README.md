# TeamSpeak 3 MassMover Plugin

TS3MassMover is a TeamSpeak 3 client plugin with two independent feature sets:

- Mass-move clients from a channel hierarchy.
- Export and restore a server's channel structure through the local TeamSpeak client.

It does not connect to ServerQuery and does not require the ServerQuery port to be open. All operations use the TeamSpeak Plugin SDK and the identity and permissions of the currently connected client.

## Channel backup and restore

The global plugin menu contains:

- `Export Server Structure`
- `Restore Server Structure`
- `Import Server Groups`
- `Delete All Non-Default Channels`

Export writes a pretty-printed `server.json` file to the current user's Desktop. It includes channel hierarchy, sibling order, names, topics, descriptions, codec settings, client limits, lifecycle flags, password-protected flags, delete delay, and icon IDs.

Restore reads `Desktop/server.json` and creates channels one at a time. Parent channels and preceding siblings are created before their dependants, and TeamSpeak callbacks are used to map old channel IDs to newly assigned IDs.

## Importing server groups and permissions

`Import Server Groups` reads `Desktop/servergroups.txt`. The supported UTF-8 text format contains three lines per group: the group name, numeric group type, and a ServerQuery-style permission line containing `sgid`, `permsid`, `permvalue`, `permnegated`, and `permskip` fields.

The old `sgid` value is used only as source metadata. The plugin creates each group through the local Plugin SDK, discovers its new destination ID, resolves every permission by `permsid`, and applies permissions in asynchronous batches.

For safety:

- Existing destination groups with the same name are skipped and never modified.
- Empty group names and malformed records are skipped.
- Permissions unknown to the destination server version are skipped and reported.
- No clients are automatically assigned to imported groups.
- The connected identity must have enough group and permission modify power.

### Restore workflow

1. Connect the TeamSpeak client to the source server.
2. Choose `Plugins > Export Server Structure`.
3. Copy the generated `server.json` to the Desktop of the machine performing the restore, if necessary.
4. Connect the same TeamSpeak client to the destination server using an identity with channel-creation permissions.
5. Choose `Plugins > Restore Server Structure`.
6. Review the TeamSpeak client log and completion message.

Do not run Export and Restore concurrently.

## Removing the old channel structure

`Delete All Non-Default Channels` removes the destination server's existing channel tree before a Restore. TeamSpeak's default channel is always preserved. Channels are sorted by depth and deleted from deepest child to shallowest parent with forced deletion, so connected clients may be moved by the server.

This operation cannot be undone. As an accidental-click safeguard, the menu item must be selected twice on the same server within 15 seconds. The first click only arms the operation and prints a warning; the second click starts deletion. Groups, permissions, clients, and the default channel are not deleted.

Recommended replacement workflow:

1. Export and retain a verified backup before cleanup.
2. Connect to the destination using an identity with channel-delete permission.
3. Select `Delete All Non-Default Channels` twice within 15 seconds.
4. Wait for `Channel Cleanup Complete`.
5. Run `Restore Server Structure` once.

Never test cleanup for the first time on a production server.

## Important limitations

- TeamSpeak clients cannot read stored channel passwords. Export records `passwordProtected`, but writes `password: null`. A password may be entered manually into the JSON before Restore; otherwise the restored channel is unprotected.
- Server groups are not part of `server.json`; they can be imported separately from `servergroups.txt`. Channel groups, client memberships, tokens, bans, virtual-server configuration, files, avatars, and client identities are not transferred.
- Restore succeeds only where the connected client has sufficient permissions on the destination server.
- Empty temporary channels would be deleted while a client-side restore is still running. They are therefore restored as semi-permanent channels and reported in the completion message.
- The destination server's existing channels are not deleted or overwritten. Restore creates additional channels.

## MassMove

Right-click a channel and choose `MassMove here`. The plugin recursively collects clients from the selected channel, its ancestors, and their subchannels, then requests that they be moved to the selected channel. The connected user is handled separately.

## Building

Prerequisites:

- TeamSpeak 3 Plugin SDK v26 in `ts3client-pluginsdk-26`
- GCC/MinGW or Microsoft Visual C++

Download the SDK automatically on Linux:

```bash
chmod +x setup_sdk.sh
./setup_sdk.sh
```

Linux:

```bash
chmod +x build.sh
./build.sh
```

Windows:

```cmd
build_windows.bat
```

cJSON 1.7.19 is vendored in `cjson/` and compiled by both build scripts.

## Installation

Linux:

```bash
cp bin/linux/massmover.so ~/.ts3client/plugins/
```

Windows:

```cmd
copy bin\windows\massmover.dll %APPDATA%\TS3Client\plugins\
```

Restart TeamSpeak and enable MassMover under `Tools > Options > Addons` or the plugin manager used by your client version.

## Testing

`tests/backup_restore_test.c` provides a local mock-SDK integration test. It exercises description retrieval, JSON serialization and parsing, parent-ID mapping, and asynchronous channel creation without contacting a TeamSpeak server.

## License

The plugin is licensed under the MIT License. cJSON retains its upstream MIT license notice in its source files.
