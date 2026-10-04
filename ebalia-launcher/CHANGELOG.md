# 1.1.0

- Added an experimental external Bedrock manager with background installation. Windows uses BedrockLauncher; Linux/macOS use the Android backend and require Google Play ownership. Windows ownership does not transfer to Android. Real gameplay remains unverified across platforms.
- Added Lost Version uninstall with confirmation and move-to-trash behavior.
- Moved operation progress to the bottom status bar.
- Bundled the Windows Visual C++ runtime and added a desktop shortcut to the per-user installer.
- Preserved the existing application data location and update checker.

The launcher does not include Minecraft, accounts, or game licenses. Windows/macOS packages are unsigned. Linux binary targets Ubuntu 24.04 x64; other distributions can build from source, including NixOS via the flake.
