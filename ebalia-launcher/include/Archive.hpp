#pragma once
#include <QString>
#include <QStringList>
namespace Archive {
void extract(const QString &file, const QString &destination, const QString &prefix = {});
// Extracts only the entries named in names (paths inside the archive, ZIP or 7z) as files directly in destination.
void extractFiles(const QString &file, const QString &destination, const QStringList &names);
// Content of one entry (ZIP or 7z), at most limit bytes; empty when the archive has no such entry.
QByteArray readEntry(const QString &file, const QString &name, qint64 limit);
// Writes folder into a ZIP, leaving out top-level entries listed in skip.
void compress(const QString &folder, const QString &file, const QStringList &skip = {});
}
