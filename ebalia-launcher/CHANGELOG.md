# 1.1.0

- Added an experimental external Bedrock manager with background installation. Windows uses BedrockLauncher; Linux/macOS use the Android backend and require Google Play ownership. Windows ownership does not transfer to Android. Real gameplay remains unverified across platforms.
- Added Lost Version uninstall with confirmation and move-to-trash behavior.
- Moved operation progress to the bottom status bar.
- Bundled the Windows Visual C++ runtime and added a desktop shortcut to the per-user installer.
- Added Update and restart inside the launcher. Release packages are verified with SHA-256, staged and startup-tested before replacing installed program files. Previous launcher files and temporary downloads are removed after successful startup. NixOS builds the release with Nix; shared store paths remain subject to Nix garbage collection.
- The updater never removes instances, Lost Versions, worlds, mods or downloaded user content. Updates are blocked while games or installations are active. Failed replacement restores the previous program files.
- Windows setup detects the existing installation directory, including the previous installer registry format, and updates it in place.
- Added a 14-step first-run interactive tour, with highlighted controls, Back/Next/Skip, and replay from Guide & tutorial.
- Existing 1.0.0 users need to install 1.1.0 once to obtain the in-app updater; future compatible releases update from within the launcher.
- Preserved the existing application data location.

The launcher does not include Minecraft, accounts, or game licenses. Windows/macOS packages are unsigned. Linux binary targets Ubuntu 24.04 x64; other distributions can build from source, including NixOS via the flake.
