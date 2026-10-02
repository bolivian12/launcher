#pragma once
#include "VersionManager.hpp"
#include <functional>
namespace LostInstaller {void install(const VersionInfo &version,const QString &destination,std::function<void(qint64,qint64)> progress={});}
