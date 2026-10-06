#include "InstanceIcons.hpp"
#include "Icons.hpp"
#include "Language.hpp"
#include "Ui.hpp"
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QMenu>
#include <QMessageBox>
#include <QPixmapCache>
#include <QSaveFile>
#include <QStandardPaths>
#include <QFile>
#include <QHash>
#include <QPainter>
QStringList InstanceIcons::keys(){return {"grass","creeper","tnt","pickaxe","gem","crafting","book","globe","server","star","horror","ebalia"};}
QString InstanceIcons::name(const QString &key){
    static const QHash<QString,const char*> names{{"grass","Grass block"},{"creeper","Creeper"},{"tnt","TNT"},{"pickaxe","Pickaxe"},{"gem","Diamond"},{"crafting","Crafting table"},
        {"book","Book"},{"globe","World"},{"server","Server"},{"star","Star"},{"horror","Eyes in the dark"},{"ebalia","EBALIA"}};
    return Language::key(QString::fromUtf8(names.value(key,"Custom")));
}
QIcon InstanceIcons::icon(const QString &value,const QString &dir){
    auto key=value.toLower();
    if(key=="custom"&&!dir.isEmpty()&&QFile::exists(dir+"/instance-icon.png"))return QIcon(dir+"/instance-icon.png");
    // Keys written by the previous creation dialog.
    if(key=="world")key="globe";else if(key=="mods")key="crafting";else if(key=="adventure")key="book";
    if(key=="ebalia")return QIcon(":/icon.png");
    if(!keys().contains(key))key="grass";
    // 16×16 pixel art, enlarged without blurring.
    static QHash<QString,QIcon> cache;
    if(!cache.contains(key)){QImage art(":/icons/instances/"+key+".png");cache.insert(key,QPixmap::fromImage(art.scaled(128,128,Qt::IgnoreAspectRatio,Qt::FastTransformation)));}
    return cache.value(key);
}
QImage InstanceIcons::chooseImage(QWidget *parent){
    const auto file=QFileDialog::getOpenFileName(parent,Language::key("Choose an icon"),QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),Language::key("Images")+" (*.png *.jpg *.jpeg *.webp *.gif *.bmp *.ico)");
    if(file.isEmpty())return {};
    QImageReader reader(file);reader.setAutoTransform(true);
    if(QFileInfo(file).size()>16*1024*1024||reader.size().width()>8192||reader.size().height()>8192){QMessageBox::warning(parent,"EBALIA",Language::key("This image is too large. Choose one under 16 MB."));return {};}
    QImage image=reader.read();
    if(image.isNull()){QMessageBox::warning(parent,"EBALIA",Language::key("This file is not an image the launcher can open."));return {};}
    // Centered square, so wide pictures are not squashed.
    const int side=qMin(image.width(),image.height());
    image=image.copy((image.width()-side)/2,(image.height()-side)/2,side,side).convertToFormat(QImage::Format_ARGB32);
    const bool pixelArt=side<=64;
    return image.scaled(128,128,Qt::IgnoreAspectRatio,pixelArt?Qt::FastTransformation:Qt::SmoothTransformation);
}
bool InstanceIcons::saveCustom(const QString &dir,const QImage &picture){
    if(dir.isEmpty()||picture.isNull())return false;
    QSaveFile out(dir+"/instance-icon.png");
    if(!out.open(QIODevice::WriteOnly)||!picture.save(&out,"PNG")||!out.commit())return false;
    QPixmapCache::clear(); // QIcon caches files by name
    return true;
}
QMenu *InstanceIcons::menu(QWidget *parent,std::function<void(const QString &,const QImage &)> chosen){
    auto menu=new QMenu(parent);menu->setObjectName("instanceIconMenu");
    for(const auto &key:keys()){auto action=menu->addAction(icon(key),name(key));QObject::connect(action,&QAction::triggered,parent,[key,chosen]{chosen(key,{});});}
    menu->addSeparator();
    auto custom=menu->addAction(Ui::icon("image",QColor(200,200,206)),Language::key("Choose an image from your computer…"));custom->setObjectName("customInstanceIcon");
    QObject::connect(custom,&QAction::triggered,parent,[parent,chosen]{auto image=chooseImage(parent);if(!image.isNull())chosen("custom",image);});
    return menu;
}
QIcon InstanceIcons::provider(const QString &provider){
    if(provider=="custom")return icons::alpha(48);if(provider=="import")return icons::folder(48);
    static const QHash<QString,QString> artwork{{"atlauncher","atlauncher"},{"curseforge","curseforge"},{"ftb","ftb"},
        {"legacy_ftb","ftb-legacy"},{"import_ftb","ftb"},{"modrinth","modrinth"},{"technic","technic"}};
    auto name=artwork.value(provider);if(name.isEmpty())return icons::mods(48);
    QIcon logo(":/icons/providers/"+name+".png");
    if(provider!="import_ftb")return logo;
    // Preserve the FTB mark and distinguish importing an existing FTB App instance.
    auto pixmap=logo.pixmap(128,128);QPainter p(&pixmap);p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(255,255,255));p.setPen(Qt::NoPen);p.drawEllipse(QRectF(84,84,40,40));
    p.setPen(QPen(QColor(60,130,190),6,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));p.drawLine(104,92,104,115);p.drawLine(94,105,104,115);p.drawLine(114,105,104,115);
    p.end();return pixmap;
}
