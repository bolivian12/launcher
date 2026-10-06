#include "ModRepository.hpp"
#include "BuildConfig.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QLockFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QUrlQuery>
#include <QTemporaryDir>
#include <QSettings>
#include <QUuid>
#include <QRegularExpression>
#include <stdexcept>
#include <algorithm>

namespace {
[[noreturn]] void fail(const QString &s) { throw std::runtime_error(s.toUtf8().constData()); }
QString str(const QJsonObject &o, const char *key) { return o[QLatin1String(key)].toString(); }
QByteArray fileBytes(const QString &path) {
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) fail("No se puede leer: " + path);
    return f.readAll();
}
void bytes(const QString &path, const QByteArray &data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
        fail("No se puede guardar: " + path);
}
bool compatible(const QJsonObject &v, const QString &game, const QString &loader) {
    return v["game_versions"].toArray().contains(game) && v["loaders"].toArray().contains(loader);
}
}
ModRepository::ModRepository(QString root, Transport transport) : m_root(std::move(root)), m_transport(std::move(transport)) {}
bool ModRepository::safeName(const QString &n) {
    return !n.isEmpty() && n != "." && n != ".." && !n.contains('/') && !n.contains('\\')
        && !n.contains(':') && !n.contains(QChar::Null) && !n.endsWith('.') && !n.endsWith(' ');
}
QJsonObject ModRepository::read(const QString &path) {
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(fileBytes(path), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) fail("JSON inválido: " + path);
    return doc.object();
}
void ModRepository::write(const QString &path, const QJsonObject &o) { bytes(path, QJsonDocument(o).toJson()); }
QByteArray ModRepository::fetch(const QUrl &url, const QMap<QByteArray,QByteArray> &headers) {
    if (url.scheme() != "https") fail("La descarga requiere HTTPS: " + url.host());
    QNetworkAccessManager nam;
    QString lastError;
    // Mojang occasionally closes the metadata connection while the launcher is
    // starting. Retry transient transport failures before falling back to the
    // cached manifest, instead of surfacing "Operation canceled" immediately.
    for (int attempt = 0; attempt < 3; ++attempt) {
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, "EBALIA-Launcher/1.1");
        for(auto it=headers.begin();it!=headers.end();++it)req.setRawHeader(it.key(),it.value());
        req.setTransferTimeout(60000);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, headers.isEmpty()?QNetworkRequest::NoLessSafeRedirectPolicy:QNetworkRequest::ManualRedirectPolicy);
        auto *reply = nam.get(req);
        QEventLoop loop; QTimer timer; timer.setSingleShot(true);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timer, &QTimer::timeout, reply, &QNetworkReply::abort);
        timer.start(70000); loop.exec();
        if (reply->error() == QNetworkReply::NoError) return reply->readAll();
        lastError = reply->errorString();
        reply->deleteLater();
        if (attempt < 2) QThread::msleep(400 * (attempt + 1));
    }
    fail("Error de red (" + url.host() + "): " + lastError);
}
QByteArray ModRepository::get(const QUrl &url) { return m_transport ? m_transport(url) : fetch(url); }
QJsonDocument ModRepository::api(const QString &path, const QList<QPair<QString, QString>> &items) {
    QUrl url("https://api.modrinth.com/v2/" + path); QUrlQuery query;
    for (const auto &p : items) query.addQueryItem(p.first, p.second);
    url.setQuery(query);
    QJsonParseError error; auto doc = QJsonDocument::fromJson(get(url), &error);
    if (error.error != QJsonParseError::NoError || doc.isNull()) fail("Respuesta inválida de Modrinth.");
    return doc;
}
QJsonArray ModRepository::search(const QString &q, const QString &game, const QString &loader, int offset, const QString &provider) {
    if (loader == "vanilla") fail("Elegí una instancia Fabric, Quilt, Forge o NeoForge para instalar mods.");
    if(provider=="curseforge") {
        int type=loader=="forge"?1:loader=="fabric"?4:loader=="quilt"?5:loader=="neoforge"?6:0;
        auto data=curse("mods/search",{{"gameId","432"},{"classId","6"},{"gameVersion",game},{"modLoaderType",QString::number(type)},
            {"searchFilter",q},{"sortField","2"},{"sortOrder","desc"},{"index",QString::number(offset)},{"pageSize","30"}}).object()["data"].toArray();
        QJsonArray hits;for(const auto &v:data){auto o=v.toObject();QStringList authors;for(const auto &a:o["authors"].toArray())authors<<a.toObject()["name"].toString();
            hits.append(QJsonObject{{"project_id","cf-"+QString::number(o["id"].toInteger())},{"title",o["name"]},{"description",o["summary"]},{"author",authors.join(", ")},{"provider","curseforge"},{"slug",o["slug"]},{"website",o["links"].toObject()["websiteUrl"]},{"icon_url",o["logo"].toObject()["thumbnailUrl"]}});
        }return hits;
    }
    QJsonArray facets{QJsonArray{"project_type:mod"}, QJsonArray{"versions:" + game}, QJsonArray{"categories:" + loader}};
    return api("search", {{"query", q}, {"facets", QString::fromUtf8(QJsonDocument(facets).toJson(QJsonDocument::Compact))},
        {"limit", "30"}, {"offset", QString::number(offset)}}).object()["hits"].toArray();
}
QJsonDocument ModRepository::curse(const QString &path,const QList<QPair<QString,QString>> &items) {
    QUrl url("https://api.curseforge.com/v1/"+path);QUrlQuery query;for(const auto &item:items)query.addQueryItem(item.first,item.second);url.setQuery(query);
    QByteArray data;
    if(m_transport)data=m_transport(url);
    else {
        auto key=curseForgeKey();
        if(key.isEmpty())fail("CurseForge requires your EBALIA API key in Provider settings.");
        data=fetch(url,{{"x-api-key",key.toUtf8()}});
    }
    QJsonParseError error;auto doc=QJsonDocument::fromJson(data,&error);if(error.error!=QJsonParseError::NoError||!doc.isObject())fail("Invalid CurseForge response.");return doc;
}
QJsonObject ModRepository::curseVersion(const QJsonObject &file,const QString &game,const QString &loader) {
    auto id=QString::number(file["id"].toInteger()),project="cf-"+QString::number(file["modId"].toInteger());
    QJsonArray loaders;for(const auto &v:file["gameVersions"].toArray())if(v.toString().compare(loader,Qt::CaseInsensitive)==0)loaders.append(loader);
    QJsonArray dependencies;for(const auto &v:file["dependencies"].toArray()){auto d=v.toObject();int relation=d["relationType"].toInt();if(relation==3||relation==5)dependencies.append(QJsonObject{{"project_id","cf-"+QString::number(d["modId"].toInteger())},{"dependency_type",relation==3?"required":"incompatible"}});}
    QJsonObject hashes;for(const auto &v:file["hashes"].toArray())if(v.toObject()["algo"].toInt()==1)hashes["sha1"]=v.toObject()["value"].toString().toLower();
    if(file["downloadUrl"].toString().isEmpty())fail("Manual download required by the author: https://www.curseforge.com/minecraft/mc-mods/"+QString::number(file["modId"].toInteger())+"/files/"+id);
    QJsonObject asset{{"filename",file["fileName"]},{"url",file["downloadUrl"]},{"hashes",hashes},{"primary",true}};
    return {{"id",project+"-"+id},{"project_id",project},{"name",file["displayName"]},{"game_versions",file["gameVersions"]},{"loaders",loaders},
        {"version_type",file["releaseType"].toInt()==1?"release":"beta"},{"date_published",file["fileDate"]},{"dependencies",dependencies},{"files",QJsonArray{asset}}};
}
void ModRepository::resolve(const QString &project, const QString &version, const QString &game,
                           const QString &loader, QMap<QString, QJsonObject> &selected, QSet<QString> &visiting) {
    if (selected.size() > 200 || visiting.size() > 100) fail("Demasiadas dependencias en el pack.");
    if (version.isEmpty() && selected.contains(project)) return;
    QJsonObject v;
    if(project.startsWith("cf-")||version.startsWith("cf-")) {
        QString projectNumber=project.mid(3);
        if(!version.isEmpty()) {
            auto parts=version.split('-');if(parts.size()!=3)fail("Invalid CurseForge file reference.");projectNumber=parts[1];
            v=curseVersion(curse("mods/"+projectNumber+"/files/"+parts[2]).object()["data"].toObject(),game,loader);
        } else {
            int type=loader=="forge"?1:loader=="fabric"?4:loader=="quilt"?5:loader=="neoforge"?6:0;
            auto files=curse("mods/"+projectNumber+"/files",{{"gameVersion",game},{"modLoaderType",QString::number(type)},{"pageSize","50"}}).object()["data"].toArray();
            QList<QJsonObject> candidates;QString blocked;
            for(const auto &f:files)try{auto candidate=curseVersion(f.toObject(),game,loader);if(compatible(candidate,game,loader))candidates<<candidate;}catch(const std::exception &e){blocked=QString::fromUtf8(e.what());}
            std::sort(candidates.begin(),candidates.end(),[](const auto &a,const auto &b){if((str(a,"version_type")=="release")!=(str(b,"version_type")=="release"))return str(a,"version_type")=="release";return str(a,"date_published")>str(b,"date_published");});
            if(candidates.isEmpty())fail(blocked.isEmpty()?project+": no compatible CurseForge file for "+game+" / "+loader:blocked);v=candidates.first();
        }
    }
    else if (!version.isEmpty()) v = api("version/" + version).object();
    else {
        auto versions = api("project/" + project + "/version", {
            {"game_versions", QString::fromUtf8(QJsonDocument(QJsonArray{game}).toJson(QJsonDocument::Compact))},
            {"loaders", QString::fromUtf8(QJsonDocument(QJsonArray{loader}).toJson(QJsonDocument::Compact))}}).array();
        // Prefer the newest stable release; the API's publication order is not a semantic version order.
        QList<QJsonObject> candidates;
        for (const auto &x : versions) if (compatible(x.toObject(), game, loader)) candidates << x.toObject();
        std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
            if ((str(a,"version_type") == "release") != (str(b,"version_type") == "release")) return str(a,"version_type") == "release";
            return str(a,"date_published") > str(b,"date_published");
        });
        if (candidates.isEmpty()) fail(project + ": sin edición compatible con " + game + " / " + loader);
        v = candidates.first();
    }
    auto id = str(v, "project_id");
    if (id.isEmpty() || str(v,"id").isEmpty() || (!project.isEmpty() && project != id) || !compatible(v, game, loader))
        fail(project + ": dependencia incompatible.");
    if (selected.contains(id)) {
        if (str(selected[id],"id") != str(v,"id")) fail(id + ": dos mods requieren versiones diferentes de la misma dependencia.");
        return;
    }
    if (visiting.contains(id)) return;
    visiting.insert(id);
    QJsonObject file;
    for (const auto &fv : v["files"].toArray()) {
        auto f = fv.toObject();
        if (str(f,"filename").endsWith(".jar") && (file.isEmpty() || f["primary"].toBool())) file = f;
        if (!file.isEmpty() && file["primary"].toBool()) break;
    }
    if (!safeName(str(file,"filename")) || !str(file,"filename").endsWith(".jar") || (str(file["hashes"].toObject(),"sha512").size() != 128 && str(file["hashes"].toObject(),"sha1").size() != 40))
        fail(id + ": archivo JAR o hash inválido.");
    v["file"] = file;
    selected[id] = v;
    for (const auto &dv : v["dependencies"].toArray()) {
        auto d = dv.toObject();
        if (str(d,"dependency_type") == "required") {
            if (str(d,"project_id").isEmpty() && str(d,"version_id").isEmpty()) fail(id + ": dependencia sin identificador.");
            resolve(str(d,"project_id"), str(d,"version_id"), game, loader, selected, visiting);
        }
    }
    visiting.remove(id);
}
QJsonObject ModRepository::plan(const QJsonArray &projects, const QString &game, const QString &loader, const QJsonArray &installed) {
    if (game.isEmpty() || !QStringList{"fabric","quilt","forge","neoforge"}.contains(loader)) fail("Los packs de mods requieren Fabric, Quilt, Forge o NeoForge.");
    QMap<QString,QJsonObject> selected; QJsonArray missing, roots;
    QSet<QString> requested;
    for(const auto &p:projects) requested.insert(str(p.toObject(),"project_id"));
    // Preserve installed projects not explicitly replaced and enforce their pinned dependencies.
    for(const auto &entry:installed) {
        auto item=entry.toObject();if(requested.contains(str(item,"project_id")))continue;
        QSet<QString> visiting;resolve(str(item,"project_id"),str(item,"version_id"),game,loader,selected,visiting);
    }

    for (const auto &pv : projects) {
        auto p = pv.toObject(); auto id = str(p,"project_id");
        if (!QRegularExpression("^[A-Za-z0-9_-]+$").match(id).hasMatch()) fail("Identificador de mod inválido.");
        auto candidate = selected; QSet<QString> visiting;
        try { resolve(id, {}, game, loader, candidate, visiting); selected = candidate; roots.append(p); }
        catch (const std::exception &e) { missing.append(QJsonObject{{"name", p["name"]}, {"reason", QString::fromUtf8(e.what())}}); }
    }
    QSet<QString> names;
    for (const auto &v : selected) {
        auto filename = str(v["file"].toObject(),"filename").toCaseFolded();
        if (names.contains(filename)) fail("Dos proyectos publican el mismo nombre de archivo.");
        names.insert(filename);
        for (const auto &dv : v["dependencies"].toArray()) {
            auto d = dv.toObject(); if (str(d,"dependency_type") != "incompatible") continue;
            for (const auto &other : selected)
                if ((!str(d,"version_id").isEmpty() && str(d,"version_id") == str(other,"id")) ||
                    (str(d,"version_id").isEmpty() && str(d,"project_id") == str(other,"project_id")))
                    fail("El pack contiene mods declarados incompatibles entre sí.");
        }
    }
    QJsonArray versions; for (const auto &v : selected) versions.append(v);
    return {{"game", game}, {"loader", loader}, {"versions", versions}, {"missing", missing}, {"roots", roots}};
}
void ModRepository::apply(const QString &instance, const QJsonObject &plan) {
    auto info = read(instance + "/instance.json");
    if (info["mcVersion"] != plan["game"] || info["loader"] != plan["loader"]) fail("Cambió la versión de la instancia; calculá otra vez el pack.");
    QLockFile lock(instance + "/mods.lock"); if (!lock.tryLock()) fail("Ya hay otra operación de mods en esta instancia.");
    const QString mods = instance + "/mods"; QDir().mkpath(mods);
    if (QFileInfo(mods).isSymLink()) fail("La carpeta mods no puede ser un enlace simbólico.");
    QTemporaryDir stage(instance + "/.mods-stage-XXXXXX"); if (!stage.isValid()) fail("No se puede preparar la descarga.");
    const QString manifest = instance + "/mods.json";
    QJsonObject old = QFile::exists(manifest) ? read(manifest) : QJsonObject{};
    QJsonArray existing = old["mods"].toArray(); QSet<QString> replaced, newNames;
    QJsonArray merged; const auto versions = plan["versions"].toArray();
    // Download and verify everything before touching installed files.
    for (const auto &vv : versions) {
        auto v = vv.toObject(), f = v["file"].toObject(); auto name = str(f,"filename");
        if (!safeName(name) || !name.endsWith(".jar") || (str(f["hashes"].toObject(),"sha512").size()!=128 && str(f["hashes"].toObject(),"sha1").size()!=40)) fail("Nombre de archivo o hash inválido.");
        if (newNames.contains(name.toCaseFolded())) fail("Archivos duplicados en el pack.");
        newNames.insert(name.toCaseFolded());
        auto data = get(QUrl(str(f,"url")));
        auto expected=f["hashes"].toObject();auto algorithm=expected.contains("sha512")?QCryptographicHash::Sha512:QCryptographicHash::Sha1;
        if (QCryptographicHash::hash(data, algorithm).toHex() != expected[algorithm==QCryptographicHash::Sha512?"sha512":"sha1"].toString().toLatin1())
            fail("Falló la verificación de " + name + ". No se modificaron tus mods.");
        bytes(stage.path() + "/" + name, data);
        replaced.insert(str(v,"project_id"));
        merged.append(QJsonObject{{"project_id", v["project_id"]}, {"version_id", v["id"]},
            {"name", v["name"]}, {"filename", name}, {"sha512", QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha512).toHex())}});
    }
    QStringList obsolete;
    for (const auto &ev : existing) {
        auto e = ev.toObject(); auto name = str(e,"filename");
        if (!safeName(name)) fail("Registro de mods inválido.");
        if (replaced.contains(str(e,"project_id"))) {
            for (const QString &suffix : {QString(), QString(".disabled")}) {
                auto path = mods + "/" + name + suffix;
                if (!QFile::exists(path)) continue;
                if (QFileInfo(path).isSymLink() || QCryptographicHash::hash(fileBytes(path),QCryptographicHash::Sha512).toHex() != str(e,"sha512").toLatin1())
                    fail(name + ": cambió fuera del launcher. Retiralo o volvé a identificarlo antes de actualizar.");
                obsolete << name + suffix;
            }
        } else if (QFile::exists(mods + "/" + name) || QFile::exists(mods + "/" + name + ".disabled")) merged.append(e);
    }
    for (const auto &vv : versions) {
        auto name = str(vv.toObject()["file"].toObject(),"filename");
        if ((QFile::exists(mods + "/" + name) && !obsolete.contains(name)) || (QFile::exists(mods + "/" + name + ".disabled") && !obsolete.contains(name + ".disabled")))
            fail(name + ": ya existe un archivo local. No se sobrescribió.");
    }
    QDir().mkpath(stage.path() + "/backup"); QStringList moved, installed;
    try {
        for (const auto &name : obsolete) {
            if (!QFile::rename(mods + "/" + name, stage.path() + "/backup/" + name)) fail("No se pudo respaldar " + name);
            moved << name;
        }
        for (const auto &vv : versions) {
            auto name = str(vv.toObject()["file"].toObject(),"filename");
            if (!QFile::rename(stage.path() + "/" + name, mods + "/" + name)) fail("No se pudo instalar " + name);
            installed << name;
        }
        write(manifest, {{"schema", 1}, {"mods", merged}});
    } catch (...) {
        for (const auto &name : installed) QFile::remove(mods + "/" + name);
        bool restored = true;
        for (const auto &name : moved) restored = QFile::rename(stage.path() + "/backup/" + name, mods + "/" + name) && restored;
        if (!restored) { stage.setAutoRemove(false); fail("Recuperación incompleta. Tus originales están en " + stage.path() + "/backup"); }
        throw;
    }
}
QJsonObject ModRepository::capture(const QString &instance, const QString &name) {
    QJsonArray projects; QSet<QString> ids;
    const auto files = QDir(instance + "/mods").entryList({"*.jar"}, QDir::Files);
    if (files.isEmpty()) fail("Esta instancia todavía no tiene mods activos.");
    QJsonArray managed;if(QFile::exists(instance+"/mods.json"))managed=read(instance+"/mods.json")["mods"].toArray();
    for (const auto &file : files) {
        auto hash = QCryptographicHash::hash(fileBytes(instance + "/mods/" + file), QCryptographicHash::Sha512).toHex();
        QJsonObject v;
        for(const auto &entry:managed){auto e=entry.toObject();if(str(e,"filename")==file&&str(e,"sha512").toLatin1()==hash){v=e;break;}}
        if(!v.isEmpty()){auto id=str(v,"project_id");if(!ids.contains(id)){projects.append(QJsonObject{{"project_id",id},{"name",v["name"]}});ids.insert(id);}continue;}
        try { v = api("version_file/" + QString::fromLatin1(hash), {{"algorithm", "sha512"}}).object(); }
        catch (const std::exception &) { fail(file + ": no se pudo identificar en Modrinth. El pack no se guardó; conservamos el archivo local."); }
        auto id = str(v,"project_id"); if (id.isEmpty()) fail("No se identificó " + file);
        if (!ids.contains(id)) { projects.append(QJsonObject{{"project_id", id}, {"name", v["name"]}}); ids.insert(id); }
    }
    return {{"schema", 1}, {"name", name.trimmed()}, {"projects", projects}};
}
QString ModRepository::savePack(const QJsonObject &pack) {
    if (pack["schema"].toInt() != 1 || str(pack,"name").trimmed().isEmpty() || pack["projects"].toArray().isEmpty()) fail("Pack vacío o formato inválido.");
    for (const auto &pv : pack["projects"].toArray())
        if (!QRegularExpression("^[A-Za-z0-9_-]+$").match(str(pv.toObject(),"project_id")).hasMatch()) fail("Proyecto inválido en el pack.");
    auto path = m_root + "/packs/" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".json";
    write(path, pack); return path;
}
QJsonArray ModRepository::packs() const {
    QJsonArray out; QDir dir(m_root + "/packs");
    for (const auto &file : dir.entryList({"*.json"}, QDir::Files)) {
        auto p = read(dir.filePath(file)); p["path"] = dir.filePath(file); out.append(p);
    }
    return out;
}
QString ModRepository::curseForgeKey(){
    auto key=qEnvironmentVariable("EBALIA_CURSEFORGE_API_KEY").trimmed();if(key.isEmpty())key=QSettings().value("integrations/curseforgeKey").toString().trimmed();
    return key.isEmpty()?QString::fromLatin1(EBALIA_BUILTIN_CURSEFORGE_KEY).trimmed():key;
}
