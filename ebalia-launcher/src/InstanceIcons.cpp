#include "InstanceIcons.hpp"
#include "Icons.hpp"
#include "Language.hpp"
#include "Ui.hpp"
#include "Archive.hpp"
#include <QtConcurrent>
#include <QFutureWatcher>
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
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
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
    // 16×16 pixel art, enlarged without blurring: Minecraft's own textures when they were read from the game,
    // the bundled pictures until then.
    static QHash<QString,QIcon> cache;static QString cachedFrom;
    if(cachedFrom!=textureFolder()){cache.clear();cachedFrom=textureFolder();}
    if(!cache.contains(key)){
        QImage art;if(!cachedFrom.isEmpty())art.load(cachedFrom+"/"+key+".png");
        if(art.isNull())art.load(":/icons/instances/"+key+".png");
        cache.insert(key,QPixmap::fromImage(art.scaled(128,128,Qt::IgnoreAspectRatio,Qt::FastTransformation)));
    }
    return cache.value(key);
}
namespace {
QString &textures(){static QString folder;return folder;}
// Textures of each icon inside the client jar: the 1.13+ path, then the older one. A rectangle crops a texture sheet.
struct Texture{const char *key;QStringList paths;QRect crop;};
const QList<Texture> &gameTextures(){
    static const QList<Texture> list{
        {"grass",{"block/grass_block_side.png","blocks/grass_side.png"},{}},
        {"creeper",{"entity/creeper/creeper.png"},QRect(8,8,8,8)}, // the face on the creeper's skin
        {"tnt",{"block/tnt_side.png","blocks/tnt_side.png"},{}},
        {"pickaxe",{"item/diamond_pickaxe.png","items/diamond_pickaxe.png"},{}},
        {"gem",{"item/diamond.png","items/diamond.png"},{}},
        {"crafting",{"block/crafting_table_front.png","blocks/crafting_table_front.png"},{}},
        {"book",{"item/enchanted_book.png","items/book_enchanted.png"},{}},
        {"globe",{"item/map.png","items/map_empty.png"},{}},
        {"server",{"block/command_block_front.png","blocks/command_block.png"},{}},
        {"star",{"item/nether_star.png","items/nether_star.png"},{}},
        {"horror",{"block/jack_o_lantern.png","blocks/pumpkin_face_on.png"},{}}};
    return list;
}
}
QString InstanceIcons::textureFolder(){return textures();}
bool InstanceIcons::extractGameTextures(const QString &jar,const QString &folder){
    QDir().mkpath(folder);int written=0;
    for(const auto &t:gameTextures()){
        QImage image;
        for(const auto &path:t.paths){try{image.loadFromData(Archive::readEntry(jar,"assets/minecraft/textures/"+path,1024*1024),"PNG");}catch(...){}if(!image.isNull())break;}
        if(image.isNull())continue;
        if(!t.crop.isNull()){const int scale=qMax(1,image.width()/64);image=image.copy(QRect(t.crop.topLeft()*scale,t.crop.size()*scale));}
        else if(image.height()>image.width())image=image.copy(0,0,image.width(),image.width()); // animated strips: the first frame
        if(image.save(folder+"/"+t.key+".png","PNG"))++written;
    }
    return written==gameTextures().size();
}
void InstanceIcons::useGameTextures(const QString &mcRoot,QObject *context,std::function<void()> ready){
    const auto folder=mcRoot+"/cache/instance-icons";
    if(QFile::exists(folder+"/.complete")){if(textures()!=folder){textures()=folder;if(ready)ready();}return;}
    // The newest client jar the launcher downloaded from Mojang.
    QString jar;QDateTime newest;
    for(const auto &v:QDir(mcRoot+"/versions").entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot)){const QFileInfo f(v.filePath()+"/"+v.fileName()+".jar");if(f.exists()&&f.size()>1000000&&(jar.isEmpty()||f.lastModified()>newest)){jar=f.filePath();newest=f.lastModified();}}
    if(jar.isEmpty())return;
    auto watcher=new QFutureWatcher<bool>(context);
    QObject::connect(watcher,&QFutureWatcher<bool>::finished,context,[watcher,folder,ready]{const bool ok=watcher->result();watcher->deleteLater();if(!ok)return;textures()=folder;if(ready)ready();});
    watcher->setFuture(QtConcurrent::run([jar,folder]{if(!extractGameTextures(jar,folder))return false;QFile done(folder+"/.complete");return done.open(QIODevice::WriteOnly);}));
}
QImage InstanceIcons::chooseImage(QWidget *parent,int size){
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
    return image.scaled(size,size,Qt::IgnoreAspectRatio,pixelArt?Qt::FastTransformation:Qt::SmoothTransformation);
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

QStringList InstanceIcons::backgrounds(){
    QStringList out;
    for(const auto &name:QDir(":/art").entryList({"f*.jpg"},QDir::Files,QDir::Name))out<<":/art/"+name;
    for(const auto &name:QDir(":/art/backgrounds").entryList({"*.png","*.jpg"},QDir::Files,QDir::Name))out<<":/art/backgrounds/"+name;
    return out;
}
QString InstanceIcons::background(const QString &dir){
    QFile f(dir+"/instance.json");QString key;
    if(f.open(QIODevice::ReadOnly))key=QJsonDocument::fromJson(f.readAll()).object()["background"].toString();
    if(key=="custom"&&QFileInfo::exists(dir+"/instance-background.jpg"))return dir+"/instance-background.jpg";
    if(key.startsWith(":/art/")&&backgrounds().contains(key))return key;
    return Ui::artFor(dir);
}
bool InstanceIcons::saveBackground(const QString &dir,const QImage &picture){
    if(dir.isEmpty()||picture.isNull())return false;
    QSaveFile out(dir+"/instance-background.jpg");
    return out.open(QIODevice::WriteOnly)&&picture.save(&out,"JPG",90)&&out.commit();
}
namespace {
QImage chooseBackgroundImage(QWidget *parent){
    const auto file=QFileDialog::getOpenFileName(parent,Language::text("Elegí una imagen de fondo","Choose a background picture","Escolha uma imagem de fundo"),QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),Language::key("Images")+" (*.png *.jpg *.jpeg *.webp *.bmp)");
    if(file.isEmpty())return {};
    QImageReader reader(file);reader.setAutoTransform(true);
    if(QFileInfo(file).size()>48*1024*1024||reader.size().width()>16384||reader.size().height()>16384){QMessageBox::warning(parent,"EBALIA",Language::text("Esta imagen es demasiado grande. Elegí una de menos de 48 MB.","This image is too large. Choose one under 48 MB.","Esta imagem é grande demais. Escolha uma com menos de 48 MB."));return {};}
    if(reader.size().width()>1920||reader.size().height()>1080)reader.setScaledSize(reader.size().scaled(1920,1080,Qt::KeepAspectRatio));
    auto image=reader.read();
    if(image.isNull())QMessageBox::warning(parent,"EBALIA",Language::key("This file is not an image the launcher can open."));
    return image.convertToFormat(QImage::Format_RGB32);
}
}
bool InstanceIcons::chooseBackground(QWidget *parent,const QString &dir,const QString &current,QString &key,QImage &custom){
    QDialog d(parent);d.setObjectName("instanceBackgroundWindow");d.setWindowTitle(Language::text("Fondo de la instancia","Instance background","Fundo da instância"));Ui::fitToScreen(&d,{760,560});
    QVBoxLayout lay(&d);lay.setContentsMargins(20,18,20,18);lay.setSpacing(12);
    auto hint=new QLabel(Language::text("Elegí una imagen del launcher o una de tu computadora. Se muestra en la tarjeta y en la página de la instancia.","Pick a launcher picture or one from your computer. It is shown on the instance card and page.","Escolha uma imagem do launcher ou do seu computador. Ela aparece no cartão e na página da instância."));hint->setWordWrap(true);hint->setObjectName("muted");lay.addWidget(hint);
    QListWidget list;list.setObjectName("backgroundList");list.setViewMode(QListView::IconMode);list.setIconSize({176,99});list.setGridSize({196,140});list.setResizeMode(QListView::Adjust);list.setMovement(QListView::Static);list.setWordWrap(true);lay.addWidget(&list,1);
    auto add=[&](const QString &title,const QString &value,const QString &image){
        auto item=new QListWidgetItem(title,&list);item->setData(Qt::UserRole,value);item->setSizeHint({188,132});item->setTextAlignment(Qt::AlignHCenter|Qt::AlignTop);
        Ui::loadArt(image,{352,198},&list,[item,&list](const QPixmap &p){if(list.row(item)>=0)item->setIcon(QIcon(Ui::cover(p,{176,99},8)));});
        if(value==current)list.setCurrentItem(item);
    };
    add(Language::text("Automático","Automatic","Automático"),{},Ui::artFor(dir));
    if(QFileInfo::exists(dir+"/instance-background.jpg"))add(Language::text("Tu imagen","Your picture","Sua imagem"),"custom",dir+"/instance-background.jpg");
    int art=0;for(const auto &resource:backgrounds()){auto title=QFileInfo(resource).completeBaseName();title.replace('-',' ');if(title.startsWith('f')&&title.contains('_'))title=Language::text("Arte EBALIA %1","EBALIA art %1","Arte EBALIA %1").arg(++art);else title[0]=title[0].toUpper();add(title,resource,resource);}
    if(!list.currentItem())list.setCurrentRow(0);
    auto row=new QHBoxLayout;lay.addLayout(row);
    auto computer=new QPushButton(Ui::icon("image",QColor(225,225,230))," "+Language::text("Elegir imagen de tu PC…","Choose a picture from your computer…","Escolher imagem do seu computador…"));computer->setObjectName("customInstanceBackground");computer->setCursor(Qt::PointingHandCursor);row->addWidget(computer);row->addStretch();
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);row->addWidget(&buttons);buttons.button(QDialogButtonBox::Ok)->setProperty("play",true);
    QObject::connect(&buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);QObject::connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
    QObject::connect(&list,&QListWidget::itemDoubleClicked,&d,&QDialog::accept);
    custom={};
    QObject::connect(computer,&QPushButton::clicked,&d,[&]{auto image=chooseBackgroundImage(&d);if(!image.isNull()){custom=image;key="custom";d.done(2);}});
    const int result=Ui::openWindow(d);
    if(result==2)return true;
    if(result!=QDialog::Accepted||!list.currentItem())return false;
    key=list.currentItem()->data(Qt::UserRole).toString();return true;
}
