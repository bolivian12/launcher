#pragma once
#include <QIcon>
#include <QStringList>
// Pixel-art instance icons drawn in code, plus the icon a modpack provides (instance-icon.png).
namespace InstanceIcons {
QStringList keys();
QIcon icon(const QString &key,const QString &instanceDir={});
QIcon provider(const QString &provider);
}
