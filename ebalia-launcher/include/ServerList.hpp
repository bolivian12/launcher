#pragma once
#include <QImage>
#include <QList>
#include <QString>
// The multiplayer list of an instance (servers.dat, uncompressed NBT). Fields the
// launcher does not know are kept as they are when the list is rewritten.
namespace ServerList {
struct Server{QString name,address;QImage icon;};
QList<Server> read(const QString &file);
// Both throw std::runtime_error when the file cannot be read or written.
void add(const QString &file,const QString &name,const QString &address);
void remove(const QString &file,int index);
}
