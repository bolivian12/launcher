#pragma once
#include "VersionManager.hpp"
#include <functional>
namespace LostInstaller {
void install(const VersionInfo &version,const QString &destination,std::function<void(qint64,qint64)> progress={});
// .exe/.bat/.cmd packages: Windows only, or Wine elsewhere.
bool windowsPackage(const VersionInfo &version);
// Java 8 for archive packages: a detected Java 8, or Mojang's official jre-legacy. Under Wine this is the Windows runtime.
QString java(const QString &dataDir,bool windowsPackage,std::function<void(int,int)> progress={});
QString winePrefix(const QString &dataDir);
QString wine(); // wine, or wine64 on distributions that only ship the 64-bit loader
}
