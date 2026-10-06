# Sin publicar

- Microsoft sign-in is enabled with the EBALIA public application ID (`microsoft-client-id.txt`).
- Microsoft accounts now show their profile picture (the Xbox gamerpic of the account; the face of the Minecraft skin when there is none) in the sidebar, the Home player card, the account menu and Settings. Pictures are stored in `mc/avatars`.
- Silence, the EBALIA mascot, accompanies the interactive tour and the Guide & tutorial page, animated: floating, breathing, blinking, glowing eyes, a swaying knife, a hop on every step and a lean toward the highlighted control.
- Portable mode: a `portable.txt` next to the launcher (next to the AppImage or the `.app` bundle) keeps instances, accounts and settings in an `ebalia-data` folder beside it.
- Silence now has a pixel-art sprite sheet with seven poses (idle, happy, surprised, wink, pointing, cheering, thinking), each with talking and blinking frames; every tour step uses its own pose.
- New hand-drawn 16×16 instance icons. Any picture from your computer can be used as an instance icon, when creating the instance or later from its page.
- The instance page shows its screenshots (click to open) and its multiplayer server list, where servers can be added, copied and removed; unknown `servers.dat` fields are preserved.
- Translation pass: the interactive tour, Patreon and creations pages, Lost Versions uninstall, Microsoft sign-in messages and the launcher updater are translated into all ten languages.
- Website: installer and portable downloads side by side, SHA-256 of every package with a copy button and the command to verify it, and a reworked phone layout where every link stays visible.
- New packages: Linux AppImage for any glibc 2.35+ distribution (Qt, libarchive and OpenSSL bundled; tested on Debian, Fedora, Arch and openSUSE in CI) with in-app updates, portable Windows `.zip`, Linux portable `.tar.gz`, and macOS Intel builds alongside Apple Silicon, each with a portable `.zip`.

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
