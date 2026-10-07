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
#include "SoftwareGl.hpp"
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
#include <QDirIterator>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <QSet>
#include <QCoreApplication>
#include <stdexcept>

namespace {
// Raised when launch-profile.json must be rebuilt: 2 keeps every library classifier (Forge universal + client).
constexpr int profileFormat=2;
// Parallel downloads for libraries and assets; each worker keeps its connection open (ModRepository::fetch).
constexpr int downloadThreads=16;
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
// Minecraft 1.13 and later (LWJGL 3) can load another opengl32.dll.
bool usesLwjgl3(const QJsonObject &profile) {
    for (const auto &l : profile["libraries"].toArray()) if (l.toObject()["name"].toString().startsWith("org.lwjgl:lwjgl:3")) return true;
    return false;
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
    // group:artifact:version[:classifier][@extension]
    auto coordinate = name; QString extension = "jar";
    if (auto at = coordinate.lastIndexOf('@'); at > 0) { extension = coordinate.mid(at+1); coordinate.truncate(at); }
    auto parts = coordinate.split(':'); if (parts.size() < 3 || extension.isEmpty()) fail("Coordenada Maven inválida.");
    QString group = parts[0]; group.replace('.','/');
    return group + "/" + parts[1] + "/" + parts[2] + "/" + parts[1] + "-" + parts[2] + (parts.size()>3 ? "-"+parts[3] : "") + "." + extension;
}
// A loader may ship several files of one artifact that differ only by classifier
// (Forge's universal and client jars, LWJGL's natives), so all three count.
QString libraryKey(const QString &name) {
    auto coordinate = name.section('@',0,0); auto parts = coordinate.split(':');
    return parts.value(0) + ":" + parts.value(1) + ":" + parts.value(3);
}
// Like the official launcher's inheritsFrom: the loader's libraries come first and
// replace the game's copy of the same artifact; every other game library stays.
QJsonArray mergeLibraryLists(const QJsonArray &game, const QJsonArray &loader) {
    QJsonArray merged; QSet<QString> keys;
    for (const auto &l : loader) { const auto key = libraryKey(s(l.toObject(),"name")); if (keys.contains(key)) continue; keys.insert(key); merged.append(l); }
    for (const auto &l : game) if (!keys.contains(libraryKey(s(l.toObject(),"name")))) merged.append(l);
    return merged;
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
            // Loader instances prepared by launchers before 1.1.0 lost libraries that share a coordinate (Forge's universal jar):
            // they install again, which reuses every downloaded file, so Play never starts a broken profile.
            if (o["ready"].toBool() && s(o,"loader")!="vanilla") {
                const auto profile = entry.filePath()+"/launch-profile.json";
                int format=0; try { format=ModRepository::read(profile)["ebaliaProfile"].toInt(); } catch (...) {}
                if (format < profileFormat) { o["ready"]=false; try { ModRepository::write(entry.filePath()+"/instance.json",o); } catch (...) {} }
            }
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
    if (QDir().rename(dir,m_root+"/trash/"+QFileInfo(dir).fileName()+"-"+QString::number(QDateTime::currentMSecsSinceEpoch()))) return;
    // Windows refuses to rename a folder while a program keeps one of its files open; the system recycle bin may still accept it.
    if (QFile::moveToTrash(dir)) return;
    fail(Language::key("The instance could not be moved because a program is using its files. Close Minecraft, file explorer windows or other programs that use that folder, then try again."));
}
QString McInstanceManager::copyInstance(const QString &dir, const QString &name) {
    if (isRunning(dir) || isInstalling(dir)) fail("La instancia está en uso.");
    if (QFileInfo(dir).absolutePath() != QDir(instancesRoot()).absolutePath() || QFileInfo(dir).isSymLink() || name.trimmed().isEmpty()) fail("Ruta de instancia inválida.");
    QTemporaryDir stage(m_root+"/.copy-XXXXXX"); if (!stage.isValid()) fail("No se pudo preparar la copia.");
    const auto target=stage.path()+"/instance"; const QStringList skip{"natives","logs","crash-reports","launcher.log","loader-install.log"};
    QDirIterator it(dir,QDir::Files|QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
    QDir().mkpath(target);
    while (it.hasNext()) {
        const auto path=it.next(); const auto rel=QDir(dir).relativeFilePath(path); if (skip.contains(rel.section('/',0,0)) || it.fileInfo().isSymLink()) continue;
        if (it.fileInfo().isDir()) { QDir().mkpath(target+"/"+rel); continue; }
        QDir().mkpath(QFileInfo(target+"/"+rel).absolutePath()); if (!QFile::copy(path,target+"/"+rel)) fail("No se pudo copiar " + rel);
    }
    auto info=ModRepository::read(target+"/instance.json"); info["name"]=name.trimmed(); info["lastPlayed"]=0; info["totalSecs"]=0;
    ModRepository::write(target+"/instance.json",info);
    const auto destination=instancesRoot()+"/"+QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!QDir().rename(target,destination)) fail("No se pudo guardar la copia.");
    return destination;
}
void McInstanceManager::exportInstance(const QString &dir, const QString &zip) {
    if (isRunning(dir) || isInstalling(dir)) fail("La instancia está en uso.");
    Archive::compress(dir,zip,{"natives","logs","crash-reports","launcher.log","loader-install.log","launch-profile.json"});
}
QJsonArray McInstanceManager::mergeLibraries(const QJsonArray &game, const QJsonArray &loader) { return mergeLibraryLists(game, loader); }
bool McInstanceManager::allowedByRules(const QJsonObject &object) {
    auto rules = object["rules"].toArray(); if (rules.isEmpty()) return true;
    bool allowed = false;
    for (const auto &rv : rules) {
        auto r = rv.toObject(); auto os = r["os"].toObject(); bool match = true;
        if (os.contains("name") && s(os,"name") != osName()) match = false;
        if (os.contains("arch")) {
            // Manifests name architectures differently: x86_64/amd64, arm64/aarch64, x86/i386.
            auto normal = [](QString a){a=a.toLower();if(a=="x86_64"||a=="x64")return QString("amd64");if(a=="aarch64")return QString("arm64");if(a=="i386"||a=="i686")return QString("x86");return a;};
            if (normal(QSysInfo::currentCpuArchitecture()) != normal(s(os,"arch"))) match = false;
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
        QString lv = s(info,"loaderVersion"); bool found = false;
        // Loaders::versions() orders the catalog newest first, releases before betas.
        const auto available = Loaders::versions(loader, game);
        if (lv.isEmpty() && !available.isEmpty()) { lv = available.first(); found = true; }
        else found = available.contains(lv);
        if (!found || !ModRepository::safeName(lv)) fail("No hay una versión compatible de " + loader + " para " + game);
        auto lp = json(base + game + "/" + lv + "/profile/json");
        if (s(lp,"mainClass").isEmpty()) fail("Perfil del cargador incompleto.");
        profile["mainClass"]=lp["mainClass"]; info["loaderVersion"]=lv;
        QJsonArray loaderLibs;
        for (const auto &l : lp["libraries"].toArray()) {
            auto lib=l.toObject(); auto path=mavenPath(s(lib,"name")); auto repo=s(lib,"url");
            if (repo.isEmpty()) repo="https://libraries.minecraft.net/";
            if (!repo.endsWith('/')) repo+='/';
            QJsonObject artifact{{"path",path},{"url",repo+path}};
            if (!s(lib,"sha1").isEmpty()) artifact["sha1"]=lib["sha1"];
            lib["downloads"]=QJsonObject{{"artifact",artifact}}; loaderLibs.append(lib);
        }
        profile["libraries"]=mergeLibraryLists(profile["libraries"].toArray(),loaderLibs);
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
        // Forge (and NeoForge for 1.20.1) versions carry the game version: modpacks often write only "47.1.106".
        if((loader=="forge"||(loader=="neoforge"&&game=="1.20.1"))&&!lv.isEmpty()&&!lv.startsWith(game+"-"))lv=game+"-"+lv;
        if(lv.isEmpty()&&!available.isEmpty())lv=available.first();
        if(lv.isEmpty()||!available.contains(lv))fail("No hay una versión compatible de " + loader + " para " + game);
        auto installer=Loaders::installerUrl(loader,game,lv);
        auto hash=QString::fromUtf8(ModRepository::fetch(QUrl(installer+".sha1"))).trimmed().section(' ',0,0);
        if(!QRegularExpression("^[a-fA-F0-9]{40}$").match(hash).hasMatch())fail("Hash del instalador inválido.");
        auto jar=m_root+"/installers/"+loader+"-"+lv+".jar";
        download({{"url",installer},{"sha1",hash.toLower()}},jar);
        QTemporaryDir unpack;Archive::extract(jar,unpack.path());
        QJsonObject loaderProfile;
        // Forge 1.12.2 and older ship a "simple installer" without --installClient: like Prism and MultiMC,
        // take the universal jar out of it and use the profile it contains; there are no processors to run.
        const bool legacy=!QFile::exists(unpack.path()+"/version.json");
        if(!legacy)loaderProfile=ModRepository::read(unpack.path()+"/version.json");
        else loaderProfile=ModRepository::read(unpack.path()+"/install_profile.json")["versionInfo"].toObject();
        auto profileId=s(loaderProfile,"id");if(!ModRepository::safeName(profileId))fail("Perfil del instalador inválido.");
        if(legacy) {
            const auto installInfo=ModRepository::read(unpack.path()+"/install_profile.json")["install"].toObject();
            const auto universal=s(installInfo,"filePath");
            if(universal.isEmpty()||universal.contains('/')||universal.contains('\\')||!QFile::exists(unpack.path()+"/"+universal))fail("Perfil del instalador inválido.");
            const auto target=m_root+"/libraries/"+safeRelative(mavenPath(s(installInfo,"path")));
            QDir().mkpath(QFileInfo(target).absolutePath());QFile::remove(target);
            if(!QFile::copy(unpack.path()+"/"+universal,target))fail("No se pudo instalar " + loader + ".");
            QJsonArray clientLibs;
            for(const auto &l:loaderProfile["libraries"].toArray())if(l.toObject()["clientreq"].toBool(true))clientLibs.append(l); // clientreq:false = server only
            loaderProfile["libraries"]=clientLibs;
        } else {
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
        }
        QJsonArray loaderLibs;
        for(const auto &lv:loaderProfile["libraries"].toArray()) {
            auto lib=lv.toObject();auto path=mavenPath(s(lib,"name"));
            auto downloads=lib["downloads"].toObject(),art=downloads["artifact"].toObject();
            if(s(art,"path").isEmpty())art["path"]=path;
            if(s(art,"url").isEmpty()&&!QFile::exists(m_root+"/libraries/"+path)) {
                auto repo=s(lib,"url");if(repo.isEmpty())repo="https://libraries.minecraft.net/";if(!repo.endsWith('/'))repo+='/';art["url"]=repo+path;
            }
            downloads["artifact"]=art;lib["downloads"]=downloads;loaderLibs.append(lib);
        }
        profile["libraries"]=mergeLibraryLists(profile["libraries"].toArray(),loaderLibs);profile["mainClass"]=loaderProfile["mainClass"];
        if(loaderProfile.contains("arguments")) {
            auto all=profile["arguments"].toObject(),extra=loaderProfile["arguments"].toObject();
            for(const auto &kind:{"game","jvm"}){auto a=all[kind].toArray();for(const auto &v:extra[kind].toArray())a.append(v);all[kind]=a;}
            profile["arguments"]=all;
        } else if(loaderProfile.contains("minecraftArguments"))profile["minecraftArguments"]=loaderProfile["minecraftArguments"];
        info["loaderVersion"]=lv;
    }
    auto libs=profile["libraries"].toArray(); int i=0;
    QDir().mkpath(dir+"/natives");
    // Libraries download in parallel like the assets below; natives are extracted once all of them are present.
    QList<QPair<QJsonObject,QString>> libraryJobs; QStringList natives;
    for (const auto &l : libs) {
        auto lib=l.toObject(); if (!allowedByRules(lib)) continue;
        auto downloads=lib["downloads"].toObject(), artifact=downloads["artifact"].toObject();
        if (!artifact.isEmpty()) libraryJobs.append({artifact,m_root+"/libraries/"+safeRelative(s(artifact,"path"))});
        auto key=lib["natives"].toObject()[osName()].toString(); key.replace("${arch}",QSysInfo::WordSize==64?"64":"32");
        if (!key.isEmpty()) {
            auto nat=downloads["classifiers"].toObject()[key].toObject(); if (nat.isEmpty()) fail("Falta una biblioteca nativa para " + osName());
            auto path=m_root+"/libraries/"+safeRelative(s(nat,"path")); libraryJobs.append({nat,path}); natives<<path;
        }
    }
    {
        QThreadPool pool; pool.setMaxThreadCount(downloadThreads);
        QList<QFuture<QString>> running;
        for (const auto &job : libraryJobs) running << QtConcurrent::run(&pool,[job] { try { download(job.first,job.second); return QString(); } catch(const std::exception &e) {return QString::fromUtf8(e.what());} });
        QString error;
        for (auto &f : running) { auto e=f.result(); if (!e.isEmpty() && error.isEmpty()) error=e; ++i; emit installProgress(dir,10+int(35.0*i/qMax(1,libraryJobs.size())),QString("Bibliotecas %1/%2").arg(i).arg(libraryJobs.size())); }
        if (!error.isEmpty()) fail(error);
    }
    for (const auto &path : natives) Archive::extract(path,dir+"/natives");
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
        QThreadPool pool; pool.setMaxThreadCount(downloadThreads);
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
    profile["ebaliaProfile"]=profileFormat;
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
        if (const auto extra=plan["environment"].toObject(); !extra.isEmpty()) { auto env=QProcessEnvironment::systemEnvironment(); for (auto it=extra.begin();it!=extra.end();++it) env.insert(it.key(),it.value().toString()); proc->setProcessEnvironment(env); }
        proc->setProcessChannelMode(QProcess::MergedChannels); proc->setStandardOutputFile(dir+"/launcher.log",QIODevice::Truncate);
        connect(proc,&QProcess::started,this,[this,dir,info]() mutable {
            info["lastPlayed"]=QDateTime::currentSecsSinceEpoch(); try {ModRepository::write(dir+"/instance.json",info);}catch(...){} emit gameStarted(dir);
        });
        connect(proc,&QProcess::errorOccurred,this,[this,dir,proc](QProcess::ProcessError e) {
            if(e==QProcess::FailedToStart) {m_running.remove(dir);emit launchFailed(dir,proc->errorString());proc->deleteLater();}
        });
        connect(proc,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,dir,proc](int code,QProcess::ExitStatus){m_running.remove(dir);proc->deleteLater();emit gameEnded(dir,code);});
#ifdef Q_OS_WIN
        proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a){a->flags|=CREATE_NO_WINDOW;}); // no console window next to the game
#endif
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
            {"classpath_separator",sep},{"library_directory",m_root+"/libraries"},{"launcher_name","EBALIA"},{"launcher_version",QCoreApplication::applicationVersion()},
            {"clientid",""},{"auth_xuid",""}};
        QStringList args{QString("-Xmx%1M").arg(qBound(512,info["xmx"].toInt(4096),65536))};
        // Software OpenGL (virtual machines, missing drivers): LWJGL 3 loads Mesa's opengl32.dll.
        QJsonObject environment;
        if (info["softwareRendering"].toBool() && SoftwareGl::available()) {
#ifdef Q_OS_WIN
            if (usesLwjgl3(p)) {
                const auto mesa=SoftwareGl::prepare(m_root);args << SoftwareGl::javaArguments(mesa);
                environment["PATH"]=QDir::toNativeSeparators(mesa)+QDir::listSeparator()+qEnvironmentVariable("PATH");environment["GALLIUM_DRIVER"]="llvmpipe";
            }
#else
            environment["LIBGL_ALWAYS_SOFTWARE"]="1";environment["GALLIUM_DRIVER"]="llvmpipe";
#endif
        }
        if (p.contains("arguments")) args << arguments(p["arguments"].toObject()["jvm"].toArray(),values);
        else args << "-Djava.library.path="+dir+"/natives" << "-cp" << cp.join(sep);
        auto logging=p["logging"].toObject()["client"].toObject(); if (!logging.isEmpty()) { auto arg=s(logging,"argument");arg.replace("${path}",m_root+"/assets/log_configs/"+s(logging["file"].toObject(),"id"));args<<arg; }
        args << s(p,"mainClass");
        if (p.contains("arguments")) args << arguments(p["arguments"].toObject()["game"].toArray(),values);
        else { QJsonArray legacy; for (const auto &a : QProcess::splitCommand(s(p,"minecraftArguments"))) legacy.append(a); args << arguments(legacy,values); }
        return QJsonObject{{"java",java},{"args",QJsonArray::fromStringList(args)},{"info",info},{"environment",environment}};
      } catch(const std::exception &e) { return QJsonObject{{"error",QString::fromUtf8(e.what())}}; }
    }));
}
void McInstanceManager::killInstance(const QString &dir) { if (auto p=m_running.value(dir)) p->terminate(); }
