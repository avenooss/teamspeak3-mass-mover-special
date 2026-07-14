# Changelog

All notable changes to this project are documented here. This project follows
[Semantic Versioning](https://semver.org/).

## [1.5.0] - 2026-07-14

### Added

- Channel-structure export to a pretty-printed `Desktop/server.json` file.
- Asynchronous channel restore using TeamSpeak Plugin SDK callbacks.
- Server-group and permission import from `Desktop/servergroups.txt`.
- Confirmed, bottom-up deletion of all non-default channels.
- Vendored cJSON 1.7.19 for JSON serialization and parsing.
- Local mock-SDK integration and server-group parser tests.

### Changed

- Plugin version updated from 1.0.1 to 1.5.0.

## [1.0.1] - 2025-05-24

- Original MassMove plugin release.

[1.5.0]: https://github.com/avenooss/teamspeak3-mass-mover-special/compare/1.0.1...master
[1.0.1]: https://github.com/SirProdigle/teamspeak3-mass-mover/releases/tag/1.0.1
