#include "InstanceIcons.hpp"
#include "Icons.hpp"
#include <QFile>
#include <QHash>
#include <QPainter>
namespace {
QPixmap pixels(const QStringList &rows,const QHash<QChar,QRgb> &palette,int size){
    QImage image(rows.first().size(),rows.size(),QImage::Format_ARGB32);image.fill(Qt::transparent);
    for(int y=0;y<rows.size();++y)for(int x=0;x<rows[y].size();++x)if(rows[y][x]!='.')image.setPixel(x,y,palette.value(rows[y][x],qRgb(255,0,255)));
    return QPixmap::fromImage(image).scaled(size,size,Qt::IgnoreAspectRatio,Qt::FastTransformation);
}
QPixmap creeper(int size){return pixels({"GGgGGGgG","GgGGGGGG","GKKGgKKG","GKKGGKKG","gGGKKGGG","GGKKKKGg","GGKKKKGG","GgKGGKGG"},{{'G',qRgb(94,170,72)},{'g',qRgb(70,140,55)},{'K',qRgb(20,28,20)}},size);}
QPixmap tnt(int size){return pixels({"RRRRRRRR","RrRRrRRr","WWWWWWWW","WKWKWKWW","WWWWWWWW","RRrRRRrR","RRRRRRRR","rRRrRRRR"},{{'R',qRgb(204,58,44)},{'r',qRgb(160,40,32)},{'W',qRgb(236,236,236)},{'K',qRgb(40,40,40)}},size);}
QPixmap pickaxe(int size){return pixels({".CCCCC..","C.....C.","......CS",".....S..","....S...","...S....","..S.....",".S......"},{{'C',qRgb(90,210,220)},{'S',qRgb(140,100,60)}},size);}
}
QStringList InstanceIcons::keys(){return {"grass","creeper","tnt","pickaxe","gem","crafting","book","globe","server","star","horror","ebalia"};}
QIcon InstanceIcons::icon(const QString &value,const QString &dir){
    auto key=value.toLower();
    if(key=="custom"&&!dir.isEmpty()&&QFile::exists(dir+"/instance-icon.png"))return QIcon(dir+"/instance-icon.png");
    // Keys written by the previous creation dialog.
    if(key=="world")key="globe";else if(key=="mods")key="crafting";else if(key=="adventure")key="book";
    const int s=64;
    if(key=="creeper")return creeper(s);if(key=="tnt")return tnt(s);if(key=="pickaxe")return pickaxe(s);
    if(key=="gem")return icons::release(s);if(key=="crafting")return icons::mods(s);if(key=="book")return icons::learn(s);
    if(key=="globe")return icons::wiki(s);if(key=="server")return icons::server(s);if(key=="star")return icons::fanart(s);
    if(key=="horror")return icons::horror(s);if(key=="ebalia")return QIcon(":/icon.png");
    return icons::alpha(s);
}
QIcon InstanceIcons::provider(const QString &provider){
    // Each service's own logo (resources/providers/NOTICE.txt); custom and import use EBALIA's pixel icons.
    if(provider=="custom")return icons::alpha(48);if(provider=="import")return icons::folder(48);
    const bool app=provider=="import_ftb";QPixmap logo(":/providers/"+QString(app?"ftb":provider)+".png");
    if(logo.isNull())return icons::alpha(48);
    QPixmap pixmap(96,96);pixmap.fill(Qt::transparent);QPainter p(&pixmap);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);
    const auto scaled=logo.scaled(app?80:96,app?80:96,Qt::KeepAspectRatio,Qt::SmoothTransformation);p.drawPixmap((96-scaled.width())/2,(96-scaled.height())/2,scaled);
    if(app){ // the FTB App import: FTB logo with a download badge
        p.setPen(Qt::NoPen);p.setBrush(QColor(60,133,39));p.drawEllipse(QRectF(58,58,36,36));p.setPen(QPen(Qt::white,5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        p.drawLine(QPointF(76,66),QPointF(76,84));p.drawLine(QPointF(68,77),QPointF(76,85));p.drawLine(QPointF(84,77),QPointF(76,85));
    }
    return pixmap;
}
