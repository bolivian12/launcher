#pragma once
#include <QIcon>
#include <QImage>
#include <QStringList>
#include <functional>
class QMenu;
class QWidget;
// Pixel-art instance icons (resources/icons/instances), plus a custom picture
// stored as instance-icon.png, which modpacks also provide.
namespace InstanceIcons {
QStringList keys();
QString name(const QString &key);
QIcon icon(const QString &key,const QString &instanceDir={});
QIcon provider(const QString &provider);
// Built-in icons and "Choose an image from your computer…". The callback gets
// the key, and for "custom" the picture to save with saveCustom().
QMenu *menu(QWidget *parent,std::function<void(const QString &key,const QImage &custom)> chosen);
QImage chooseImage(QWidget *parent);
bool saveCustom(const QString &instanceDir,const QImage &picture);
// Card and page background: a launcher picture (key ":/art/…"), a picture from the computer ("custom",
// stored as instance-background.jpg) or, with an empty key, one picked from the instance folder name.
QStringList backgrounds();
QString background(const QString &instanceDir);
// Every launcher picture plus "Choose an image from your computer…"; false when cancelled.
bool chooseBackground(QWidget *parent,const QString &instanceDir,const QString &current,QString &key,QImage &custom);
bool saveBackground(const QString &instanceDir,const QImage &picture);
}
