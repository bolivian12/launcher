#include "SkinManager.hpp"
#include "ModRepository.hpp"
#include <QImageReader>
#include <QPainter>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QEventLoop>
#include <QTimer>
#include <QBuffer>
#include <archive.h>
#include <archive_entry.h>
#include <memory>
#include <stdexcept>
namespace {
[[noreturn]] void fail(const QString &s){throw std::runtime_error(s.toStdString());}
QImage skinImage(const QString &file){
    if(QFileInfo(file).size()>1024*1024)fail("Skin PNG exceeds 1 MB.");
    QImageReader r(file,"png");auto size=r.size();
    if(size!=QSize(64,64)&&size!=QSize(64,32))fail("Skin must be a 64 × 64 or 64 × 32 PNG.");
    auto image=r.read();if(image.isNull())fail("Could not decode skin PNG.");return image.convertToFormat(QImage::Format_RGBA8888);
}
QByteArray png(const QImage &image){QByteArray out;QBuffer b(&out);b.open(QIODevice::WriteOnly);if(!image.save(&b,"PNG"))fail("Could not encode skin.");return out;}
void save(const QString &path,const QByteArray &data){QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit())fail("Could not save skin data.");}
QJsonArray packList(const QByteArray &options){for(const auto &line:options.split('\n'))if(line.startsWith("resourcePacks:"))return QJsonDocument::fromJson(line.mid(14)).array();return {};}
QByteArray optionsWith(QByteArray options,bool enabled){
    auto packs=packList(options);for(int i=packs.size()-1;i>=0;--i)if(packs[i].toString()=="file/ebalia-local-skin.zip")packs.removeAt(i);
    if(enabled)packs.append("file/ebalia-local-skin.zip");auto replacement="resourcePacks:"+QJsonDocument(packs).toJson(QJsonDocument::Compact);
    auto lines=options.split('\n');bool found=false;for(auto &line:lines)if(line.startsWith("resourcePacks:")){line=replacement;found=true;}
    if(!found)lines.append(replacement);return lines.join('\n');
}
QByteArray options(const QString &dir){QFile f(dir+"/options.txt");if(!f.exists())return {};if(!f.open(QIODevice::ReadOnly))fail("Could not read instance options.");return f.readAll();}
QImage modelTexture(const QImage &src,bool sourceSlim,bool targetSlim){
    if(sourceSlim==targetSlim)return src;
    QImage out=src;QPainter p(&out);int sw=sourceSlim?3:4,dw=targetSlim?3:4;
    for(auto base:{QPoint(40,16),QPoint(32,48),QPoint(40,32),QPoint(48,48)}){
        p.setCompositionMode(QPainter::CompositionMode_Source);p.fillRect(QRect(base,QSize(16,16)),Qt::transparent);
        auto piece=[&](int sx,int sy,int w,int h,int dx,int dy,int width){p.drawImage(QRect(base.x()+dx,base.y()+dy,width,h),src,QRect(base.x()+sx,base.y()+sy,w,h));};
        piece(4,0,sw,4,4,0,dw);piece(4+sw,0,sw,4,4+dw,0,dw);
        piece(0,4,4,12,0,4,4);piece(4,4,sw,12,4,4,dw);piece(4+sw,4,4,12,4+dw,4,4);piece(8+sw,4,sw,12,8+dw,4,dw);
    }return out;
}
}
QJsonObject SkinManager::importSkin(const QString &file,const QString &name,const QString &variant){
    if(name.trimmed().isEmpty()||(variant!="classic"&&variant!="slim"))fail("Invalid skin name or model.");auto image=skinImage(file);
    if(image.height()==32&&variant=="slim")fail("Legacy 64 × 32 skins require the Classic model.");
    auto data=png(image);auto id=QString::fromLatin1(QCryptographicHash::hash(data+variant.toUtf8(),QCryptographicHash::Sha256).toHex());
    QDir().mkpath(m_root+"/skins");auto path=m_root+"/skins/"+id+".png";save(path,data);
    QJsonObject entry{{"id",id},{"name",name.trimmed()},{"variant",variant},{"file",path}};ModRepository::write(m_root+"/skins/"+id+".json",entry);return entry;
}
QJsonArray SkinManager::skins()const{QJsonArray list;QDir dir(m_root+"/skins");for(const auto &file:dir.entryList({"*.json"},QDir::Files)){auto entry=ModRepository::read(dir.filePath(file));entry["file"]=dir.filePath(QFileInfo(file).completeBaseName()+".png");list.append(entry);}return list;}
QImage SkinManager::preview(const QImage &skin,const QString &variant,bool back){
    QImage out(240,320,QImage::Format_ARGB32_Premultiplied);out.fill(Qt::transparent);QPainter p(&out);p.setRenderHint(QPainter::SmoothPixmapTransform,false);
    int unit=8,arm=variant=="slim"?3:4;int x=120-4*unit,y=24;
    p.setPen(Qt::NoPen);p.setBrush(QColor(0,0,0,75));p.drawEllipse(QRect(43,290,154,16));
    auto part=[&](QRect source,QRect target){p.drawImage(target,skin,source);};
    part(QRect(back?24:8,8,8,8),QRect(x,y,64,64));part(QRect(back?24:8,8,8,8).translated(32,0),QRect(x-2,y-2,68,68));
    part(QRect(back?32:20,20,8,12),QRect(x,y+64,64,96));
    part(QRect(back?12:4,20,4,12),QRect(x,y+160,32,96));
    part(QRect(skin.height()==32?(back?12:4):(back?28:20),skin.height()==32?20:52,4,12),QRect(x+32,y+160,32,96));
    part(QRect(back?48+arm:44,20,arm,12),QRect(x-arm*unit,y+64,arm*unit,96));
    part(QRect(skin.height()==32?(back?48+arm:44):(back?40+arm:36),skin.height()==32?20:52,arm,12),QRect(x+64,y+64,arm*unit,96));
    if(skin.height()==64){part(QRect(back?32:20,36,8,12),QRect(x-1,y+63,66,98));part(QRect(back?12:4,36,4,12),QRect(x-1,y+159,34,98));part(QRect(back?12:4,52,4,12),QRect(x+31,y+159,34,98));part(QRect(back?48+arm:44,36,arm,12),QRect(x-arm*unit-1,y+63,arm*unit+2,98));part(QRect(back?56+arm:52,52,arm,12),QRect(x+63,y+63,arm*unit+2,98));}
    return out;
}
void SkinManager::upload(const QString &file,const QString &variant,const QString &token){
    if(token.isEmpty())fail("Microsoft account required.");auto data=png(skinImage(file));if(variant!="slim"&&variant!="classic")fail("Invalid model.");
    QNetworkAccessManager nam;QNetworkRequest req(QUrl("https://api.minecraftservices.com/minecraft/profile/skins"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("Authorization","Bearer "+token.toUtf8());req.setTransferTimeout(30000);
    req.setHeader(QNetworkRequest::UserAgentHeader,"EBALIA/4.0");
    auto multi=new QHttpMultiPart(QHttpMultiPart::FormDataType);QHttpPart model;model.setHeader(QNetworkRequest::ContentDispositionHeader,"form-data; name=\"variant\"");model.setBody(variant.toUtf8());multi->append(model);
    QHttpPart part;part.setHeader(QNetworkRequest::ContentDispositionHeader,"form-data; name=\"file\"; filename=\"skin.png\"");part.setHeader(QNetworkRequest::ContentTypeHeader,"image/png");part.setBody(data);multi->append(part);
    auto reply=nam.post(req,multi);multi->setParent(reply);QEventLoop loop;QTimer timer;timer.setSingleShot(true);QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);QObject::connect(&timer,&QTimer::timeout,reply,&QNetworkReply::abort);timer.start(60000);loop.exec();
    if(reply->error()!=QNetworkReply::NoError)fail("Minecraft skin service: "+reply->errorString());
    auto result=QJsonDocument::fromJson(reply->readAll()).object();if(!result.contains("skins"))fail("The skin service returned an unexpected response. Check your Minecraft profile before retrying.");
}
void SkinManager::applyLocal(const QString &file,const QString &variant,const QString &instance,const QString &clientJar){
    auto image=skinImage(file);if(image.height()!=64)fail("Local skins require a modern 64 × 64 PNG.");
    if(variant!="classic"&&variant!="slim")fail("Invalid skin model.");
    std::unique_ptr<archive,decltype(&archive_read_free)> a(archive_read_new(),archive_read_free);archive_read_support_format_zip(a.get());
    if(archive_read_open_filename(a.get(),QFile::encodeName(clientJar).constData(),65536)!=ARCHIVE_OK)fail("Install the instance before applying a local skin.");
    archive_entry *entry;int format=0;QStringList textures;
    while(archive_read_next_header(a.get(),&entry)==ARCHIVE_OK){auto path=QString::fromUtf8(archive_entry_pathname(entry));
        if(path=="version.json"){QByteArray data;char buffer[8192];la_ssize_t n;while((n=archive_read_data(a.get(),buffer,sizeof(buffer)))>0){data.append(buffer,n);if(data.size()>1024*1024)fail("Invalid game metadata.");}auto p=QJsonDocument::fromJson(data).object()["pack_version"].toObject()["resource"];format=p.isObject()?p.toObject()["major"].toInt():p.toInt();}
        if(path.startsWith("assets/minecraft/textures/entity/player/")&&path.endsWith(".png")&&path.split('/').size()==7)textures<<path;
        if(path=="assets/minecraft/textures/entity/steve.png"||path=="assets/minecraft/textures/entity/alex.png")textures<<path;
    }
    if(format==0||textures.isEmpty())fail("This game version does not support the local skin pack. Use a Microsoft skin instead.");
    QTemporaryDir stage;auto path=stage.path()+"/skin.zip";
    std::unique_ptr<archive,decltype(&archive_write_free)> zip(archive_write_new(),archive_write_free);archive_write_set_format_zip(zip.get());
    if(archive_write_open_filename(zip.get(),QFile::encodeName(path).constData())!=ARCHIVE_OK)fail("Could not create skin pack.");
    auto add=[&](const QString &name,const QByteArray &bytes){auto e=archive_entry_new();archive_entry_set_pathname(e,name.toUtf8().constData());archive_entry_set_filetype(e,AE_IFREG);archive_entry_set_perm(e,0644);archive_entry_set_size(e,bytes.size());int status=archive_write_header(zip.get(),e);archive_entry_free(e);if(status!=ARCHIVE_OK||archive_write_data(zip.get(),bytes.constData(),bytes.size())!=bytes.size())fail("Could not write skin pack.");};
    add("pack.mcmeta",QJsonDocument(QJsonObject{{"pack",QJsonObject{{"pack_format",format},{"description","EBALIA local skin (client only)"}}}}).toJson());
    for(const auto &texture:textures)add(texture,png(modelTexture(image,variant=="slim",texture.contains("/slim/")||texture.endsWith("/alex.png"))));
    if(archive_write_close(zip.get())!=ARCHIVE_OK)fail("Could not finish skin pack.");
    auto updated=optionsWith(options(instance),true);QDir().mkpath(instance+"/resourcepacks");QFile generated(path);
    if(!generated.open(QIODevice::ReadOnly))fail("Could not read generated skin pack.");
    auto destination=instance+"/resourcepacks/ebalia-local-skin.zip";QFile previous(destination);bool existed=previous.exists();QByteArray original;
    if(existed){if(!previous.open(QIODevice::ReadOnly))fail("Could not back up local skin pack.");original=previous.readAll();previous.close();}
    save(destination,generated.readAll());
    try{save(instance+"/options.txt",updated);}catch(...){if(existed)save(destination,original);else QFile::remove(destination);throw;}

}
void SkinManager::removeLocal(const QString &instance){save(instance+"/options.txt",optionsWith(options(instance),false));}
