#include "Archive.hpp"
#include <archive.h>
#include <archive_entry.h>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <memory>
#include <stdexcept>
void Archive::extract(const QString &file, const QString &destination, const QString &prefix) {
    auto fail = [](const QString &s) { throw std::runtime_error(s.toStdString()); };
    std::unique_ptr<archive, decltype(&archive_read_free)> a(archive_read_new(), archive_read_free);
    archive_read_support_filter_all(a.get()); archive_read_support_format_zip(a.get());
    if (archive_read_open_filename(a.get(), QFile::encodeName(file).constData(), 65536) != ARCHIVE_OK) fail("No se pudo abrir el ZIP.");
    QDir().mkpath(destination); archive_entry *entry = nullptr; qint64 total = 0; int status;
    while ((status = archive_read_next_header(a.get(), &entry)) == ARCHIVE_OK) {
        auto name = QString::fromUtf8(archive_entry_pathname(entry)); name.replace('\\','/');
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
