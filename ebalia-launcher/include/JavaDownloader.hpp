#pragma once
#include "ModRepository.hpp"
#include <QString>
#include <functional>
// Official Java runtimes published by Mojang for the Minecraft Launcher (jre-legacy = Java 8, java-runtime-delta = Java 21, ...).
namespace JavaDownloader {
QString platform();
QString component(int major);
// Installs into root/<component>-<platform> through a staging folder and returns the java executable.
QString install(const QString &root,const QString &component,QString platform={},std::function<void(int,int)> progress={},ModRepository::Transport transport={});
QString installed(const QString &root,const QString &component,QString platform={});
}
