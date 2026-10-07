#include "Archive.hpp"
#include <archive.h>
#include <archive_entry.h>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QDirIterator>
#include <QDateTime>
#include <memory>
#include <stdexcept>
#ifndef Q_OS_WIN
#include <locale.h>
#ifdef Q_OS_MACOS
#include <xlocale.h>
#endif
#endif
namespace {
// libarchive converts entry names to the C library locale and drops those it cannot convert. A UTF-8 locale for this
// thread only keeps every name when the program runs under "C" or "POSIX" (minimal Linux installs, launched from a service).
// On Windows libarchive converts to UTF-16 itself.
struct Utf8Names {
#ifndef Q_OS_WIN
    locale_t made = locale_t(0), previous = locale_t(0);
    Utf8Names() { for (auto name : {"C.UTF-8", "C.utf8", "UTF-8", "en_US.UTF-8"}) if ((made = newlocale(LC_CTYPE_MASK, name, locale_t(0)))) break; if (made) previous = uselocale(made); }
    ~Utf8Names() { if (made) { uselocale(previous); freelocale(made); } }
#endif
};
// Paths given to libarchive: UTF-16 on Windows (the ANSI code page cannot hold every name), the file system encoding elsewhere.
int openRead(archive *a, const QString &file) {
#ifdef Q_OS_WIN
    return archive_read_open_filename_w(a, reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(file).utf16()), 65536);
#else
    return archive_read_open_filename(a, QFile::encodeName(file).constData(), 65536);
#endif
}
int openWrite(archive *a, const QString &file) {
#ifdef Q_OS_WIN
    return archive_write_open_filename_w(a, reinterpret_cast<const wchar_t *>(QDir::toNativeSeparators(file).utf16()));
#else
    return archive_write_open_filename(a, QFile::encodeName(file).constData());
#endif
}
// Entry names as UTF-8 whatever the C library locale is (Windows and minimal systems run without a UTF-8 locale).
QString entryName(archive_entry *entry) {
    if (auto utf8 = archive_entry_pathname_utf8(entry)) return QString::fromUtf8(utf8);
    if (auto wide = archive_entry_pathname_w(entry)) return QString::fromWCharArray(wide);
    if (auto raw = archive_entry_pathname(entry)) return QString::fromLocal8Bit(raw);
    return {};
}
}
void Archive::extract(const QString &file, const QString &destination, const QString &prefix) {
    auto fail = [](const QString &s) { throw std::runtime_error(s.toStdString()); };
    Utf8Names utf8;
    std::unique_ptr<archive, decltype(&archive_read_free)> a(archive_read_new(), archive_read_free);
    archive_read_support_filter_all(a.get()); archive_read_support_format_zip(a.get());
    if (openRead(a.get(), file) != ARCHIVE_OK) fail("No se pudo abrir el ZIP.");
    QDir().mkpath(destination); archive_entry *entry = nullptr; qint64 total = 0; int status;
    // ARCHIVE_WARN: a name could not be converted to the locale's encoding, entryName() reads it as UTF-8.
    while ((status = archive_read_next_header(a.get(), &entry)) == ARCHIVE_OK || status == ARCHIVE_WARN) {
        auto name = entryName(entry); name.replace('\\','/');
        if (name.isEmpty()) fail("Nombre ilegible dentro del ZIP.");
        if(!prefix.isEmpty()){auto base=prefix;while(base.endsWith('/'))base.chop(1);if(!name.startsWith(base+"/")){archive_read_data_skip(a.get());continue;}name=name.mid(base.size()+1);if(name.isEmpty())continue;}
        auto parts = name.split('/');
        if (QDir::isAbsolutePath(name) || parts.contains("..") || name.contains(':') || archive_entry_symlink(entry) || archive_entry_hardlink(entry)) fail("Ruta insegura dentro del ZIP: " + name);
        const auto type = archive_entry_filetype(entry);
        if (type != AE_IFDIR && type != AE_IFREG) fail("Tipo de archivo no admitido en ZIP.");
        const auto dest = QDir(destination).filePath(name);
        QString parent = destination;
        for (const auto &part : parts) { parent = QDir(parent).filePath(part); if (QFileInfo(parent).isSymLink()) fail("Destino con enlace simbólico."); }
        if (type == AE_IFDIR) { QDir().mkpath(dest); continue; }
        QDir().mkpath(QFileInfo(dest).absolutePath()); QSaveFile out(dest);
        if (!out.open(QIODevice::WriteOnly)) fail("No se puede extraer " + name);
        char buffer[65536]; la_ssize_t size;
        while ((size = archive_read_data(a.get(),buffer,sizeof(buffer))) > 0) {
            total += size; if (total > 8LL*1024*1024*1024) fail("ZIP excede 8 GB descomprimidos.");
            if (out.write(buffer,size) != size) fail("Sin espacio al extraer " + name);
        }
        if (size < 0 || !out.commit()) fail("Archivo ZIP incompleto: " + name);
    }
    if (status != ARCHIVE_EOF) fail("ZIP dañado.");
}
void Archive::extractFiles(const QString &file, const QString &destination, const QStringList &names) {
    auto fail = [](const QString &s) { throw std::runtime_error(s.toStdString()); };
    Utf8Names utf8;
    std::unique_ptr<archive, decltype(&archive_read_free)> a(archive_read_new(), archive_read_free);
    archive_read_support_filter_all(a.get()); archive_read_support_format_zip(a.get()); archive_read_support_format_7zip(a.get());
    if (openRead(a.get(), file) != ARCHIVE_OK) fail("No se pudo abrir el archivo comprimido.");
    QDir().mkpath(destination); archive_entry *entry = nullptr; int status; int found = 0;
    while ((status = archive_read_next_header(a.get(), &entry)) == ARCHIVE_OK || status == ARCHIVE_WARN) {
        auto name = entryName(entry); name.replace('\\','/');
        if (!names.contains(name) || archive_entry_filetype(entry) != AE_IFREG) { archive_read_data_skip(a.get()); continue; }
        QSaveFile out(QDir(destination).filePath(QFileInfo(name).fileName()));
        if (!out.open(QIODevice::WriteOnly)) fail("No se puede extraer " + name);
        char buffer[65536]; la_ssize_t size;
        while ((size = archive_read_data(a.get(), buffer, sizeof(buffer))) > 0) if (out.write(buffer, size) != size) fail("Sin espacio al extraer " + name);
        if (size < 0 || !out.commit()) fail("Archivo incompleto: " + name);
        if (++found == names.size()) return;
    }
    if (status != ARCHIVE_EOF) fail("Archivo comprimido dañado.");
}
void Archive::compress(const QString &folder, const QString &file, const QStringList &skip) {
    auto fail = [](const QString &s) { throw std::runtime_error(s.toStdString()); };
    Utf8Names utf8;
    const auto partial = file + ".part"; QFile::remove(partial);
    std::unique_ptr<archive, decltype(&archive_write_free)> a(archive_write_new(), archive_write_free);
    archive_write_set_format_zip(a.get()); archive_write_set_options(a.get(), "zip:hdrcharset=UTF-8");
    if (openWrite(a.get(), partial) != ARCHIVE_OK) fail("No se pudo crear el ZIP.");
    QDirIterator it(folder, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const auto path = it.next(); const auto info = it.fileInfo(); if (info.isSymLink()) continue;
        const auto name = QDir(folder).relativeFilePath(path); if (skip.contains(name.section('/', 0, 0))) continue;
        QFile in(path); if (!in.open(QIODevice::ReadOnly)) fail("No se pudo leer " + name);
        std::unique_ptr<archive_entry, decltype(&archive_entry_free)> entry(archive_entry_new(), archive_entry_free);
        archive_entry_set_pathname_utf8(entry.get(), name.toUtf8().constData()); archive_entry_set_filetype(entry.get(), AE_IFREG);
        archive_entry_set_perm(entry.get(), 0644); archive_entry_set_size(entry.get(), in.size()); archive_entry_set_mtime(entry.get(), info.lastModified().toSecsSinceEpoch(), 0);
        if (archive_write_header(a.get(), entry.get()) != ARCHIVE_OK) fail("No se pudo escribir " + name);
        while (!in.atEnd()) { auto data = in.read(1 << 20); if (archive_write_data(a.get(), data.constData(), size_t(data.size())) != data.size()) fail("Sin espacio al comprimir " + name); }
    }
    if (archive_write_close(a.get()) != ARCHIVE_OK) fail("No se pudo completar el ZIP.");
    QFile::remove(file); if (!QFile::rename(partial, file)) fail("No se pudo guardar el ZIP.");
}
