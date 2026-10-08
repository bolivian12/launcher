#pragma once
#include <QString>
namespace BedrockInstaller {
// Runs on a worker thread. Returns the installed provider path, or "flatpak".
QString install(const QString &root);
// Windows: BedrockLauncher refuses to start without the Visual C++ 2015-2022 x64 runtime (14.14.26405 or later)
// registered system-wide. True elsewhere.
bool vcRuntimeReady();
// Downloads Microsoft's installer and runs it; Windows asks for administrator permission. Runs on a worker thread.
void installVcRuntime(const QString &root);
}
