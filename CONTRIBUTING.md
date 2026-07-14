# Contributing

Contributions are welcome through focused pull requests.

## Development workflow

1. Create a branch from the latest `master`.
2. Keep the existing C style and TeamSpeak Plugin SDK v26 compatibility.
3. Do not commit the downloaded SDK, generated binaries, build output, or real
   server backup/import files.
4. Run `./setup_sdk.sh`, `./build.sh`, and both tests documented in the README.
5. Update the README and changelog when behavior visible to users changes.
6. Open a pull request describing the change, its safety implications, and how
   it was tested.

## TeamSpeak SDK rules

- Use only functions, enums, and properties present in the official SDK headers.
- Keep all operations compatible with a closed ServerQuery port.
- Check SDK return codes and preserve asynchronous callback state.
- Never weaken the confirmation or default-channel protection used by channel
  cleanup.

Keep feature changes separate from formatting or build-system maintenance where
possible so reviews remain straightforward.
