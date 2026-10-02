#pragma once
#include <QStringList>
namespace Loaders {
QStringList versions(const QString &loader,const QString &game);
QString installerUrl(const QString &loader,const QString &game,const QString &version);
}
