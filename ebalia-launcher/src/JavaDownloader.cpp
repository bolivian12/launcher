#include "JavaDownloader.hpp"
#include "Language.hpp"
#include <QtCore>
#include <QtConcurrent>
#include <archive.h>
#include <archive_entry.h>
#include <memory>
#include <stdexcept>
namespace {
const QString catalog="https://piston-meta.mojang.com/v1/products/java-runtime/2ec0cc96c44e5a76b9c8b7c39df7210883d12871/all.json";
[[noreturn]] void fail(const QString &message){throw std::runtime_error(message.toStdString());}
QByteArray sha1(const QByteArray &data){return QCryptographicHash::hash(data,QCryptographicHash::Sha1).toHex();}
QString relative(const QString &path){
    if(path.isEmpty()||QDir::isAbsolutePath(path)||path.contains('\\')||path.contains(':')||path.split('/').contains(".."))fail("Unsafe path in Java runtime: "+path);
    return path;
}
// Mojang serves most files as raw LZMA ("alone" format) next to the uncompressed copy.
QByteArray unlzma(const QByteArray &data){
    std::unique_ptr<archive,decltype(&archive_read_free)> a(archive_read_new(),archive_read_free);
    archive_read_support_filter_lzma(a.get());archive_read_support_format_raw(a.get());
    if(archive_read_open_memory(a.get(),data.constData(),size_t(data.size()))!=ARCHIVE_OK)return {};
    archive_entry *entry=nullptr;if(archive_read_next_header(a.get(),&entry)!=ARCHIVE_OK)return {};
    QByteArray out;char buffer[65536];la_ssize_t n;
    while((n=archive_read_data(a.get(),buffer,sizeof buffer))>0){out.append(buffer,n);if(out.size()>1024LL*1024*1024)return {};}
    return n<0?QByteArray():out;
}
}
QString JavaDownloader::platform(){
    const auto arch=QSysInfo::currentCpuArchitecture();
#ifdef Q_OS_WIN
    if(arch=="arm64")return "windows-arm64";
    return arch=="i386"?"windows-x86":"windows-x64";
#elif defined(Q_OS_MACOS)
    return arch=="arm64"?"mac-os-arm64":"mac-os";
#else
    if(arch=="x86_64")return "linux";
    return arch=="i386"?"linux-i386":QString();
#endif
}
QString JavaDownloader::component(int major){
    switch(major){case 8:return "jre-legacy";case 16:return "java-runtime-alpha";case 17:return "java-runtime-gamma";case 21:return "java-runtime-delta";case 25:return "java-runtime-epsilon";default:return {};}
}
QString JavaDownloader::installed(const QString &root,const QString &component,QString platform){
    if(platform.isEmpty())platform=JavaDownloader::platform();
    QStringList platforms{platform};
    if(platform=="mac-os-arm64")platforms<<"mac-os";else if(platform=="windows-arm64")platforms<<"windows-x64"; // see install()
    for(const auto &p:platforms)try{auto marker=ModRepository::read(root+"/"+component+"-"+p+"/.ebalia-runtime.json");auto java=root+"/"+component+"-"+p+"/"+relative(marker["java"].toString());if(QFileInfo(java).isFile())return java;}catch(...){}
    return {};
}
QString JavaDownloader::install(const QString &root,const QString &component,QString platform,std::function<void(int,int)> progress,ModRepository::Transport transport){
    if(platform.isEmpty())platform=JavaDownloader::platform();
    if(platform.isEmpty()||!ModRepository::safeName(component)||!ModRepository::safeName(platform))fail(Language::key("Mojang does not publish an official Java runtime for this system."));
    auto get=[transport](const QString &url){return transport?transport(QUrl(url)):ModRepository::fetch(QUrl(url));};
    auto json=[&](const QString &url,const QByteArray &hash={}){auto data=get(url);if(!hash.isEmpty()&&sha1(data)!=hash.toLower())fail("Java runtime catalog checksum mismatch");QJsonParseError e;auto d=QJsonDocument::fromJson(data,&e);if(e.error!=QJsonParseError::NoError||!d.isObject())fail("Invalid Java runtime catalog");return d.object();};
    const auto all=json(catalog);auto entries=all[platform].toObject()[component].toArray();
    // Mojang has no Java 8 for Apple Silicon or Windows on ARM: like the official launcher,
    // use the x64 build, which macOS (Rosetta) and Windows 11 run through emulation. The
    // LWJGL 2 natives of the versions that need Java 8 are x64-only anyway.
    if(entries.isEmpty()&&(platform=="mac-os-arm64"||platform=="windows-arm64")){
        const auto x64=platform=="mac-os-arm64"?QString("mac-os"):QString("windows-x64");
        entries=all[x64].toObject()[component].toArray();if(!entries.isEmpty())platform=x64;
    }
    if(entries.isEmpty())fail(Language::key("Mojang does not publish an official Java runtime for this system.")+" ("+component+" · "+platform+")");
    const auto entry=entries.first().toObject(),manifest=entry["manifest"].toObject();const auto version=entry["version"].toObject()["name"].toString();
    const auto target=root+"/"+component+"-"+platform;
    // Two instances may request the same runtime at once; the second one reuses the first download.
    QDir().mkpath(root);QLockFile lock(target+".lock");lock.setStaleLockTime(0);if(!lock.lock())fail("Could not lock the Java folder");
    try{auto marker=ModRepository::read(target+"/.ebalia-runtime.json");auto java=target+"/"+relative(marker["java"].toString());if(marker["version"].toString()==version&&QFileInfo(java).isFile())return java;}catch(...){}
    const auto files=json(manifest["url"].toString(),manifest["sha1"].toString().toLatin1())["files"].toObject();
    QDir().mkpath(root);QTemporaryDir stage(root+"/.java-XXXXXX");if(!stage.isValid())fail("Could not prepare the Java folder");
    const auto base=stage.path()+"/runtime";QDir().mkpath(base);
    const QString executable=platform.startsWith("windows")?"bin/java.exe":"bin/java";QString java;
    QList<QPair<QString,QJsonObject>> downloads;QList<QPair<QString,QString>> links;
    for(auto it=files.begin();it!=files.end();++it){
        const auto path=relative(it.key());const auto o=it.value().toObject();const auto type=o["type"].toString();
        if(type=="directory")QDir().mkpath(base+"/"+path);
        else if(type=="file"){downloads<<qMakePair(path,o);if((path==executable||path.endsWith("/"+executable))&&(java.isEmpty()||path.size()<java.size()))java=path;}
        else if(type=="link")links<<qMakePair(path,o["target"].toString());
    }
    if(java.isEmpty())fail("The Java runtime package has no java executable");
    QThreadPool pool;pool.setMaxThreadCount(8);QList<QFuture<QString>> jobs;
    for(const auto &file:downloads)jobs<<QtConcurrent::run(&pool,[base,file,get]{
        try{
            const auto downloads=file.second["downloads"].toObject();const auto raw=downloads["raw"].toObject(),packed=downloads["lzma"].toObject();
            const auto expected=raw["sha1"].toString().toLatin1().toLower();QByteArray data;bool ok=false;
            if(!packed.isEmpty()){auto compressed=get(packed["url"].toString());if(sha1(compressed)==packed["sha1"].toString().toLatin1().toLower()){data=unlzma(compressed);ok=sha1(data)==expected;}}
            if(!ok){data=get(raw["url"].toString());if(sha1(data)!=expected)fail("Damaged Java download: "+file.first);}
            const auto path=base+"/"+file.first;QDir().mkpath(QFileInfo(path).absolutePath());QSaveFile out(path);
            if(!out.open(QIODevice::WriteOnly)||out.write(data)!=data.size()||!out.commit())fail("Could not write "+file.first);
            if(file.second["executable"].toBool())QFile::setPermissions(path,QFile::permissions(path)|QFile::ExeOwner|QFile::ExeGroup|QFile::ExeOther|QFile::ReadGroup|QFile::ReadOther);
            return QString();
        }catch(const std::exception &e){return QString::fromUtf8(e.what());}
    });
    QString error;int done=0;
    for(auto &job:jobs){auto e=job.result();if(!e.isEmpty()&&error.isEmpty())error=e;if(progress)progress(++done,jobs.size());}
    if(!error.isEmpty())fail(error);
#ifndef Q_OS_WIN
    for(const auto &link:links){
        const auto path=base+"/"+link.first;const auto resolved=QDir::cleanPath(QFileInfo(path).absolutePath()+"/"+link.second);
        if(link.second.startsWith('/')||!(resolved+"/").startsWith(QDir::cleanPath(base)+"/"))fail("Unsafe link in Java runtime: "+link.first);
        QDir().mkpath(QFileInfo(path).absolutePath());QFile::remove(path);if(!QFile::link(link.second,path))fail("Could not create "+link.first);
    }
#endif
    ModRepository::write(base+"/.ebalia-runtime.json",{{"component",component},{"platform",platform},{"version",version},{"java",java}});
    QString old;if(QFileInfo::exists(target)){old=target+".old-"+QString::number(QDateTime::currentMSecsSinceEpoch());if(!QDir().rename(target,old))fail("Could not replace the previous Java runtime");}
    if(!QDir().rename(base,target)){if(!old.isEmpty())QDir().rename(old,target);fail("Could not install the Java runtime");}
    if(!old.isEmpty())QDir(old).removeRecursively();
    return target+"/"+java;
}
