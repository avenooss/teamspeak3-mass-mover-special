# TeamSpeak 3 MassMover Special

[![CI](https://github.com/avenooss/teamspeak3-mass-mover-special/actions/workflows/ci.yml/badge.svg)](https://github.com/avenooss/teamspeak3-mass-mover-special/actions/workflows/ci.yml)
[![Version](https://img.shields.io/badge/version-1.5.0-blue.svg)](CHANGELOG.md)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

TS3MassMover Special extends the original MassMover client plugin with channel
backup, restore, cleanup, and server-group migration. It uses TeamSpeak 3 Plugin
SDK v26 and the permissions of the currently connected client.

> **No ServerQuery connection is used.** The ServerQuery port may remain closed.

## Features

| Feature | Plugin menu action | Input/output |
| --- | --- | --- |
| Move clients recursively | `MassMove here` | Current TeamSpeak connection |
| Export channel structure | `Export Server Structure` | Writes `Desktop/server.json` |
| Restore channel structure | `Restore Server Structure` | Reads `Desktop/server.json` |
| Import server groups | `Import Server Groups` | Reads `Desktop/servergroups.txt` |
| Remove existing channels | `Delete All Non-Default Channels` | Preserves the default channel |

Channel export includes hierarchy, sibling order, names, topics, descriptions,
codec settings, client limits, lifecycle flags, password-protection flags,
delete delay, and icon IDs. Restore creates parents and preceding siblings first
and maps old IDs to destination IDs through TeamSpeak callbacks.

## Safety and limitations

- Channel cleanup is destructive. It requires selecting the menu item twice on
  the same server within 15 seconds and always preserves the default channel.
- Restore creates channels; it does not overwrite existing ones. Clean the
  destination first if you want to avoid duplicates.
- Stored channel passwords cannot be read by a TeamSpeak client. Export writes
  `passwordProtected`, but `password` is `null`. Add a known password to the JSON
  manually before restore if required.
- Empty temporary channels cannot survive during a client-side restore, so they
  are restored as semi-permanent and reported at completion.
- Server groups are imported separately. Existing groups with the same name are
  skipped and client memberships are not assigned automatically.
- Channel groups, tokens, bans, virtual-server configuration, files, avatars,
  and client identities are outside the current backup format.
- Every operation is limited by the connected identity's TeamSpeak permissions.

Always keep a verified export and test migration on a non-production server
before deleting channels.

## Build

Prerequisites:

- A 64-bit TeamSpeak 3 client compatible with Plugin SDK v26
- GCC on Linux, MinGW-w64 or Visual Studio on Windows
- `curl` or `wget`, plus `tar`, to run the SDK setup script

Linux or a Linux environment with MinGW-w64 installed:

```bash
chmod +x setup_sdk.sh build.sh
./setup_sdk.sh
./build.sh
```

The script builds `bin/linux/massmover.so`. If MinGW-w64 is available, it also
builds `bin/windows/massmover.dll`. It never installs either file automatically.

Windows, after placing the official SDK in `ts3client-pluginsdk-26`:

```cmd
build_windows.bat
```

cJSON 1.7.19 is vendored under `cjson/`; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Install

1. Close TeamSpeak.
2. Copy only the plugin binary for your operating system:
   - Windows: `bin\windows\massmover.dll` to `%APPDATA%\TS3Client\plugins\`
   - Native Linux: `bin/linux/massmover.so` to `~/.ts3client/plugins/`
   - Flatpak Linux: `bin/linux/massmover.so` to
     `~/.var/app/com.teamspeak.TeamSpeak3/.ts3client/plugins/`
3. Start TeamSpeak and enable the plugin in its Addons/Plugins manager.

No SDK files, source files, JSON library files, or build directories need to be
copied into the TeamSpeak plugin directory.

## Recommended migration workflow

1. Connect to the source server and select `Export Server Structure`.
2. Keep a safe copy of the generated `Desktop/server.json`.
3. Put the required `servergroups.txt` on the Desktop if groups will be migrated.
4. Connect to the destination with an identity that has sufficient permissions.
5. If appropriate, select `Delete All Non-Default Channels` twice within 15
   seconds and wait for the completion message.
6. Select `Restore Server Structure` and wait for completion.
7. Select `Import Server Groups` and review skipped permissions/groups in the
   TeamSpeak client log.
8. Verify channel hierarchy, permissions, and access with a test identity.

Do not start another export, restore, import, or cleanup operation while one is
still active.

## Tests

The test suite uses mocked TeamSpeak SDK callbacks and never contacts a server:

```bash
mkdir -p build/tests
gcc -O0 -g -Wall -std=gnu99 -Its3client-pluginsdk-26/include -Icjson \
  tests/backup_restore_test.c cjson/cJSON.c -o build/tests/backup_restore_test
gcc -O0 -g -Wall -std=gnu99 -Its3client-pluginsdk-26/include -Icjson \
  tests/servergroups_file_test.c cjson/cJSON.c -o build/tests/servergroups_file_test
./build/tests/backup_restore_test
./build/tests/servergroups_file_test tests/fixtures/servergroups.txt
```

GitHub Actions runs the Linux build, Windows cross-build, mock integration test,
and group parser test for every pull request.

## Repository layout

```text
src/                 Plugin implementation and public header
cjson/               Vendored cJSON source
tests/               Mock integration tests and fixtures
.github/workflows/   Continuous integration
build.sh             Linux and optional Windows cross-build
build_windows.bat    Native Windows build
setup_sdk.sh         Official Plugin SDK v26 downloader
```

See [CHANGELOG.md](CHANGELOG.md) for release history and
[CONTRIBUTING.md](CONTRIBUTING.md) before submitting changes.

## License

This project is distributed under the [MIT License](LICENSE). Vendored cJSON
retains its upstream MIT notice.
