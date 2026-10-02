#include "McInstanceManager.hpp"
#include "ModRepository.hpp"
#include "Archive.hpp"
#include "Loaders.hpp"
#include <QTemporaryDir>
#include "JavaRunner.hpp"
#include "JavaRuntime.hpp"
#include "JavaDownloader.hpp"
#include "Language.hpp"
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <QUuid>
#include <QDateTime>
#include <QSettings>
#include <QSysInfo>
#include <QRegularExpression>
#include <QLockFile>
#include <QStandardPaths>
#include <stdexcept>

namespace {
const QString manifestUrl = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
[[noreturn]] void fail(const QString &s) { throw std::runtime_error(s.toStdString()); }
QString s(const QJsonObject &o, const char *k) { return o[QLatin1String(k)].toString(); }
void save(const QString &path, const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath()); QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit()) fail("No se pudo guardar " + path);
}
QJsonObject json(const QString &url) {
    QJsonParseError e; auto doc = QJsonDocument::fromJson(ModRepository::fetch(QUrl(url)), &e);
    if (e.error != QJsonParseError::NoError || !doc.isObject()) fail("Metadatos inválidos: " + url);
    return doc.object();
}
void download(const QJsonObject &meta, const QString &path) {
    const auto sha = s(meta,"sha1").toLatin1();
    if (QFile::exists(path)) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly) && (!sha.isEmpty() ? QCryptographicHash::hash(f.readAll(),QCryptographicHash::Sha1).toHex() == sha : f.size() > 0)) return;
    }
    if (s(meta,"url").isEmpty()) fail("URL de descarga vacía: " + path);
    auto data = ModRepository::fetch(QUrl(s(meta,"url")));
    if (data.isEmpty() || (!sha.isEmpty() && QCryptographicHash::hash(data,QCryptographicHash::Sha1).toHex() != sha)) fail("Descarga dañada: " + path);
    save(path, data);
}
QString osName() {
#ifdef Q_OS_WIN
    return "windows";
#elif defined(Q_OS_MACOS)
    return "osx";
#else
    return "linux";
#endif
}
QString mavenPath(const QString &name) {
    auto parts = name.split(':'); if (parts.size() < 3) fail("Coordenada Maven inválida.");
    QString group = parts[0]; group.replace('.','/');
    return group + "/" + parts[1] + "/" + parts[2] + "/" + parts[1] + "-" + parts[2] + (parts.size()>3 ? "-"+parts[3] : "") + ".jar";
}
QString safeRelative(const QString &path) {
    if (path.isEmpty() || QDir::isAbsolutePath(path) || path.contains('\\') || path.contains(':') || path.split('/').contains("..")) fail("Ruta de metadatos inválida.");
    return path;
}
QList<McVersion> versions(const QJsonObject &manifest) {
    QList<McVersion> list;
    for (const auto &v : manifest["versions"].toArray()) { auto o = v.toObject(); list.append({s(o,"id"),s(o,"type"),s(o,"url"),s(o,"releaseTime").left(10)}); }
    if (list.isEmpty()) fail("El catálogo de Minecraft está vacío.");
    return list;
}

}
McInstanceManager::McInstanceManager(const QString &root, QObject *parent) : QObject(parent), m_root(root + "/mc") {
    QDir().mkpath(instancesRoot());
}
void McInstanceManager::fetchManifest() {
    if (m_fetching) return; m_fetching = true;
    auto *watcher = new QFutureWatcher<QJsonObject>(this);
    const auto cache = m_root + "/manifest.json";
    connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,watcher] {
        m_fetching = false; auto result = watcher->result(); watcher->deleteLater();
        try { emit manifestReady(versions(result)); } catch (const std::exception &) {}
        if (result.contains("error")) emit manifestFailed(s(result,"error"));
    });
    watcher->setFuture(QtConcurrent::run([cache] {
        try { auto o = json(manifestUrl); versions(o); ModRepository::write(cache,o); return o; }
        catch (const std::exception &e) {
            QJsonObject o; try { o = ModRepository::read(cache); } catch (...) {}
            o["error"] = QString::fromUtf8(e.what()) + (o.contains("versions") ? " · Mostrando catálogo guardado." : ""); return o;
        }
    }));
}
QList<McInstance> McInstanceManager::instances() const {
    QList<McInstance> out; QDir root(instancesRoot());
    for (const auto &entry : root.entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name)) {
        if (entry.isSymLink()) continue;
        try {
            auto o = ModRepository::read(entry.filePath()+"/instance.json");
            out.append({s(o,"name"),entry.filePath(),s(o,"mcVersion"),s(o,"loader"),o["ready"].toBool(),o["xmx"].toInt(4096),o["lastPlayed"].toInteger(),o["totalSecs"].toInteger()});
        } catch (...) { /* A damaged instance never prevents the other instances from opening. */ }
    }
    return out;
}
QString McInstanceManager::createInstance(const QString &name, const QString &version, const QString &loader, const QString &lv) {
    if (name.trimmed().isEmpty() || !ModRepository::safeName(version) || !QStringList{"vanilla","fabric","quilt","forge","neoforge"}.contains(loader)) fail("Nombre, versión o cargador inválido.");
    auto dir = instancesRoot()+"/"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    ModRepository::write(dir+"/instance.json",{{"name",name.trimmed()},{"mcVersion",version},{"loader",loader},{"loaderVersion",lv},{"ready",false},{"xmx",4096}});
    QDir().mkpath(dir+"/mods"); return dir;
}
void McInstanceManager::deleteInstance(const QString &dir) {
    if (isRunning(dir) || isInstalling(dir)) fail("La instancia está en uso.");
    if (QFileInfo(dir).absolutePath() != QDir(instancesRoot()).absolutePath() || QFileInfo(dir).isSymLink()) fail("Ruta de instancia inválida.");
    // Reversible removal: retain worlds, configs and mods in the trash directory.
    QDir().mkpath(m_root+"/trash");
    if (!QDir().rename(dir,m_root+"/trash/"+QFileInfo(dir).fileName()+"-"+QString::number(QDateTime::currentMSecsSinceEpoch()))) fail("No se pudo mover la instancia a la papelera.");
}
bool McInstanceManager::allowedByRules(const QJsonObject &object) {
    auto rules = object["rules"].toArray(); if (rules.isEmpty()) return true;
    bool allowed = false;
    for (const auto &rv : rules) {
        auto r = rv.toObject(); auto os = r["os"].toObject(); bool match = true;
        if (os.contains("name") && s(os,"name") != osName()) match = false;
        if (os.contains("arch")) {
            auto arch = QSysInfo::currentCpuArchitecture();
            if (arch == "x86_64") arch = "amd64";
            if (arch != s(os,"arch")) match = false;
        }
        if (os.contains("version") && !QRegularExpression(s(os,"version")).match(QSysInfo::kernelVersion()).hasMatch()) match = false;
        const auto features = r["features"].toObject();
        for (auto it = features.begin(); it != features.end(); ++it) if (it.value().toBool()) match = false;
        if (match) allowed = s(r,"action") == "allow";
    }
    return allowed;
}
QStringList McInstanceManager::arguments(const QJsonArray &list, const QMap<QString,QString> &values) {
    QStringList out;
    for (const auto &arg : list) {
        QStringList entries;
        if (arg.isString()) entries << arg.toString();
        else if (allowedByRules(arg.toObject())) {
            auto value = arg.toObject()["value"];
            if (value.isString()) entries << value.toString(); else for (const auto &v : value.toArray()) entries << v.toString();
        }
        for (auto entry : entries) {
            for (auto it=values.begin();it!=values.end();++it) entry.replace("${"+it.key()+"}",it.value());
            if (entry.contains("${")) fail("Argumento no soportado por esta versión: " + entry);
            out << entry;
        }
    }
    return out;
}
void McInstanceManager::installInstance(const QString &dir) {
    if (isInstalling(dir) || isRunning(dir)) return;
    m_installing.insert(dir); auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher,&QFutureWatcher<QString>::finished,this,[this,watcher,dir] {
        auto error = watcher->result(); watcher->deleteLater(); m_installing.remove(dir); emit installDone(dir,error.isEmpty(),error);
    });
    watcher->setFuture(QtConcurrent::run([this,dir] {
        try { install(dir); return QString(); } catch (const std::exception &e) { return QString::fromUtf8(e.what()); }
    }));
}
void McInstanceManager::install(const QString &dir) {
    QLockFile lock(m_root+"/install.lock"); if (!lock.tryLock()) fail("Otra instalación está usando la biblioteca compartida. Intentá otra vez al terminar.");
    auto info = ModRepository::read(dir+"/instance.json"); info["ready"] = false; ModRepository::write(dir+"/instance.json",info);
    const auto game = s(info,"mcVersion"), loader = s(info,"loader");
    if (!ModRepository::safeName(game)) fail("Versión inválida.");
    emit installProgress(dir,0,"Consultando el catálogo oficial…");
    auto manifest = json(manifestUrl); QString url;
    for (const auto &v : manifest["versions"].toArray()) if (s(v.toObject(),"id")==game) url=s(v.toObject(),"url");
    if (url.isEmpty()) fail("Minecraft " + game + " no aparece en el catálogo oficial.");
    auto profile = json(url);
    if (s(profile,"mainClass").isEmpty()) fail("Falta la clase de arranque.");
    if (loader == "fabric" || loader == "quilt") {
        if (loader != "fabric" && loader != "quilt") fail("Esta instancia usa un cargador todavía no soportado. Forge y NeoForge requieren un instalador completo.");
        emit installProgress(dir,3,"Resolviendo " + loader + "…");
        const auto base = loader == "fabric" ? "https://meta.fabricmc.net/v2/versions/loader/" : "https://meta.quiltmc.org/v3/versions/loader/";
        auto list = QJsonDocument::fromJson(ModRepository::fetch(QUrl(base + game))).array();
        QString lv = s(info,"loaderVersion"); bool found = false;
        for (const auto &v : list) {
            auto lo = v.toObject()["loader"].toObject();
            if ((!lv.isEmpty() && s(lo,"version")==lv) || (lv.isEmpty() && lo["stable"].toBool(true))) { lv=s(lo,"version"); found=true; break; }
        }
        if (!found || !ModRepository::safeName(lv)) fail("No hay una versión compatible de " + loader + " para " + game);
        auto lp = json(base + game + "/" + lv + "/profile/json");
        if (s(lp,"mainClass").isEmpty()) fail("Perfil del cargador incompleto.");
        profile["mainClass"]=lp["mainClass"]; info["loaderVersion"]=lv;
        auto libs = profile["libraries"].toArray();
        for (const auto &l : lp["libraries"].toArray()) {
            auto lib=l.toObject(); auto path=mavenPath(s(lib,"name")); auto repo=s(lib,"url");
            if (repo.isEmpty()) repo="https://libraries.minecraft.net/";
            lib["downloads"]=QJsonObject{{"artifact",QJsonObject{{"path",path},{"url",repo+path}}}}; libs.append(lib);
        }
        profile["libraries"]=libs;
        if (lp.contains("arguments")) {
            auto all=profile["arguments"].toObject(), extra=lp["arguments"].toObject();
            for (const auto &kind : {"game","jvm"}) { auto a=all[kind].toArray(); for (const auto &v : extra[kind].toArray()) a.append(v); all[kind]=a; }
            profile["arguments"]=all;
        }
    }
    emit installProgress(dir,5,"Descargando cliente…");
    download(profile["downloads"].toObject()["client"].toObject(),m_root+"/versions/"+game+"/"+game+".jar");
    if (loader == "forge" || loader == "neoforge") {
        emit installProgress(dir,7,"Preparando instalador oficial de " + loader + "…");
        QString java=javaFor(dir,info,profile);
        auto lv=s(info,"loaderVersion");auto available=Loaders::versions(loader,game);
        if(loader=="forge"&&!lv.isEmpty()&&!lv.startsWith(game+"-"))lv=game+"-"+lv;
        if(lv.isEmpty()&&!available.isEmpty())lv=available.first();
        if(lv.isEmpty()||!available.contains(lv))fail("No hay una versión compatible de " + loader + " para " + game);
        auto installer=Loaders::installerUrl(loader,game,lv);
        auto hash=QString::fromUtf8(ModRepository::fetch(QUrl(installer+".sha1"))).trimmed().section(' ',0,0);
        if(!QRegularExpression("^[a-fA-F0-9]{40}$").match(hash).hasMatch())fail("Hash del instalador inválido.");
        auto jar=m_root+"/installers/"+loader+"-"+lv+".jar";
        download({{"url",installer},{"sha1",hash.toLower()}},jar);
        QTemporaryDir unpack;Archive::extract(jar,unpack.path());
        QJsonObject loaderProfile;
        if(QFile::exists(unpack.path()+"/version.json"))loaderProfile=ModRepository::read(unpack.path()+"/version.json");
        else loaderProfile=ModRepository::read(unpack.path()+"/install_profile.json")["versionInfo"].toObject();
        auto profileId=s(loaderProfile,"id");if(!ModRepository::safeName(profileId))fail("Perfil del instalador inválido.");
        ModRepository::write(m_root+"/versions/"+game+"/"+game+".json",profile);
        if(!QFile::exists(m_root+"/launcher_profiles.json"))ModRepository::write(m_root+"/launcher_profiles.json",{{"profiles",QJsonObject{}}});
        emit installProgress(dir,8,"Instalando " + loader + " · procesando bibliotecas…");
        QProcess proc;proc.setWorkingDirectory(m_root);proc.setProcessChannelMode(QProcess::MergedChannels);
        proc.setStandardOutputFile(dir+"/loader-install.log");
        proc.start(java,{"-Djava.awt.headless=true","-jar",jar,"--installClient",m_root});
        if(!proc.waitForStarted(10000))fail("No se pudo iniciar el instalador de " + loader);
        if(!proc.waitForFinished(15*60*1000)){proc.kill();proc.waitForFinished();fail("El instalador excedió el tiempo máximo. Revisá loader-install.log.");}
        if(proc.exitStatus()!=QProcess::NormalExit||proc.exitCode()!=0)fail("Falló el instalador oficial. Revisá loader-install.log.");
        loaderProfile=ModRepository::read(m_root+"/versions/"+profileId+"/"+profileId+".json");
        auto libs=profile["libraries"].toArray();
        for(const auto &lv:loaderProfile["libraries"].toArray()) {
            auto lib=lv.toObject();auto path=mavenPath(s(lib,"name"));
            auto downloads=lib["downloads"].toObject(),art=downloads["artifact"].toObject();
            if(s(art,"path").isEmpty())art["path"]=path;
            if(s(art,"url").isEmpty()&&!QFile::exists(m_root+"/libraries/"+path)) {
                auto repo=s(lib,"url");if(repo.isEmpty())repo="https://libraries.minecraft.net/";art["url"]=repo+path;
            }
            downloads["artifact"]=art;lib["downloads"]=downloads;
            QString coordinate=s(lib,"name").section(':',0,1);
            for(int n=libs.size()-1;n>=0;--n)if(s(libs[n].toObject(),"name").section(':',0,1)==coordinate)libs.removeAt(n);
            libs.append(lib);
        }
        profile["libraries"]=libs;profile["mainClass"]=loaderProfile["mainClass"];
        if(loaderProfile.contains("arguments")) {
            auto all=profile["arguments"].toObject(),extra=loaderProfile["arguments"].toObject();
            for(const auto &kind:{"game","jvm"}){auto a=all[kind].toArray();for(const auto &v:extra[kind].toArray())a.append(v);all[kind]=a;}
            profile["arguments"]=all;
        } else if(loaderProfile.contains("minecraftArguments"))profile["minecraftArguments"]=loaderProfile["minecraftArguments"];
        info["loaderVersion"]=lv;
    }
    auto libs=profile["libraries"].toArray(); int i=0;
    QDir().mkpath(dir+"/natives");
    for (const auto &l : libs) {
        auto lib=l.toObject(); ++i; if (!allowedByRules(lib)) continue;
        emit installProgress(dir,10+int(35.0*i/qMax(1,libs.size())),QString("Bibliotecas %1/%2").arg(i).arg(libs.size()));
        auto downloads=lib["downloads"].toObject(), artifact=downloads["artifact"].toObject();
        if (!artifact.isEmpty()) download(artifact,m_root+"/libraries/"+safeRelative(s(artifact,"path")));
        auto key=lib["natives"].toObject()[osName()].toString(); key.replace("${arch}",QSysInfo::WordSize==64?"64":"32");
        if (!key.isEmpty()) {
            auto nat=downloads["classifiers"].toObject()[key].toObject(); if (nat.isEmpty()) fail("Falta una biblioteca nativa para " + osName());
            auto path=m_root+"/libraries/"+safeRelative(s(nat,"path")); download(nat,path); Archive::extract(path,dir+"/natives");
        }
    }
    auto index=profile["assetIndex"].toObject();
    if (!index.isEmpty()) {
        auto id=s(index,"id"); if (!ModRepository::safeName(id)) fail("Índice de recursos inválido.");
        auto path=m_root+"/assets/indexes/"+id+".json"; download(index,path);
        auto assetInfo=ModRepository::read(path); auto objects=assetInfo["objects"].toObject(); i=0;
        // Bounded parallelism: each request has its own worker-local network manager.
        QList<QPair<QJsonObject,QString>> jobs;
        for (auto it=objects.begin();it!=objects.end();++it) {
            auto hash=s(it.value().toObject(),"hash"); if (!QRegularExpression("^[a-f0-9]{40}$").match(hash).hasMatch()) fail("Hash de recurso inválido.");
            auto dest=m_root+"/assets/objects/"+hash.left(2)+"/"+hash;
            jobs.append({QJsonObject{{"url","https://resources.download.minecraft.net/"+hash.left(2)+"/"+hash},{"sha1",hash}},dest});
        }
        QThreadPool pool; pool.setMaxThreadCount(8);
        QList<QFuture<QString>> downloads;
        for (const auto &job : jobs) downloads << QtConcurrent::run(&pool,[job] { try { download(job.first,job.second); return QString(); } catch(const std::exception &e) {return QString::fromUtf8(e.what());} });
        QString error;
        for (auto &f : downloads) { auto e=f.result(); if (!e.isEmpty() && error.isEmpty()) error=e; ++i; emit installProgress(dir,45+int(50.0*i/qMax(1,jobs.size())),QString("Recursos %1/%2").arg(i).arg(jobs.size())); }
        if (!error.isEmpty()) fail(error);
        if (assetInfo["virtual"].toBool() || assetInfo["map_to_resources"].toBool()) {
            for (auto it=objects.begin();it!=objects.end();++it) {
                auto hash=s(it.value().toObject(),"hash"); QFile source(m_root+"/assets/objects/"+hash.left(2)+"/"+hash);
                if (!source.open(QIODevice::ReadOnly)) fail("Recurso no disponible."); auto data=source.readAll();
                if (assetInfo["virtual"].toBool()) save(m_root+"/assets/virtual/"+id+"/"+safeRelative(it.key()),data);
                if (assetInfo["map_to_resources"].toBool()) save(dir+"/resources/"+safeRelative(it.key()),data);
            }
        }
    }
    auto logging=profile["logging"].toObject()["client"].toObject();
    if (!logging.isEmpty()) { auto f=logging["file"].toObject(); download(f,m_root+"/assets/log_configs/"+safeRelative(s(f,"id"))); }
    javaFor(dir,info,profile); // Download the official runtime now rather than when the player presses Play.
    ModRepository::write(dir+"/launch-profile.json",profile);
    info["ready"]=true; ModRepository::write(dir+"/instance.json",info); emit installProgress(dir,100,"Instalación completa");
}
QString McInstanceManager::javaFor(const QString &dir,const QJsonObject &info,const QJsonObject &profile) {
    const int required=profile["javaVersion"].toObject()["majorVersion"].toInt(8);
    const auto custom=s(info,"javaPath");
    if(!custom.isEmpty()) {
        auto java=JavaRuntime::inspect(custom);if(JavaRuntime::compatible(required,java.major))return java.path;
        if(!java.major)fail(Language::key("The Java selected in instance settings does not work: %1. Choose “Automatic (recommended)” or another executable.").arg(custom));
        fail(Language::key("The Java selected in instance settings is Java %1, but this Minecraft version requires Java %2. Choose “Automatic (recommended)” to use or download the right one.").arg(java.major).arg(required));
    }
    const auto global=QSettings().value("mc/defaultJava").toString();
    if(!global.isEmpty()){auto java=JavaRuntime::inspect(global);if(JavaRuntime::compatible(required,java.major))return java.path;}
    auto java=JavaRuntime::select(required);if(!java.isEmpty())return java;
    // NixOS cannot run Mojang's generic Linux runtime; its package provides Java through EBALIA_JAVA_PATHS.
    if(QFile::exists("/etc/NIXOS"))fail(Language::key("This version requires Java %1. On NixOS, start EBALIA from its Nix package, which includes Java 8, 17, 21 and 25.").arg(required));
    auto component=s(profile["javaVersion"].toObject(),"component");if(component.isEmpty())component=JavaDownloader::component(required);
    if(component.isEmpty()||JavaDownloader::platform().isEmpty())fail(Language::key("This version requires Java %1. Install it and choose its executable in instance settings.").arg(required));
    emit installProgress(dir,0,Language::key("Downloading official Java %1 from Mojang…").arg(required));
    auto path=JavaDownloader::install(m_root+"/java",component,{},[this,dir,required](int done,int total){emit installProgress(dir,int(100.0*done/qMax(1,total)),QString("Java %1 · %2/%3").arg(required).arg(done).arg(total));});
    auto check=JavaRuntime::inspect(path);
    if(!JavaRuntime::compatible(required,check.major))fail(Language::key("Java %1 was downloaded but could not run on this system. Install it manually and choose it in instance settings.").arg(required));
    return check.path;
}
void McInstanceManager::launch(const QString &dir,const QString &name,const QString &uuid,const QString &token,const QString &type) {
    if (isRunning(dir)||isInstalling(dir)) return;
    // Java discovery (and a possible runtime download) runs off the UI thread; the instance stays busy meanwhile.
    m_installing.insert(dir);auto *watcher=new QFutureWatcher<QJsonObject>(this);
    connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,watcher,dir] {
        auto plan=watcher->result();watcher->deleteLater();m_installing.remove(dir);
        if(plan.contains("error")){emit launchFailed(dir,s(plan,"error"));return;}
        QStringList args;for(const auto &a:plan["args"].toArray())args<<a.toString();
        auto info=plan["info"].toObject();
        auto *proc=new QProcess(this); m_running[dir]=proc; proc->setWorkingDirectory(dir);
        proc->setProcessChannelMode(QProcess::MergedChannels); proc->setStandardOutputFile(dir+"/launcher.log",QIODevice::Truncate);
        connect(proc,&QProcess::started,this,[this,dir,info]() mutable {
            info["lastPlayed"]=QDateTime::currentSecsSinceEpoch(); try {ModRepository::write(dir+"/instance.json",info);}catch(...){} emit gameStarted(dir);
        });
        connect(proc,&QProcess::errorOccurred,this,[this,dir,proc](QProcess::ProcessError e) {
            if(e==QProcess::FailedToStart) {m_running.remove(dir);emit launchFailed(dir,proc->errorString());proc->deleteLater();}
        });
        connect(proc,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,dir,proc](int code,QProcess::ExitStatus){m_running.remove(dir);proc->deleteLater();emit gameEnded(dir,code);});
        proc->start(s(plan,"java"),args);
    });
    watcher->setFuture(QtConcurrent::run([this,dir,name,uuid,token,type]() -> QJsonObject {
      try {
        auto info=ModRepository::read(dir+"/instance.json"); if (!info["ready"].toBool()) fail("Primero instalá la instancia.");
        if (!QFile::exists(dir+"/launch-profile.json")) fail("Instancia del launcher anterior: usá Reparar para preparar el nuevo arranque.");
        auto p=ModRepository::read(dir+"/launch-profile.json"); auto game=s(info,"mcVersion");
        const auto java=javaFor(dir,info,p);
#ifdef Q_OS_WIN
        QString sep=";";
#else
        QString sep=":";
#endif
        QStringList cp;
        for (const auto &l : p["libraries"].toArray()) {
            auto lib=l.toObject(); if (!allowedByRules(lib)) continue;
            auto path=s(lib["downloads"].toObject()["artifact"].toObject(),"path");
            if (!path.isEmpty()) { auto f=m_root+"/libraries/"+safeRelative(path); if (!QFile::exists(f)) fail("Falta una biblioteca; usá Reparar."); cp << f; }
        }
        auto jar=m_root+"/versions/"+game+"/"+game+".jar"; if (!QFile::exists(jar)) fail("Falta el cliente; usá Reparar."); cp << jar;
        auto assetsId=s(p["assetIndex"].toObject(),"id");
        QMap<QString,QString> values{{"auth_player_name",name},{"auth_uuid",uuid},{"auth_access_token",token.isEmpty()?"0":token},
            {"auth_session",token.isEmpty()?"0":token},{"user_type",type},{"user_properties","{}"},{"version_name",game},
            {"version_type",s(p,"type")},{"game_directory",dir},{"assets_root",m_root+"/assets"},{"assets_index_name",assetsId},
            {"game_assets",m_root+"/assets/virtual/"+assetsId},{"natives_directory",dir+"/natives"},{"classpath",cp.join(sep)},
            {"classpath_separator",sep},{"library_directory",m_root+"/libraries"},{"launcher_name","EBALIA"},{"launcher_version","4.0.0"},
            {"clientid",""},{"auth_xuid",""}};
        QStringList args{QString("-Xmx%1M").arg(qBound(512,info["xmx"].toInt(4096),65536))};
        if (p.contains("arguments")) args << arguments(p["arguments"].toObject()["jvm"].toArray(),values);
        else args << "-Djava.library.path="+dir+"/natives" << "-cp" << cp.join(sep);
        auto logging=p["logging"].toObject()["client"].toObject(); if (!logging.isEmpty()) { auto arg=s(logging,"argument");arg.replace("${path}",m_root+"/assets/log_configs/"+s(logging["file"].toObject(),"id"));args<<arg; }
        args << s(p,"mainClass");
        if (p.contains("arguments")) args << arguments(p["arguments"].toObject()["game"].toArray(),values);
        else { QJsonArray legacy; for (const auto &a : QProcess::splitCommand(s(p,"minecraftArguments"))) legacy.append(a); args << arguments(legacy,values); }
        return QJsonObject{{"java",java},{"args",QJsonArray::fromStringList(args)},{"info",info}};
      } catch(const std::exception &e) { return QJsonObject{{"error",QString::fromUtf8(e.what())}}; }
    }));
}
void McInstanceManager::killInstance(const QString &dir) { if (auto p=m_running.value(dir)) p->terminate(); }
