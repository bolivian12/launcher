#include <QtTest>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QSaveFile>
#include <QUrlQuery>
#include <archive.h>
#include <archive_entry.h>
#include "ModRepository.hpp"
#include "McInstanceManager.hpp"
#include "Archive.hpp"
#include "Language.hpp"
#include "Loaders.hpp"
#include "SkinManager.hpp"
#include "JavaRuntime.hpp"
#include "PackService.hpp"
#include <stdexcept>

namespace {
QJsonObject version(QString id,QString project,QString file,QByteArray data,QJsonArray deps={}) {
    return {{"id",id},{"project_id",project},{"name",project},{"version_type","release"},{"date_published","2026-01-01"},
        {"game_versions",QJsonArray{"1.20.1"}},{"loaders",QJsonArray{"fabric"}},{"dependencies",deps},
        {"files",QJsonArray{QJsonObject{{"filename",file},{"url","https://cdn.modrinth.com/"+file},{"primary",true},
        {"hashes",QJsonObject{{"sha512",QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha512).toHex())}}}}}}};
}
QByteArray encode(QJsonObject o){return QJsonDocument(o).toJson();}
QByteArray encode(QJsonArray a){return QJsonDocument(a).toJson();}
QByteArray readFile(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
void writeFile(const QString &path,const QByteArray &data){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(data),data.size());}
void zip(const QString &path,const QString &name,const QByteArray &data="jar"){
    auto a=archive_write_new();archive_write_set_format_zip(a);QCOMPARE(archive_write_open_filename(a,QFile::encodeName(path).constData()),ARCHIVE_OK);
    auto e=archive_entry_new();archive_entry_set_pathname(e,name.toUtf8().constData());archive_entry_set_filetype(e,AE_IFREG);archive_entry_set_perm(e,0644);archive_entry_set_size(e,data.size());archive_write_header(a,e);archive_write_data(a,data.constData(),data.size());archive_entry_free(e);archive_write_close(a);archive_write_free(a);
}
}
class CoreTests:public QObject {
    Q_OBJECT
private slots:
    void modpackImportsAndRollback(){
        QTemporaryDir root;auto archive=root.path()+"/test.mrpack";auto hash=QString::fromLatin1(QCryptographicHash::hash("mod",QCryptographicHash::Sha512).toHex());
        QJsonObject index{{"formatVersion",1},{"game","minecraft"},{"dependencies",QJsonObject{{"minecraft","1.20.1"},{"fabric-loader","0.16.0"}}},{"files",QJsonArray{QJsonObject{{"path","mods/test.jar"},{"hashes",QJsonObject{{"sha512",hash}}},{"downloads",QJsonArray{"https://example.test/mod.jar"}}}}}};
        zip(archive,"modrinth.index.json",encode(index));bool corrupt=false;
        PackService packs(root.path(),[&](QUrl){return QByteArray(corrupt?"bad":"mod");});
        auto dir=packs.install({{"provider","import"},{"path",archive}},{},"Imported");QCOMPARE(readFile(dir+"/mods/test.jar"),QByteArray("mod"));auto info=ModRepository::read(dir+"/instance.json");QCOMPARE(info["loader"].toString(),QString("fabric"));QCOMPARE(info["mcVersion"].toString(),QString("1.20.1"));QVERIFY(!info["ready"].toBool());
        corrupt=true;QVERIFY_EXCEPTION_THROWN(packs.install({{"provider","import"},{"path",archive}},{},"Broken"),std::runtime_error);QCOMPARE(QDir(root.path()+"/mc/instances").entryList(QDir::Dirs|QDir::NoDotAndDotDot).size(),1);
        QVERIFY_EXCEPTION_THROWN(PackService::safePath("../outside"),std::runtime_error);QVERIFY_EXCEPTION_THROWN(PackService::safePath("C:\\escape"),std::runtime_error);
    }
    void ftbAppImport(){
        QTemporaryDir root;auto source=root.path()+"/source";QDir().mkpath(source+"/mods");QDir().mkpath(source+"/saves");ModRepository::write(source+"/instance.json",{{"name","FTB"},{"mcVersion","1.21.1"},{"modLoader","neoforge-21.1.200"}});writeFile(source+"/mods/test.jar","mod");writeFile(source+"/saves/world.dat","world");
        auto dir=PackService(root.path()).install({{"provider","import_ftb"},{"path",source}},{},"Copied");QCOMPARE(readFile(dir+"/saves/world.dat"),QByteArray("world"));QVERIFY(QFile::exists(source+"/mods/test.jar"));QCOMPARE(ModRepository::read(dir+"/instance.json")["loader"].toString(),QString("neoforge"));
    }
    void packProviderAdapters(){
        QTemporaryDir root;auto hash=QString::fromLatin1(QCryptographicHash::hash("mod",QCryptographicHash::Md5).toHex());
        PackService service(root.path(),[&](QUrl url){
            auto path=url.path();
            if(path.endsWith("packsnew.json"))return QByteArray(R"([{"name":"Test Pack","type":"public","versions":[{"version":"v1","minecraft":"1.20.1"}]}])");
            if(path.endsWith("Configs.json")){auto o=QJsonDocument::fromJson(R"({"minecraft":"1.20.1","noConfigs":true,"loader":{"type":"fabric","metadata":{"loader":"0.16.0"}},"mods":[{"file":"test.jar","type":"mods"}]})").object();auto mods=o["mods"].toArray();auto mod=mods[0].toObject();mod["md5"]=hash;mod["url"]="https://example.test/mod";mods[0]=mod;o["mods"]=mods;return encode(o);}
            if(path=="/mod")return QByteArray("mod");
            if(path.contains("static/"))return QByteArray("<modpacks><modpack name='Legacy' dir='legacy' version='1.0' mcVersion='1.12.2' url='pack.zip'/></modpacks>");
            if(path=="/trending")return QByteArray(R"({"modpacks":[{"slug":"test","name":"Technic Test"}]})");
            if(path=="/modpack/test")return encode(QJsonObject{{"version","1"},{"minecraft","1.20.1"},{"url","https://example.test/archive"}});
            return QByteArray("{}");
        });

        auto atl=service.search("atlauncher","Test");QCOMPARE(atl.size(),1);auto builds=service.versions(atl.first().toObject());QCOMPARE(builds.size(),1);auto dir=service.install(atl.first().toObject(),builds.first().toObject(),"ATL");QCOMPARE(readFile(dir+"/mods/test.jar"),QByteArray("mod"));QCOMPARE(service.search("legacy_ftb","Legacy").size(),2);auto technic=service.search("technic","");QCOMPARE(technic.size(),1);QCOMPARE(service.versions(technic.first().toObject()).size(),1);
    }
    void livePackCatalogs(){
        if(!qEnvironmentVariableIsSet("EBALIA_LIVE_TESTS"))QSKIP("Opt-in network test");QTemporaryDir root;PackService service(root.path());
        for(auto pair:{qMakePair("modrinth","Fabulously"),qMakePair("atlauncher","All The Forge 10"),qMakePair("ftb","Evolution"),qMakePair("legacy_ftb","Academy"),qMakePair("technic","tekkit")}){auto results=service.search(pair.first,pair.second);QVERIFY2(!results.isEmpty(),pair.first);auto builds=service.versions(results.first().toObject());QVERIFY2(!builds.isEmpty(),pair.first);}
    }
    void javaVersionFormats(){
        QCOMPARE(JavaRuntime::parse("java","openjdk version \"1.8.0_462\"\nOpenJDK 64-Bit Server VM").major,8);
        QCOMPARE(JavaRuntime::parse("java","openjdk 21.0.8 2025-07-15").major,21);
        auto j=JavaRuntime::parse("java","    java.version = 17.0.20\n    java.vendor = Eclipse Adoptium\n    os.arch = amd64\n");QCOMPARE(j.major,17);QCOMPARE(j.architecture,QString("amd64"));
        QCOMPARE(JavaRuntime::parse("java","not a Java installation").major,0);
    }
    void javaDiscoveryNestedRuntime(){
#ifndef Q_OS_WIN
        QTemporaryDir tmp;auto path=tmp.path()+"/java-runtime-gamma/linux/java-runtime-gamma/bin/java";
        QDir().mkpath(QFileInfo(path).absolutePath());writeFile(path,"#!/bin/sh\nprintf '    java.version = 21.0.8\\n    os.arch = amd64\\n' >&2\n");QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        QVERIFY(JavaRuntime::candidates({tmp.path()}).contains(path));auto runtime=JavaRuntime::inspect(path);QCOMPARE(runtime.major,21);QCOMPARE(JavaRuntime::select(21,path),path);QVERIFY(JavaRuntime::select(8,path).isEmpty());
#endif
    }

    void uniqueInstances(){
        QTemporaryDir t;McInstanceManager m(t.path());auto a=m.createInstance("Same","1.20.1","fabric");auto b=m.createInstance("Same","1.20.1","fabric");
        QVERIFY(a!=b);QCOMPARE(m.instances().size(),2);writeFile(a+"/world.txt","keep");m.deleteInstance(a);QCOMPARE(m.instances().size(),1);
        auto trash=QDir(m.mcDir()+"/trash").entryList(QDir::Dirs|QDir::NoDotAndDotDot);QCOMPARE(trash.size(),1);QCOMPARE(readFile(m.mcDir()+"/trash/"+trash[0]+"/world.txt"),QByteArray("keep"));
        QVERIFY_EXCEPTION_THROWN(m.deleteInstance(t.path()),std::runtime_error);
    }
    void paths(){QVERIFY(!ModRepository::safeName("../file.jar"));QVERIFY(!ModRepository::safeName("C:\\x"));QVERIFY(!ModRepository::safeName(".."));QVERIFY(ModRepository::safeName("sodium.jar"));}
    void arguments(){
        QJsonArray a{"--gameDir","${game_directory}",QJsonObject{{"rules",QJsonArray{QJsonObject{{"action","allow"},{"features",QJsonObject{{"is_demo_user",true}}}}}},{"value","--demo"}}};
        QCOMPARE(McInstanceManager::arguments(a,{{"game_directory","/a path/game"}}),QStringList({"--gameDir","/a path/game"}));
        QVERIFY_EXCEPTION_THROWN(McInstanceManager::arguments(QJsonArray{"${unknown}"},{}),std::runtime_error);
        QVERIFY(!McInstanceManager::allowedByRules({{"rules",QJsonArray{QJsonObject{{"action","allow"},{"os",QJsonObject{{"name","nonexistent"}}}}}}}));
    }
    void dependenciesAndMissing(){
        QTemporaryDir t;auto dep=QJsonObject{{"dependency_type","required"},{"project_id","API"},{"version_id","API1"}};
        auto mod=version("S1","Sodium","sodium.jar","sodium",QJsonArray{dep});auto api=version("API1","API","api.jar","api");
        ModRepository repo(t.path(),[&](const QUrl &url){auto path=url.path();if(path=="/v2/project/Sodium/version")return encode(QJsonArray{mod});if(path=="/v2/version/API1")return encode(api);return encode(QJsonArray{});});
        auto plan=repo.plan(QJsonArray{QJsonObject{{"project_id","Sodium"},{"name","Sodium"}},QJsonObject{{"project_id","Missing"},{"name","Missing"}}},"1.20.1","fabric");
        QCOMPARE(plan["versions"].toArray().size(),2);QCOMPARE(plan["missing"].toArray().size(),1);
        auto bad=repo.plan(QJsonArray{QJsonObject{{"project_id","Sodium"}}},"1.21","fabric");QVERIFY(bad["versions"].toArray().isEmpty());
    }
    void pinnedConflict(){
        QTemporaryDir t;auto dep=[](QString v){return QJsonObject{{"dependency_type","required"},{"project_id","API"},{"version_id",v}};};
        auto one=version("A1","A","a.jar","a",QJsonArray{dep("API1")}),two=version("B1","B","b.jar","b",QJsonArray{dep("API2")});
        ModRepository repo(t.path(),[&](const QUrl &url){auto p=url.path();if(p.contains("project/A/"))return encode(QJsonArray{one});if(p.contains("project/B/"))return encode(QJsonArray{two});if(p.endsWith("API1"))return encode(version("API1","API","api.jar","api"));return encode(version("API2","API","api2.jar","api2"));});
        auto p=repo.plan(QJsonArray{QJsonObject{{"project_id","A"}},QJsonObject{{"project_id","B"}}},"1.20.1","fabric");QCOMPARE(p["missing"].toArray().size(),1);QCOMPARE(p["versions"].toArray().size(),2);
    }
    void atomicMods(){
        QTemporaryDir t;McInstanceManager manager(t.path());auto dir=manager.createInstance("Test","1.20.1","fabric");
        auto v=version("A1","A","a.jar","original");bool corrupt=false,offline=false;
        ModRepository repo(t.path(),[&](const QUrl &url){if(url.host()=="cdn.modrinth.com"){if(offline)throw std::runtime_error("Network disconnected");return QByteArray(corrupt?"corrupt":"original");}return encode(QJsonArray{v});});
        auto projects=QJsonArray{QJsonObject{{"project_id","A"}}};auto p=repo.plan(projects,"1.20.1","fabric");repo.apply(dir,p);QCOMPARE(readFile(dir+"/mods/a.jar"),QByteArray("original"));
        auto manifest=readFile(dir+"/mods.json");writeFile(dir+"/mods/manual.jar","untouched");
        corrupt=true;QVERIFY_EXCEPTION_THROWN(repo.apply(dir,p),std::runtime_error);QCOMPARE(readFile(dir+"/mods.json"),manifest);QCOMPARE(readFile(dir+"/mods/a.jar"),QByteArray("original"));
        corrupt=false;offline=true;QVERIFY_EXCEPTION_THROWN(repo.apply(dir,p),std::runtime_error);QCOMPARE(readFile(dir+"/mods/manual.jar"),QByteArray("untouched"));
        offline=false;auto info=ModRepository::read(dir+"/instance.json");info["mcVersion"]="1.21";ModRepository::write(dir+"/instance.json",info);QVERIFY_EXCEPTION_THROWN(repo.apply(dir,p),std::runtime_error);
    }
    void localFileNeverOverwritten(){
        QTemporaryDir t;McInstanceManager manager(t.path());auto dir=manager.createInstance("Test","1.20.1","fabric");writeFile(dir+"/mods/a.jar","local");
        auto v=version("A1","A","a.jar","download");ModRepository repo(t.path(),[&](const QUrl &url){if(url.host()=="cdn.modrinth.com")return QByteArray("download");return encode(QJsonArray{v});});
        auto p=repo.plan(QJsonArray{QJsonObject{{"project_id","A"}}},"1.20.1","fabric");QVERIFY_EXCEPTION_THROWN(repo.apply(dir,p),std::runtime_error);QCOMPARE(readFile(dir+"/mods/a.jar"),QByteArray("local"));
    }
    void packsAndIdentification(){
        QTemporaryDir t;McInstanceManager manager(t.path());auto dir=manager.createInstance("Test","1.20.1","fabric");writeFile(dir+"/mods/a.jar","a");writeFile(dir+"/mods/off.jar.disabled","off");
        ModRepository repo(t.path(),[](const QUrl &){return encode(QJsonObject{{"project_id","A"},{"name","A"}});});
        auto p=repo.capture(dir,"Favorites");QCOMPARE(p["projects"].toArray().size(),1);repo.savePack(p);QCOMPARE(repo.packs().size(),1);
        ModRepository bad(t.path(),[](const QUrl &)->QByteArray{throw std::runtime_error("404");});QVERIFY_EXCEPTION_THROWN(bad.capture(dir,"Fail"),std::runtime_error);QCOMPARE(repo.packs().size(),1);
        QVERIFY_EXCEPTION_THROWN(repo.savePack({{"schema",1},{"name","bad"},{"projects",QJsonArray{QJsonObject{{"project_id","../../path"}}}}}),std::runtime_error);
    }
    void archiveTraversal(){
        QTemporaryDir t;zip(t.path()+"/safe.zip","folder/mod.jar");Archive::extract(t.path()+"/safe.zip",t.path()+"/out");QCOMPARE(readFile(t.path()+"/out/folder/mod.jar"),QByteArray("jar"));
        zip(t.path()+"/bad.zip","../escaped.jar");QVERIFY_EXCEPTION_THROWN(Archive::extract(t.path()+"/bad.zip",t.path()+"/out"),std::runtime_error);QVERIFY(!QFile::exists(t.path()+"/escaped.jar"));
    }
    void translations(){
        QFile english(":/locales/en.json");QVERIFY(english.open(QIODevice::ReadOnly));auto keys=QJsonDocument::fromJson(english.readAll()).object().keys();QCOMPARE(Language::available().size(),10);
        for(const auto &code:Language::available()){
            QFile f(":/locales/"+code+".json");QVERIFY2(f.open(QIODevice::ReadOnly),qPrintable(code));auto data=QJsonDocument::fromJson(f.readAll()).object();QCOMPARE(data.keys(),keys);
            for(const auto &key:keys){QVERIFY2(!data[key].toString().trimmed().isEmpty(),qPrintable(code+": "+key));QVERIFY(!data[key].toString().contains(QChar::ReplacementCharacter));}
            Language::current=code;QVERIFY(!Language::standard("Cancel").isEmpty());QCOMPARE(Language::text("Guardar pack","Save pack","Salvar pack"),data["Save pack"].toString());
        }
    }
    void curseForgeAdapter(){
        QTemporaryDir t;McInstanceManager manager(t.path());auto dir=manager.createInstance("CF","1.20.1","fabric");
        auto file=[](int id,int project,QString name,QByteArray data,QJsonArray deps=QJsonArray{}){
            return QJsonObject{{"id",id},{"modId",project},{"displayName",name},{"fileName",name+".jar"},{"releaseType",1},{"fileDate","2026-01-01"},
                {"gameVersions",QJsonArray{"1.20.1","Fabric"}},{"downloadUrl","https://cdn.curseforge.com/"+name+".jar"},{"dependencies",deps},
                {"hashes",QJsonArray{QJsonObject{{"algo",1},{"value",QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha1).toHex())}}}}};
        };
        auto mod=file(10,100,"mod","mod",QJsonArray{QJsonObject{{"modId",200},{"relationType",3}}});auto dep=file(20,200,"api","api");
        ModRepository repo(t.path(),[&](const QUrl &url){
            if(url.host()=="cdn.curseforge.com")return url.path().contains("mod.jar")?QByteArray("mod"):QByteArray("api");
            if(url.path().endsWith("mods/search")){if(QUrlQuery(url).queryItemValue("modLoaderType")!="4")throw std::runtime_error("Wrong loader filter");return encode(QJsonObject{{"data",QJsonArray{QJsonObject{{"id",100},{"name","Example"},{"summary","Test"}}}}});}
            if(url.path().contains("/100/"))return encode(QJsonObject{{"data",QJsonArray{mod}}});return encode(QJsonObject{{"data",QJsonArray{dep}}});
        });
        auto hits=repo.search("example","1.20.1","fabric",0,"curseforge");QCOMPARE(hits[0].toObject()["project_id"].toString(),QString("cf-100"));
        auto plan=repo.plan(QJsonArray{QJsonObject{{"project_id","cf-100"},{"name","Example"}}},"1.20.1","fabric");QCOMPARE(plan["versions"].toArray().size(),2);QVERIFY(plan["missing"].toArray().isEmpty());repo.apply(dir,plan);QCOMPARE(readFile(dir+"/mods/mod.jar"),QByteArray("mod"));
        auto pack=repo.capture(dir,"CF pack");QCOMPARE(pack["projects"].toArray().size(),2);
        mod["downloadUrl"]=QJsonValue::Null;auto blocked=repo.plan(QJsonArray{QJsonObject{{"project_id","cf-100"}}},"1.20.1","fabric");QVERIFY(blocked["versions"].toArray().isEmpty());QVERIFY(blocked["missing"].toArray()[0].toObject()["reason"].toString().contains("Manual download"));
    }
    void skinLibraryAndLocalPack(){
        QTemporaryDir t;QImage skin(64,64,QImage::Format_RGBA8888);skin.fill(QColor("#63c8a0"));auto source=t.path()+"/input.png";QVERIFY(skin.save(source));
        SkinManager skins(t.path());auto imported=skins.importSkin(source,"Example","slim");QCOMPARE(skins.skins().size(),1);QCOMPARE(SkinManager::preview(skin,"slim").size(),QSize(240,320));QVERIFY(!SkinManager::preview(skin,"slim",true).isNull());
        QImage invalid(32,32,QImage::Format_RGBA8888);invalid.fill(Qt::red);invalid.save(t.path()+"/bad.png");QVERIFY_EXCEPTION_THROWN(skins.importSkin(t.path()+"/bad.png","Bad","classic"),std::runtime_error);
        auto jar=t.path()+"/client.jar";auto a=archive_write_new();archive_write_set_format_zip(a);QCOMPARE(archive_write_open_filename(a,QFile::encodeName(jar).constData()),ARCHIVE_OK);
        QMap<QString,QByteArray> files{{"version.json",encode(QJsonObject{{"pack_version",QJsonObject{{"resource",15}}}})},
            {"assets/minecraft/textures/entity/player/wide/steve.png","placeholder"},{"assets/minecraft/textures/entity/player/slim/alex.png","placeholder"}};
        for(auto it=files.begin();it!=files.end();++it){auto e=archive_entry_new();archive_entry_set_pathname(e,it.key().toUtf8().constData());archive_entry_set_filetype(e,AE_IFREG);archive_entry_set_size(e,it.value().size());archive_write_header(a,e);archive_write_data(a,it.value().constData(),it.value().size());archive_entry_free(e);}archive_write_close(a);archive_write_free(a);
        auto instance=t.path()+"/instance";QDir().mkpath(instance);writeFile(instance+"/options.txt","music:0.5\nresourcePacks:[\"vanilla\",\"file/custom.zip\"]\n");
        SkinManager::applyLocal(imported["file"].toString(),"slim",instance,jar);QVERIFY(readFile(instance+"/options.txt").contains("file/custom.zip"));QVERIFY(readFile(instance+"/options.txt").contains("file/ebalia-local-skin.zip"));
        Archive::extract(instance+"/resourcepacks/ebalia-local-skin.zip",t.path()+"/unpack");QCOMPARE(ModRepository::read(t.path()+"/unpack/pack.mcmeta")["pack"].toObject()["pack_format"].toInt(),15);
        QImage packed(t.path()+"/unpack/assets/minecraft/textures/entity/player/wide/steve.png");QCOMPARE(packed.size(),QSize(64,64));
        SkinManager::removeLocal(instance);QVERIFY(!readFile(instance+"/options.txt").contains("file/ebalia-local-skin.zip"));QVERIFY(readFile(instance+"/options.txt").contains("music:0.5"));
    }
    void liveModrinth(){
        if(!qEnvironmentVariableIsSet("EBALIA_LIVE_TESTS"))QSKIP("Opt-in network test");
        QTemporaryDir t;McInstanceManager manager(t.path());auto dir=manager.createInstance("Live","1.20.1","fabric");ModRepository repo(t.path());
        auto hits=repo.search("sodium","1.20.1","fabric");QVERIFY(!hits.isEmpty());QJsonObject sodium;for(const auto &h:hits)if(h.toObject()["slug"].toString()=="sodium")sodium=h.toObject();QVERIFY(!sodium.isEmpty());
        auto plan=repo.plan(QJsonArray{QJsonObject{{"project_id",sodium["project_id"]},{"name","Sodium"}}},"1.20.1","fabric");QVERIFY(plan["missing"].toArray().isEmpty());QVERIFY(!plan["versions"].toArray().isEmpty());repo.apply(dir,plan);auto pack=repo.capture(dir,"Sodium");QVERIFY(!pack["projects"].toArray().isEmpty());
        auto next=repo.plan(pack["projects"].toArray(),"1.21.1","fabric");QVERIFY(next["missing"].toArray().isEmpty());
        QVERIFY(!Loaders::versions("forge","1.20.1").isEmpty());QVERIFY(!Loaders::versions("neoforge","1.21.1").isEmpty());
    }
};
QTEST_MAIN(CoreTests)
#include "CoreTests.moc"
