#include "PackService.hpp"
#include "Archive.hpp"
#include "Download.hpp"
#include "ModCompat.hpp"
#include "McInstanceManager.hpp"
#include <QtCore>
#include <QImage>
#include <QXmlStreamReader>
#include <QtConcurrent>
#include <stdexcept>
namespace {
const QString atl="https://download.nodecdn.net/containers/atl/",ftb="https://api.feed-the-beast.com/v1/modpacks/public",legacy="https://dist.creeper.host/FTB2/",technic="https://api.technicpack.net/";
[[noreturn]]void fail(QString message){throw std::runtime_error(message.toStdString());}
QString enc(QString s){return QString::fromLatin1(QUrl::toPercentEncoding(s));}
QString safeName(QString name){if(!ModRepository::safeName(name))fail("Invalid file name in modpack");return name;}
QJsonObject fileHashes(QJsonObject o){auto hashes=o["hashes"].toObject();for(auto key:{"sha512","sha256","sha1","md5"})if(o[key].isString())hashes[key]=o[key];return hashes;}
void write(const QString &path,QByteArray data){QDir().mkpath(QFileInfo(path).absolutePath());QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit())fail("Could not write modpack file");}
// Optional metadata: missing or invalid files read as empty.
QJsonObject json(const QString &path){try{return QFileInfo(path).isFile()?ModRepository::read(path):QJsonObject{};}catch(const std::exception &){return {};}}
// Entries that archivers add next to the packed folder (macOS Finder, Windows Explorer).
bool junk(const QString &name){return name=="__MACOSX"||name==".DS_Store"||name=="Thumbs.db"||name=="desktop.ini"||name.startsWith("._");}
const QStringList junkNames{"__MACOSX",".DS_Store","Thumbs.db","desktop.ini"};
// Game folders: a pack is never wrapped inside one of these.
const QStringList gameFolders{"minecraft",".minecraft","mods","overrides","client-overrides","config","saves","bin","resourcepacks","shaderpacks","coremods","kubejs","scripts","defaultconfigs"};
bool hasPackMetadata(const QString &dir){
    for(auto name:{"modrinth.index.json","mmc-pack.json","instance.cfg","minecraftinstance.json","bin/modpack.jar","bin/version.json"})if(QFileInfo::exists(dir+"/"+name))return true;
    const auto instance=json(dir+"/instance.json");
    return json(dir+"/manifest.json")["minecraft"].isObject()||instance.contains("mcVersion")||instance["launcher"].isObject()
        ||json(dir+"/config.json")["loader"].toObject().contains("mcVersion")||json(dir+"/profile.json")["metadata"].toObject().contains("game_version");
}
// One loader version format for every source: no game prefix ("1.20.1-47.2.0", NeoForge "1.20.1-47.1.106") and no game suffix on Fabric and Quilt ("0.15.11-1.20.1").
// Forge keeps the suffix of its own artifacts (1.7.10: "10.13.4.1614-1.7.10").
void normalizeLoader(QJsonObject &c){
    const auto game=c["mcVersion"].toString(),loader=c["loader"].toString();auto version=c["loaderVersion"].toString();
    if(game.isEmpty()||loader=="vanilla"||version.isEmpty())return;
    if(version.startsWith(game+"-"))version=version.mid(game.size()+1);
    if((loader=="fabric"||loader=="quilt")&&version.endsWith("-"+game))version.chop(game.size()+1);
    c["loaderVersion"]=version;
}
// Loader ids as launchers write them: "forge-47.2.0" (CurseForge), "1.20.1-forge-47.2.0" and "fabric-loader-1.20.1-0.15.11" (FTB App),
// "neoforge-1.20.1-47.1.106", or only the game version (vanilla). Needs c["mcVersion"].
void loaderId(QString id,QJsonObject &c){
    const auto game=c["mcVersion"].toString();id=id.trimmed();
    if(!game.isEmpty()&&id.startsWith(game+"-"))id=id.mid(game.size()+1);
    if(id.isEmpty()||id==game){c["loader"]="vanilla";c.remove("loaderVersion");return;}
    static const QRegularExpression known("^(neoforge|forge|fabric|quilt)(?:-loader)?-(.+)$",QRegularExpression::CaseInsensitiveOption);
    auto m=known.match(id);
    if(m.hasMatch()){c["loader"]=m.captured(1).toLower();c["loaderVersion"]=m.captured(2);}else{c["loader"]=id.section('-',0,0).toLower();c["loaderVersion"]=id.section('-',1);}
    normalizeLoader(c);
}
// Files of the official Minecraft Launcher: what it downloads again, and account files that are never copied.
const QStringList launcherFiles{"versions","libraries","assets","runtime","webcache2","logs","crash-reports","launcher_profiles.json","launcher_settings.json","launcher_ui_state.json","launcher_log.txt","launcher_cef_log.txt","launcher_entitlements.json","launcher_gamer_pics.json","launcher_product_state.json","launcher_skins.json","treatment_tags.json","usercache.json"};
const QStringList accountFiles{"launcher_accounts.json","launcher_accounts_microsoft_store.json","launcher_msa_credentials.bin","launcher_msa_credentials_microsoft_store.bin","launcher_msa_credentials.json"};
// FTB App instances keep the loader target in .ftbapp/version.json (older ones in version.json).
void ftbTargets(const QString &dir,QJsonObject &c){
    for(auto path:{".ftbapp/version.json","version.json"})for(auto v:json(dir+"/"+path)["targets"].toArray())if(v.toObject()["type"]=="modloader"){c["loader"]=v.toObject()["name"].toString().toLower();c["loaderVersion"]=v.toObject()["version"];return;}
}
}
QString PackService::safePath(QString path){path.replace('\\','/');while(path.startsWith("./"))path=path.mid(2);if(path.isEmpty()||QDir::isAbsolutePath(path)||path.contains(':')||path.split('/').contains(".."))fail("Unsafe modpack path: "+path);return path;}
QByteArray PackService::bytes(const QString &url,bool curse){if(m_transport)return m_transport(QUrl(url));QMap<QByteArray,QByteArray> headers;if(curse){auto key=ModRepository::curseForgeKey();if(key.isEmpty())fail("CurseForge requires your EBALIA API key in Provider settings.");headers["x-api-key"]=key.toUtf8();}return ModRepository::fetch(QUrl(url),headers);}
QJsonDocument PackService::get(const QString &url,bool curse){QJsonParseError e;auto d=QJsonDocument::fromJson(bytes(url,curse),&e);if(e.error!=QJsonParseError::NoError)fail("Invalid modpack catalog");return d;}
void PackService::download(const QString &url,const QString &destination,const QJsonObject &hashes){
    QCryptographicHash::Algorithm algorithm=QCryptographicHash::Sha256;QByteArray expected;for(auto pair:{qMakePair("sha512",QCryptographicHash::Sha512),qMakePair("sha256",QCryptographicHash::Sha256),qMakePair("sha1",QCryptographicHash::Sha1),qMakePair("md5",QCryptographicHash::Md5)})if(!hashes[pair.first].toString().isEmpty()){algorithm=pair.second;expected=hashes[pair.first].toString().toLatin1();break;}
    if(m_transport){auto data=bytes(url);if(!expected.isEmpty()&&QCryptographicHash::hash(data,algorithm).toHex()!=expected.toLower())fail("Modpack checksum mismatch");write(destination,data);}else Download::file(QUrl(url),destination,expected,algorithm);
}
QJsonArray PackService::search(const QString &provider,const QString &query,int page){
    QJsonArray result;auto add=[&](QString id,QString name,QString description,QJsonObject extra=QJsonObject{}){extra["provider"]=provider;extra["id"]=id;extra["name"]=name;extra["description"]=description;result.append(extra);};
    if(provider=="modrinth"){auto d=get("https://api.modrinth.com/v2/search?facets="+enc("[[\"project_type:modpack\"]]")+"&query="+enc(query)+"&limit=30&offset="+QString::number(page*30)).object();for(auto v:d["hits"].toArray()){auto p=v.toObject();add(p["project_id"].toString(),p["title"].toString(),p["description"].toString(),{{"icon",p["icon_url"]},{"author",p["author"]},{"downloads",p["downloads"]},{"url","https://modrinth.com/modpack/"+p["slug"].toString()}});}}
    else if(provider=="curseforge"){auto d=get("https://api.curseforge.com/v1/mods/search?gameId=432&classId=4471&searchFilter="+enc(query)+"&pageSize=30&index="+QString::number(page*30),true).object();for(auto v:d["data"].toArray()){auto p=v.toObject();QJsonObject extra{{"icon",p["logo"].toObject()["thumbnailUrl"]},{"downloads",p["downloadCount"]},{"url",p["links"].toObject()["websiteUrl"]}};if(!p["authors"].toArray().isEmpty())extra["author"]=p["authors"][0].toObject()["name"];add(QString::number(p["id"].toInteger()),p["name"].toString(),p["summary"].toString(),extra);}}
    else if(provider=="atlauncher"){for(auto v:get(atl+"launcher/json/packsnew.json").array()){auto p=v.toObject();if(p["type"]!="public"||!p["name"].toString().contains(query,Qt::CaseInsensitive))continue;auto name=p["name"].toString();auto slug=name;slug.remove(QRegularExpression("[^a-zA-Z0-9]"));add(slug,name,p["description"].toString(),{{"versions",p["versions"]},{"icon","https://cdn.atlcdn.net/images/packs/"+slug.toLower()+".png"},{"url",p["websiteURL"]}});}}
    else if(provider=="ftb"){auto ids=get(ftb+"/modpack/all").object()["packs"].toArray();QList<QJsonObject> packs;QMutex mutex;QThreadPool pool;pool.setMaxThreadCount(6);QList<QFuture<void>> jobs;for(auto id:ids){auto key=QString::number(id.toInteger());jobs<<QtConcurrent::run(&pool,[&,key]{auto cache=m_root+"/cache/ftb/"+key+".json";QJsonObject p;try{if(QFileInfo(cache).lastModified().secsTo(QDateTime::currentDateTime())<3600&&QFile::exists(cache))p=ModRepository::read(cache);else{p=get(ftb+"/modpack/"+key).object();ModRepository::write(cache,p);}}catch(...){return;}QMutexLocker lock(&mutex);packs<<p;});}for(auto &job:jobs)job.waitForFinished();std::sort(packs.begin(),packs.end(),[](auto a,auto b){return a["name"].toString()<b["name"].toString();});for(auto p:packs)if(p["name"].toString().contains(query,Qt::CaseInsensitive)){QJsonObject extra{{"versions",p["versions"]},{"body",p["description"]},{"bodyFormat","markdown"},{"downloads",p["installs"]}};for(auto art:p["art"].toArray())if(art.toObject()["type"]=="square")extra["icon"]=art.toObject()["url"];if(!p["authors"].toArray().isEmpty())extra["author"]=p["authors"][0].toObject()["name"];add(QString::number(p["id"].toInteger()),p["name"].toString(),p["synopsis"].toString(),extra);}}
    else if(provider=="legacy_ftb"){for(auto file:{"modpacks.xml","thirdparty.xml"}){QXmlStreamReader xml(bytes(legacy+"static/"+file));while(!xml.atEnd()){xml.readNext();if(xml.isStartElement()&&xml.name()==QLatin1String("modpack")){auto a=xml.attributes();auto name=a.value("name").toString();if(!name.contains(query,Qt::CaseInsensitive))continue;QJsonArray builds;auto names=a.value("oldVersions").toString().split(';',Qt::SkipEmptyParts);names.prepend(a.value("version").toString());names.removeDuplicates();for(auto v:names)if(!v.isEmpty())builds.append(QJsonObject{{"id",v},{"name",v}});QJsonObject extra{{"file",a.value("url").toString()},{"mcVersion",a.value("mcVersion").toString()},{"versions",builds},{"author",a.value("author").toString()},{"body",a.value("description").toString()},{"bodyFormat","html"}};auto logo=a.value("logo").toString();if(ModRepository::safeName(logo))extra["icon"]=legacy+"static/"+logo;add(a.value("dir").toString(),name,a.value("mcVersion").toString(),extra);}}if(xml.hasError())fail("Invalid FTB Legacy catalog");}}
    else if(provider=="technic"){auto d=get(technic+(query.isEmpty()?"trending?build=999":"search?build=999&q="+enc(query))).object();for(auto v:d["modpacks"].toArray()){auto p=v.toObject();add(p["slug"].toString(),p["name"].toString(),{},{{"icon",p["iconUrl"]},{"url",p["url"]}});}}
    else fail("Unknown modpack provider");return result;
}
QJsonArray PackService::versions(const QJsonObject &pack){
    auto provider=pack["provider"].toString(),id=pack["id"].toString();QJsonArray out;
    if(provider=="modrinth"){for(auto v:get("https://api.modrinth.com/v2/project/"+enc(id)+"/version").array()){auto o=v.toObject();auto games=o["game_versions"].toArray();out.append(QJsonObject{{"id",o["id"]},{"name",o["version_number"].toString()+(games.isEmpty()?QString():" · Minecraft "+games.last().toString())+(o["version_type"]=="release"?QString():" · "+o["version_type"].toString())},{"data",o}});}}
    else if(provider=="curseforge"){for(auto v:get("https://api.curseforge.com/v1/mods/"+enc(id)+"/files?pageSize=50",true).object()["data"].toArray()){auto o=v.toObject();out.append(QJsonObject{{"id",QString::number(o["id"].toInteger())},{"name",o["displayName"]},{"data",o}});}}
    else if(provider=="technic"){auto p=get(technic+"modpack/"+enc(id)+"?build=999").object();auto solder=p["solder"].toString();if(solder.isEmpty())out.append(QJsonObject{{"id",p["version"]},{"name",p["version"]},{"data",p}});else{if(!solder.endsWith('/'))solder+='/';auto meta=get(solder+"modpack/"+enc(id)).object();auto builds=meta["builds"].toArray();auto recommended=meta["recommended"].toString();if(builds.contains(recommended)){for(int i=0;i<builds.size();++i)if(builds[i]==recommended){builds.removeAt(i);break;}builds.prepend(recommended);}for(auto b:builds)out.append(QJsonObject{{"id",b},{"name",b},{"data",p},{"solder",solder}});}}
    else{auto list=pack["versions"].toArray();if(provider=="ftb"){QJsonArray reversed;for(auto v:list)reversed.prepend(v);list=reversed;}for(auto v:list){const auto o=v.toObject();auto id=o["version"].toString(o["id"].toVariant().toString());out.append(QJsonObject{{"id",id},{"name",o["name"].toString(id)},{"data",o}});}}return out;
}
void PackService::copyTree(const QString &from,const QString &to,const QStringList &skip){
    // Instance folders on disk often link shared folders (saves, resourcepacks): copy what the link points to. Archives never hold links (Archive::extract refuses them).
    QDirIterator it(from,QDir::Files|QDir::Dirs|QDir::Hidden|QDir::NoDotAndDotDot,QDirIterator::Subdirectories|QDirIterator::FollowSymlinks);while(it.hasNext()){auto path=it.next();auto rel=QDir(from).relativeFilePath(path);if(rel.isEmpty()||rel.startsWith("../")||rel==".."||QDir::isAbsolutePath(rel))fail("Unsafe modpack path: "+rel);if(skip.contains(rel.section('/',0,0)))continue;auto target=to+"/"+rel;if(it.fileInfo().isDir()){QDir().mkpath(target);continue;}QFile source(path);if(!source.open(QIODevice::ReadOnly))fail("Could not read imported file");QDir().mkpath(QFileInfo(target).absolutePath());QSaveFile dest(target);if(!dest.open(QIODevice::WriteOnly))fail("Could not write imported file");while(!source.atEnd()){auto data=source.read(1024*1024);if(data.isEmpty()&&source.error()!=QFile::NoError)fail("Read error");if(dest.write(data)!=data.size())fail("Copy error");}if(!dest.commit())fail("Copy error");}}
void PackService::profileLoader(const QJsonObject &profile,QJsonObject &config){
    if(!profile["inheritsFrom"].toString().isEmpty())config["mcVersion"]=profile["inheritsFrom"];
    bool neoforged=false;
    for(auto v:profile["libraries"].toArray()){
        auto name=v.toObject()["name"].toString().section('@',0,0);auto coordinate=name.section(':',0,1),version=name.section(':',2,2);QString loader;
        if(coordinate.startsWith("net.neoforged"))neoforged=true;
        if(coordinate=="net.minecraftforge:forge"||coordinate=="net.minecraftforge:minecraftforge"||coordinate=="net.minecraftforge:fmlloader")loader="forge";else if(coordinate=="net.neoforged:neoforge"||coordinate=="net.neoforged:forge")loader="neoforge";else if(coordinate=="net.fabricmc:fabric-loader")loader="fabric";else if(coordinate=="org.quiltmc:quilt-loader")loader="quilt";
        if(!loader.isEmpty()){config["loader"]=loader;if((loader=="forge"||loader=="neoforge")&&version.startsWith(config["mcVersion"].toString()+"-"))version=version.mid(config["mcVersion"].toString().size()+1);config["loaderVersion"]=version;}
    }
    // Modern Forge and NeoForge profiles name their loader in the game arguments (NeoForge for 1.20.1 still says "forgeVersion").
    QStringList args;for(auto v:profile["arguments"].toObject()["game"].toArray())if(v.isString())args<<v.toString();
    args<<profile["minecraftArguments"].toString().split(' ',Qt::SkipEmptyParts);
    auto value=[&](const QString &key){auto i=args.indexOf(key);return i>=0&&i+1<args.size()?args[i+1]:QString();};
    if(config["mcVersion"].toString().isEmpty()&&!value("--fml.mcVersion").isEmpty())config["mcVersion"]=value("--fml.mcVersion");
    if(config["mcVersion"].toString().isEmpty())for(auto v:profile["libraries"].toArray()){const auto name=v.toObject()["name"].toString();if(name.startsWith("net.fabricmc:intermediary:")||name.startsWith("org.quiltmc:hashed:")){config["mcVersion"]=name.section(':',2,2);break;}}
    if(!value("--fml.neoForgeVersion").isEmpty()){config["loader"]="neoforge";config["loaderVersion"]=value("--fml.neoForgeVersion");}
    else if(!value("--fml.forgeVersion").isEmpty()){config["loader"]=neoforged||value("--fml.forgeGroup")=="net.neoforged"?"neoforge":"forge";config["loaderVersion"]=value("--fml.forgeVersion");}
}
QJsonObject PackService::ftbManifest(const QJsonObject &d,const QString &stage){
    QJsonObject c{{"loader","vanilla"}};for(auto v:d["targets"].toArray()){auto t=v.toObject();if(t["type"]=="game")c["mcVersion"]=t["version"];if(t["type"]=="modloader"){c["loader"]=t["name"].toString().toLower();c["loaderVersion"]=t["version"];}}
    for(auto v:d["files"].toArray()){auto f=v.toObject();if(f["serveronly"].toBool()||f["optional"].toBool())continue;auto path=f["path"].toString();while(path.startsWith("./"))path=path.mid(2);if(path==".")path.clear();path=(path.isEmpty()?QString():path+"/")+safeName(f["name"].toString());download(f["url"].toString(),stage+"/"+safePath(path),fileHashes(f));}return c;
}
QStringList PackService::pastedEntries(const QString &instancesDir){
    QStringList out;
    for(const auto &e:QDir(instancesDir).entryInfoList(QDir::Dirs|QDir::Files|QDir::NoDotAndDotDot,QDir::Name)){
        if(e.fileName().startsWith('.')||e.isSymLink())continue; // the launcher's own temporary folders start with a dot
        // An EBALIA instance with a damaged instance.json is not someone else's folder.
        if(e.isDir()&&(QFile::exists(e.filePath()+"/launch-profile.json")||(QFile::exists(e.filePath()+"/instance.json")&&json(e.filePath()+"/instance.json").isEmpty())))continue;
        // EBALIA always writes "loader" in instance.json; FTB App and ATLauncher instance.json files do not.
        if(e.isDir()?!json(e.filePath()+"/instance.json").contains("loader"):QStringList{"zip","mrpack"}.contains(e.suffix().toLower()))out<<e.filePath();
    }
    return out;
}
QString PackService::adopt(const QString &path){
    const QFileInfo source(path);QString name=source.isDir()?source.fileName():source.completeBaseName();int memory=4096;
    // Prism Launcher, MultiMC and PolyMC keep the name and memory in instance.cfg.
    if(QFile::exists(path+"/instance.cfg")){QSettings cfg(path+"/instance.cfg",QSettings::IniFormat);name=cfg.value("name",name).toString();if(cfg.value("OverrideMemory").toBool())memory=cfg.value("MaxMemAlloc",memory).toInt();}
    else if(const auto o=json(path+"/instance.json");!o["name"].toString().isEmpty())name=o["name"].toString();
    else if(const auto o=json(path+"/minecraftinstance.json");!o["name"].toString().isEmpty())name=o["name"].toString();
    auto dir=install(QJsonObject{{"provider","import"},{"path",path}},{},name,{},memory);
    const auto trash=m_root+"/mc/trash";QDir().mkpath(trash);
    if(!QDir().rename(path,trash+"/pasted-"+source.fileName()+"-"+QString::number(QDateTime::currentMSecsSinceEpoch())))
        if(!QFile::moveToTrash(path)){QDir(dir).removeRecursively();fail("The pasted folder is in use by another program. Close it and the launcher will try again.");}
    // The pasted folder held the name: take it now that it is free.
    const auto named=McInstanceManager::folderFor(QFileInfo(dir).absolutePath(),name,dir);
    return named!=QFileInfo(dir).absoluteFilePath()&&QDir().rename(dir,named)?named:dir;
}
QStringList PackService::releases(){
    // Mojang's release list, newest first, as McInstanceManager keeps it; downloaded when the launcher has not yet.
    QJsonObject manifest=json(m_root+"/mc/manifest.json");
    if(manifest.isEmpty())try{manifest=QJsonDocument::fromJson(bytes("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json")).object();}catch(...){}
    QStringList out;for(const auto &v:manifest["versions"].toArray())if(v.toObject()["type"]=="release")out<<v.toObject()["id"].toString();
    return out;
}
QJsonObject PackService::unpack(const QString &archive,const QString &stage){QTemporaryDir temp;Archive::extract(archive,temp.path());return importFolder(temp.path(),stage);}
QJsonObject PackService::importFolder(QString base,const QString &stage){
    QJsonObject c{{"loader","vanilla"}};
    // Exports and hand-made ZIPs often wrap the instance in one or two folders, next to what macOS Finder or Windows Explorer add.
    for(int depth=0;depth<4&&!hasPackMetadata(base);++depth){
        QFileInfoList top;for(const auto &e:QDir(base).entryInfoList(QDir::Dirs|QDir::Files|QDir::Hidden|QDir::NoDotAndDotDot))if(!junk(e.fileName()))top<<e;
        if(top.size()!=1||!top.first().isDir()||top.first().isSymLink()||gameFolders.contains(top.first().fileName()))break;
        base=top.first().filePath();
    }
    if(QFile::exists(base+"/modrinth.index.json")){
        auto d=ModRepository::read(base+"/modrinth.index.json");if(d["formatVersion"].toInt()!=1||d["game"]!="minecraft")fail("Unsupported Modrinth pack format");
        auto deps=d["dependencies"].toObject();c["mcVersion"]=deps["minecraft"];for(auto key:{"forge","neoforge","fabric-loader","quilt-loader"})if(deps.contains(key)){c["loader"]=QString(key).section('-',0,0);c["loaderVersion"]=deps[key];}
        for(auto v:d["files"].toArray()){
            // Client "optional" files are installed, as the Modrinth App and Prism Launcher do: required mods often depend on them.
            auto f=v.toObject();if(f["env"].toObject()["client"]=="unsupported")continue;
            auto urls=f["downloads"].toArray();if(urls.isEmpty())fail("Modpack file has no download URL");auto target=stage+"/"+safePath(f["path"].toString());
            // "downloads" lists mirrors of one file: try them in order.
            QString error;bool done=false;for(auto url:urls){try{download(url.toString(),target,f["hashes"].toObject());done=true;break;}catch(const std::exception &e){error=QString::fromUtf8(e.what());}}if(!done)fail(error);
        }
        for(auto folder:{"overrides","client-overrides"})if(QDir(base+"/"+folder).exists())copyTree(base+"/"+folder,stage,junkNames);return c;
    }
    if(json(base+"/manifest.json")["minecraft"].isObject()){
        auto d=ModRepository::read(base+"/manifest.json");auto mc=d["minecraft"].toObject();c["mcVersion"]=mc["version"];
        for(auto v:mc["modLoaders"].toArray()){auto lo=v.toObject();if(!lo["primary"].toBool(true))continue;loaderId(lo["id"].toString(),c);break;}
        for(auto v:d["files"].toArray()){
            auto f=v.toObject();if(!f["required"].toBool(true))continue;auto id=QString::number(f["projectID"].toInteger()),version=QString::number(f["fileID"].toInteger());
            auto file=get("https://api.curseforge.com/v1/mods/"+id+"/files/"+version,true).object()["data"].toObject();if(file["downloadUrl"].toString().isEmpty())fail("Author requires manual download: https://www.curseforge.com/projects/"+id+"/files/"+version);
            QJsonObject hashes;for(auto h:file["hashes"].toArray())if(h.toObject()["algo"].toInt()==1)hashes["sha1"]=h.toObject()["value"];
            // Resource packs and shaders are listed next to mods; only the project's class tells them apart (jars are always mods).
            auto name=safeName(file["fileName"].toString());QString folder="mods";
            if(!name.endsWith(".jar",Qt::CaseInsensitive)){auto classId=get("https://api.curseforge.com/v1/mods/"+id,true).object()["data"].toObject()["classId"].toInt();if(classId==12)folder="resourcepacks";else if(classId==6552)folder="shaderpacks";}
            download(file["downloadUrl"].toString(),stage+"/"+folder+"/"+name,hashes);
        }
        auto overrides=safePath(d["overrides"].toString("overrides"));if(QDir(base+"/"+overrides).exists())copyTree(base+"/"+overrides,stage,junkNames);return c;
    }
    // Prism Launcher, PolyMC and MultiMC describe the game and loader as components.
    if(QFile::exists(base+"/mmc-pack.json")){
        components(ModRepository::read(base+"/mmc-pack.json"),c);
        if(c.contains("unsupported"))fail("LiteLoader instances are not supported. Use the original launcher for this instance.");
        if(!QDir(base+"/jarmods").isEmpty()&&QDir(base+"/jarmods").exists())fail("This legacy pack modifies minecraft.jar directly; its original launcher is required.");
    }
    QString game=base;if(QDir(base+"/minecraft").exists())game+="/minecraft";else if(QDir(base+"/.minecraft").exists())game+="/.minecraft";
    QStringList skip=junkNames+accountFiles;
    // A version folder of TLauncher (and other launchers built on the official format): "<name>.json" with the
    // libraries and main class next to "<name>.jar" and the game folders.
    if(c["mcVersion"].toString().isEmpty()&&!QFile::exists(base+"/instance.json")&&!QFile::exists(base+"/minecraftinstance.json")){
        QString chosen;QJsonObject profile;
        for(const auto &file:QDir(game).entryList({"*.json"},QDir::Files,QDir::Name)){
            const auto o=json(game+"/"+file);if(o["mainClass"].toString().isEmpty()||!o["libraries"].isArray())continue;
            // Prefer the profile that has its game jar beside it, then one that names a loader.
            const bool jar=QFile::exists(game+"/"+QFileInfo(file).completeBaseName()+".jar");
            if(chosen.isEmpty()||jar||(!o["inheritsFrom"].toString().isEmpty()&&profile["inheritsFrom"].toString().isEmpty())){chosen=file;profile=o;if(jar)break;}
        }
        if(!chosen.isEmpty()){
            profileLoader(profile,c);
            if(c["mcVersion"].toString().isEmpty())c["mcVersion"]=profile["inheritsFrom"].toString(profile["jar"].toString());
            if(c["mcVersion"].toString().isEmpty()&&QRegularExpression(R"(^\d+\.\d+(\.\d+)?$)").match(profile["id"].toString()).hasMatch())c["mcVersion"]=profile["id"];
            const auto name=QFileInfo(chosen).completeBaseName();
            skip<<chosen<<name+".jar"<<"TLauncherAdditional.json"<<"natives"<<"logs"<<"crash-reports"<<"usercache.json"<<"usernamecache.json";
        }
    }
    // A .minecraft folder of the official Minecraft Launcher: the version played last (or the newest, loaders first).
    if(c["mcVersion"].toString().isEmpty()&&QDir(game+"/versions").exists()&&!QFile::exists(base+"/instance.json")&&!QFile::exists(base+"/minecraftinstance.json")){
        const QDir versions(game+"/versions");QString chosen;QDateTime newest;
        for(const auto &v:json(game+"/launcher_profiles.json")["profiles"].toObject()){
            const auto p=v.toObject();const auto id=p["lastVersionId"].toString();const auto used=QDateTime::fromString(p["lastUsed"].toString(),Qt::ISODateWithMs);
            if(!ModRepository::safeName(id)||!QFile::exists(versions.filePath(id+"/"+id+".json")))continue;
            if(chosen.isEmpty()||used>newest){chosen=id;newest=used;}
        }
        if(chosen.isEmpty()){QDateTime time;bool loader=false;
            for(const auto &id:versions.entryList(QDir::Dirs|QDir::NoDotAndDotDot)){const auto file=versions.filePath(id+"/"+id+".json");if(!QFile::exists(file))continue;
                const bool isLoader=!json(file)["inheritsFrom"].toString().isEmpty();const auto modified=QFileInfo(file).lastModified();
                if(chosen.isEmpty()||(isLoader&&!loader)||(isLoader==loader&&modified>time)){chosen=id;time=modified;loader=isLoader;}}}
        if(!chosen.isEmpty()){const auto profile=json(versions.filePath(chosen+"/"+chosen+".json"));profileLoader(profile,c);if(c["mcVersion"].toString().isEmpty())c["mcVersion"]=profile["inheritsFrom"].toString(profile["id"].toString(chosen));skip<<launcherFiles;}
    }
    copyTree(game,stage,skip);
    if(c["mcVersion"].toString().isEmpty()&&QFile::exists(base+"/minecraftinstance.json")){ // CurseForge app instance folder
        const auto o=ModRepository::read(base+"/minecraftinstance.json");const auto loader=o["baseModLoader"].toObject();
        c["mcVersion"]=o["gameVersion"].toString(loader["minecraftVersion"].toString());if(!loader["name"].toString().isEmpty())loaderId(loader["name"].toString(),c);
    }
    if(c["mcVersion"].toString().isEmpty()&&QFile::exists(base+"/instance.json")){
        auto o=ModRepository::read(base+"/instance.json");
        if(o["launcher"].isObject()&&!o.contains("mcVersion")){ // ATLauncher: "id" is the game version
            c["mcVersion"]=o["id"];const auto loader=o["launcher"].toObject()["loaderVersion"].toObject();
            if(!loader["type"].toString().isEmpty()){c["loader"]=loader["type"].toString().toLower();c["loaderVersion"]=loader["version"];}
        }else{ // EBALIA export or FTB App instance
            c["mcVersion"]=o["mcVersion"];
            if(o.contains("loader")){c["loader"]=o["loader"].toString("vanilla");c["loaderVersion"]=o["loaderVersion"];}
            else loaderId(o["modLoader"].toString(),c);
            if(c["loader"]=="vanilla")ftbTargets(base,c);
        }
    }
    if(c["mcVersion"].toString().isEmpty()){ // GDLauncher
        const auto loader=json(base+"/config.json")["loader"].toObject();
        if(!loader["mcVersion"].toString().isEmpty()){c["mcVersion"]=loader["mcVersion"];auto type=loader["loaderType"].toString("vanilla").toLower();if(type!="vanilla"){c["loader"]=type;c["loaderVersion"]=loader["loaderVersion"];}}
    }
    if(c["mcVersion"].toString().isEmpty()){ // Modrinth App
        const auto meta=json(base+"/profile.json")["metadata"].toObject();
        if(!meta["game_version"].toString().isEmpty()){c["mcVersion"]=meta["game_version"];auto type=meta["loader"].toString("vanilla").toLower();if(type!="vanilla"){c["loader"]=type;c["loaderVersion"]=meta["loader_version"].toObject()["id"];}}
    }
    if(c["mcVersion"].toString().isEmpty()){ // Technic
        for(auto path:{"bin/version.json","version.json","pack.json"})if(QFile::exists(stage+"/"+path)){auto profile=ModRepository::read(stage+"/"+path);profileLoader(profile,c);}
        if(QFile::exists(stage+"/bin/modpack.jar")&&c["loader"]=="vanilla"){QTemporaryDir jar;Archive::extract(stage+"/bin/modpack.jar",jar.path());if(QFile::exists(jar.path()+"/version.json"))profileLoader(ModRepository::read(jar.path()+"/version.json"),c);else fail("This legacy pack modifies minecraft.jar directly; its original launcher is required.");}
    }
    if(c["mcVersion"].toString().isEmpty()){ // only mods: the version and loader every mod accepts
        const auto mods=ModCompat::scan(stage+"/mods");const auto found=ModCompat::detect(mods,releases());
        if(!found.minecraft.isEmpty()){c["mcVersion"]=found.minecraft;c["loader"]=found.loader;c.remove("loaderVersion");}
    }
    for(auto name:{"instance.cfg","mmc-pack.json","minecraftinstance.json","profile.json"})QFile::remove(stage+"/"+name);
    return c;
}
void PackService::components(const QJsonObject &pack,QJsonObject &c){
    static const QMap<QString,QString> loaders{{"net.minecraftforge","forge"},{"net.neoforged","neoforge"},{"net.fabricmc.fabric-loader","fabric"},{"org.quiltmc.quilt-loader","quilt"}};
    for(auto v:pack["components"].toArray()){
        const auto o=v.toObject();auto uid=o["uid"].toString();auto version=o["version"].toString(o["cachedVersion"].toString()); // const: a missing key must not be inserted mid-expression
        if(uid=="net.minecraft")c["mcVersion"]=version;else if(loaders.contains(uid)){c["loader"]=loaders[uid];c["loaderVersion"]=version;}else if(uid=="com.mumfrey.liteloader")c["unsupported"]=uid;
    }
}
QJsonArray PackService::localInstances(){
    // Only instance metadata is read here (name, game version, loader); never accounts or launcher settings beyond the instance folder.
    QJsonArray out;QSet<QString> seen;const auto home=QDir::homePath();
    auto entry=[&](const QString &path,const QString &source,QJsonObject o){auto key=QFileInfo(path).canonicalFilePath();if(key.isEmpty()||seen.contains(key))return;seen.insert(key);o["path"]=path;o["source"]=source;if(!o.contains("loader"))o["loader"]="vanilla";out.append(o);};
    QStringList data=QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
#ifdef Q_OS_WIN
    data.prepend(qEnvironmentVariable("APPDATA"));
#endif
    QList<QPair<QString,QString>> prism;
    for(const auto &base:data)if(!base.isEmpty())prism<<qMakePair(base+"/PrismLauncher",QString("Prism Launcher"))<<qMakePair(base+"/PolyMC",QString("PolyMC"))<<qMakePair(base+"/MultiMC",QString("MultiMC"));
    prism<<qMakePair(home+"/.var/app/org.prismlauncher.PrismLauncher/data/PrismLauncher",QString("Prism Launcher"));
    for(const auto &launcher:prism){
        auto folder=launcher.first+"/instances";
        for(auto name:{"prismlauncher.cfg","polymc.cfg","multimc.cfg"})if(QFileInfo::exists(launcher.first+"/"+name)){QSettings cfg(launcher.first+"/"+name,QSettings::IniFormat);auto custom=cfg.value("InstanceDir").toString();if(!custom.isEmpty())folder=QDir::isAbsolutePath(custom)?custom:launcher.first+"/"+custom;}
        for(const auto &dir:QDir(folder).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name)){
            if(!QFile::exists(dir.filePath()+"/instance.cfg")||!QFile::exists(dir.filePath()+"/mmc-pack.json"))continue;
            QSettings cfg(dir.filePath()+"/instance.cfg",QSettings::IniFormat);QJsonObject o{{"name",cfg.value("name",dir.fileName()).toString()}};
            try{components(ModRepository::read(dir.filePath()+"/mmc-pack.json"),o);}catch(...){}
            entry(dir.filePath(),launcher.second,o);
        }
    }
#ifdef Q_OS_WIN
    const QString ftbApp=qEnvironmentVariable("LOCALAPPDATA")+"/.ftba",curse=home+"/curseforge/minecraft/Instances";
#elif defined(Q_OS_MACOS)
    const QString ftbApp=home+"/Library/Application Support/.ftba",curse=home+"/Documents/curseforge/minecraft/Instances";
#else
    const QString ftbApp=home+"/.ftba",curse=home+"/curseforge/minecraft/Instances";
#endif
    QStringList ftbFolders{ftbApp+"/instances"};
    for(auto settings:{ftbApp+"/storage/settings.json",ftbApp+"/bin/settings.json"})try{auto location=ModRepository::read(settings)["instanceLocation"].toString();if(!location.isEmpty())ftbFolders.prepend(location);}catch(...){}
    for(const auto &folder:ftbFolders)for(const auto &dir:QDir(folder).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name)){
        try{auto o=ModRepository::read(dir.filePath()+"/instance.json");if(o["mcVersion"].toString().isEmpty())continue;QJsonObject e{{"name",o["name"].toString(dir.fileName())},{"mcVersion",o["mcVersion"]}};loaderId(o["modLoader"].toString(),e);if(e["loader"]=="vanilla")ftbTargets(dir.filePath(),e);entry(dir.filePath(),"FTB App",e);}catch(...){}
    }
    // The official Minecraft Launcher and launchers that share its .minecraft format (TLauncher, SKLauncher…):
    // the .minecraft folder itself when it has mods, and versions with a game folder of their own.
#ifdef Q_OS_WIN
    const QString dotMinecraft=qEnvironmentVariable("APPDATA")+"/.minecraft";
#elif defined(Q_OS_MACOS)
    const QString dotMinecraft=home+"/Library/Application Support/minecraft";
#else
    const QString dotMinecraft=home+"/.minecraft";
#endif
    auto describeProfile=[](const QString &file){QJsonObject e{{"loader","vanilla"}};const auto p=json(file);profileLoader(p,e);if(e["mcVersion"].toString().isEmpty())e["mcVersion"]=p["inheritsFrom"].toString(p["jar"].toString(p["id"].toString()));normalizeLoader(e);return e;};
    const bool tlauncher=QFileInfo::exists(dotMinecraft+"/TlauncherProfiles.json")||QFileInfo::exists(dotMinecraft+"/tlauncher_profiles.json");
    for(const auto &dir:QDir(dotMinecraft+"/versions").entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name)){
        const auto file=dir.filePath()+"/"+dir.fileName()+".json";
        if(!QFile::exists(file)||!(QDir(dir.filePath()+"/mods").exists()||QDir(dir.filePath()+"/saves").exists()))continue;
        try{auto e=describeProfile(file);if(e["mcVersion"].toString().isEmpty())continue;e["name"]=dir.fileName();entry(dir.filePath(),tlauncher?"TLauncher":"Minecraft Launcher",e);}catch(...){}
    }
    if(QDir(dotMinecraft+"/mods").exists()&&!QDir(dotMinecraft+"/mods").isEmpty()){
        // Same choice as the import: the version played last.
        QString chosen;QDateTime newest;
        for(const auto &v:json(dotMinecraft+"/launcher_profiles.json")["profiles"].toObject()){const auto p=v.toObject();const auto id=p["lastVersionId"].toString();const auto used=QDateTime::fromString(p["lastUsed"].toString(),Qt::ISODateWithMs);
            if(ModRepository::safeName(id)&&QFile::exists(dotMinecraft+"/versions/"+id+"/"+id+".json")&&(chosen.isEmpty()||used>newest)){chosen=id;newest=used;}}
        if(!chosen.isEmpty())try{auto e=describeProfile(dotMinecraft+"/versions/"+chosen+"/"+chosen+".json");e["name"]=QString(".minecraft · ")+chosen;entry(dotMinecraft,tlauncher?"TLauncher":"Minecraft Launcher",e);}catch(...){}
    }
    for(const auto &dir:QDir(curse).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name)){
        try{auto o=ModRepository::read(dir.filePath()+"/minecraftinstance.json");QJsonObject e{{"name",o["name"].toString(dir.fileName())},{"mcVersion",o["gameVersion"]}};auto loader=o["baseModLoader"].toObject()["name"].toString();if(!loader.isEmpty())loaderId(loader,e);entry(dir.filePath(),"CurseForge",e);}catch(...){}
    }
    return out;
}
QString PackService::install(const QJsonObject &pack,const QJsonObject &build,const QString &name,const QString &group,int memory){
    auto provider=pack["provider"].toString(),id=pack["id"].toString(),version=build["id"].toString();auto instances=m_root+"/mc/instances";QDir().mkpath(instances);QTemporaryDir temp(m_root+"/mc/.pack-XXXXXX");if(!temp.isValid())fail("Could not prepare pack installation");auto stage=temp.path()+"/instance";QDir().mkpath(stage);QJsonObject c{{"loader","vanilla"}};auto archive=temp.path()+"/pack.zip";
    if(provider=="import_ftb"){auto from=pack["path"].toString();auto p=ModRepository::read(from+"/instance.json");c["mcVersion"]=p["mcVersion"];loaderId(p["modLoader"].toString(),c);for(auto dir:{"mods","config","defaultconfigs","kubejs","scripts","resourcepacks","shaderpacks","saves"})if(QDir(from+"/"+dir).exists())copyTree(from+"/"+dir,stage+"/"+dir);for(auto file:{"options.txt","servers.dat"})if(QFile::exists(from+"/"+file)){QFile f(from+"/"+file);if(f.open(QIODevice::ReadOnly))write(stage+"/"+file,f.readAll());}if(c["loader"]=="vanilla")ftbTargets(from,c);}
    else if(provider=="import"){auto path=pack["path"].toString();if(path.startsWith("https://",Qt::CaseInsensitive)){download(path,archive);c=unpack(archive,stage);}else if(QFileInfo(path).isDir())c=importFolder(path,stage);else c=unpack(path,stage);}
    else if(provider=="ftb"){c=ftbManifest(get(ftb+"/modpack/"+enc(id)+"/"+enc(version)).object(),stage);}
    else if(provider=="modrinth"){auto files=build["data"].toObject()["files"].toArray();QJsonObject file;for(auto v:files)if(v.toObject()["primary"].toBool()||file.isEmpty())file=v.toObject();download(file["url"].toString(),archive,file["hashes"].toObject());c=unpack(archive,stage);}
    else if(provider=="curseforge"){auto f=build["data"].toObject();if(f["downloadUrl"].toString().isEmpty())fail("This pack requires manual download from CurseForge, then Import.");QJsonObject hashes;for(auto h:f["hashes"].toArray())if(h.toObject()["algo"].toInt()==1)hashes["sha1"]=h.toObject()["value"];download(f["downloadUrl"].toString(),archive,hashes);c=unpack(archive,stage);}
    else if(provider=="legacy_ftb"){auto v=version;v.replace('.','_');download(legacy+"modpacks/"+enc(id)+"/"+enc(v)+"/"+enc(pack["file"].toString()),archive);c=unpack(archive,stage);if(c["mcVersion"].toString().isEmpty())c["mcVersion"]=pack["mcVersion"];if(c["loader"]=="vanilla"&&QDir(stage+"/mods").exists())fail("The legacy pack does not identify its loader. Import it from its original launcher.");}
    else if(provider=="technic"){auto solder=build["solder"].toString();if(solder.isEmpty()){auto p=build["data"].toObject();download(p["url"].toString(),archive);c=unpack(archive,stage);if(c["mcVersion"].toString().isEmpty())c["mcVersion"]=p["minecraft"];}else{auto d=get(solder+"modpack/"+enc(id)+"/"+enc(version)).object();c["mcVersion"]=d["minecraft"];if(!d["forge"].toString().isEmpty()){c["loader"]="forge";c["loaderVersion"]=d["forge"];}for(auto v:d["mods"].toArray()){auto f=v.toObject();download(f["url"].toString(),archive,fileHashes(f));Archive::extract(archive,stage);}for(auto path:{"bin/version.json","version.json"})if(QFile::exists(stage+"/"+path))profileLoader(ModRepository::read(stage+"/"+path),c);if(c["loaderVersion"].toString().size()<5&&QFile::exists(stage+"/bin/modpack.jar"))fail("This Technic pack uses a legacy patched client. Its original launcher is required.");}}
    else if(provider=="atlauncher"){auto base=atl+"packs/"+enc(id)+"/versions/"+enc(version)+"/";auto d=get(base+"Configs.json").object();c["mcVersion"]=d["minecraft"];auto loader=d["loader"].toObject();if(!loader.isEmpty()){c["loader"]=loader["type"];const auto meta=loader["metadata"].toObject();c["loaderVersion"]=meta["version"].toString(meta["loader"].toString());}for(auto v:d["mods"].toArray()){auto f=v.toObject();if(!f["client"].toBool(true)||f["optional"].toBool())continue;auto type=f["type"].toString("mods");QString folder;if(type=="mods")folder="mods";else if(type=="coremods")folder="coremods";else if(type=="resourcepack")folder="resourcepacks";else if(type=="shaderpack")folder="shaderpacks";else if(type=="texturepack")folder="texturepacks";else if(type=="root")folder="";else if(type=="dependency")folder="mods/"+c["mcVersion"].toString();else fail("Unsupported ATLauncher file type: "+type+" · "+f["name"].toString());if(f["download"]=="browser")fail("Manual download required by author: "+f["url"].toString());auto url=f["url"].toString();if(f["download"]=="server")url=atl+url;download(url,stage+"/"+(folder.isEmpty()?QString():folder+"/")+safeName(f["file"].toString()),fileHashes(f));}if(!d["noConfigs"].toBool()){download(base+"Configs.zip",archive,d["configs"].toObject());Archive::extract(archive,stage);}}
    else fail("Unknown pack source");
    normalizeLoader(c);
    if(c["mcVersion"].toString().isEmpty())fail("Could not detect the Minecraft version of this pack: it has no launcher data and no mods that name a version.");
    // Mods that do not match the pack's Minecraft version or loader are reported (the pack's own choice is kept).
    if(c["loader"].toString()!="vanilla"||QDir(stage+"/mods").exists()){QJsonArray warnings;for(const auto &p:ModCompat::problems(ModCompat::scan(stage+"/mods"),c["mcVersion"].toString(),c["loader"].toString()=="vanilla"?QString("vanilla"):c["loader"].toString()))warnings.append(p);if(!warnings.isEmpty())c["compatibilityWarnings"]=warnings;}
    if(!ModRepository::safeName(c["mcVersion"].toString())||!QStringList{"vanilla","forge","neoforge","fabric","quilt"}.contains(c["loader"].toString()))fail("Pack Minecraft version or loader is unsupported");
    // Imported content cannot replace launcher metadata or impersonate a completed installation.
    c["name"]=name.trimmed();if(c["name"].toString().isEmpty())fail("Instance name is required");c["group"]=group;c["xmx"]=qBound(512,memory,65536);c["ready"]=false;c["packProvider"]=provider;c["packId"]=id;c["packVersion"]=version;
    auto icon=pack["icon"].toString();if(icon.startsWith("https://")){try{QImage image;auto data=bytes(icon);if(data.size()<4*1024*1024&&image.loadFromData(data)&&image.scaled(128,128,Qt::KeepAspectRatio,Qt::SmoothTransformation).save(stage+"/instance-icon.png"))c["icon"]="custom";}catch(...){}} // decorative: a missing icon never blocks the pack
    ModRepository::write(stage+"/instance.json",c);QFile::remove(stage+"/launch-profile.json");QFile::remove(stage+"/mods.json");auto destination=McInstanceManager::folderFor(instances,c["name"].toString());if(!QDir().rename(stage,destination))fail("Could not commit modpack instance");return destination;
}
