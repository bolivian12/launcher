# Bedrock integration — experimental in 1.1.0

Windows uses BedrockLauncher with a Minecraft for Windows license / applicable Game Pass subscription. Linux and macOS use an Android backend requiring Google Play ownership. A Windows purchase does not unlock Android downloads. Microsoft/Xbox multiplayer sign-in is separate from downloading the game.

The install button installs the external manager and supported dependencies, then opens that manager for sign-in and game setup. It does not bundle Minecraft or guarantee the latest game release works. Native Windows/macOS game sessions and a real Linux game session have not been validated.

## Automatic setup added
The Install Bedrock button runs setup in a worker thread with an inline status bar.
Linux installs the user Flatpak and dependencies from Flathub. Tested successfully here:
io.mrarm.mcpelauncher v1.8.4, runtime org.kde.Platform 6.10.
NixOS still requires system-level Flatpak support; the app explains missing prerequisites rather than editing NixOS configuration.
Windows downloads the latest upstream stable ZIP and uses winget for .NET Desktop Runtime 8 if needed. Developer Mode may require user action.
macOS copies the latest upstream DMG's application to the lab's data directory; OS security approval remains in effect.
Windows/macOS installation paths have NOT been executed on their native OS.
The full game requires account sign-in in the installed provider; it is not bundled.
No one-click full-game/no-account compatibility is claimed.

Lost Version uninstall and nonmodal operation progress also included. Uninstall moves files to the trash after explicit confirmation and does not remove shared Java/archive caches.

