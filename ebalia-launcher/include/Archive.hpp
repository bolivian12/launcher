#pragma once
#include <QString>
#include <QStringList>
namespace Archive {
void extract(const QString &file, const QString &destination, const QString &prefix = {});
// Writes folder into a ZIP, leaving out top-level entries listed in skip.
void compress(const QString &folder, const QString &file, const QStringList &skip = {});
}
