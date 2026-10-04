#pragma once
#include <QString>
namespace BedrockInstaller {
// Runs on a worker thread. Returns the installed provider path, or "flatpak".
QString install(const QString &root);
}
