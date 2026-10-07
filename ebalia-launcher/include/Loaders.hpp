#pragma once
#include <QStringList>
namespace Loaders {
QStringList versions(const QString &loader,const QString &game);
QString installerUrl(const QString &loader,const QString &game,const QString &version);
// Start of the NeoForge versions made for a Minecraft version ("21.1." for 1.21.1, "26.3.0." for 26.3).
QString neoforgePrefix(const QString &game);
// Newest first; stable NeoForge builds before betas.
void sort(const QString &loader,const QString &game,QStringList &versions);
}
