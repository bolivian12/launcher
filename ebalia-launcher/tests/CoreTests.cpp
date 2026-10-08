#include <QtTest>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QSaveFile>
#include <QUrlQuery>
#include <QImageReader>
#include <archive.h>
#include <archive_entry.h>
#include "ModRepository.hpp"
#include "McInstanceManager.hpp"
#include "Archive.hpp"
#include "Language.hpp"
#include "Loaders.hpp"
#include <algorithm>
#include "SkinManager.hpp"
#include "JavaRuntime.hpp"
#include "JavaDownloader.hpp"
#include "PackService.hpp"
#include "ServerList.hpp"
#include "LostNative.hpp"
#include "SoftwareGl.hpp"
#include "ModCompat.hpp"
#include "CrashReport.hpp"
#include "GpuInfo.hpp"
#include <stdexcept>
#include <clocale>
#include <functional>

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
QByteArray lzma(const QByteArray &data){
    QByteArray out(data.size()+65536,0);size_t used=0;auto a=archive_write_new();archive_write_add_filter_lzma(a);archive_write_set_format_raw(a);archive_write_open_memory(a,out.data(),size_t(out.size()),&used);
    auto e=archive_entry_new();archive_entry_set_filetype(e,AE_IFREG);archive_entry_set_size(e,data.size());archive_write_header(a,e);archive_write_data(a,data.constData(),data.size());archive_entry_free(e);archive_write_close(a);archive_write_free(a);out.resize(int(used));return out;
}
void zip(const QString &path,const QString &name,const QByteArray &data="jar"){
    auto a=archive_write_new();archive_write_set_format_zip(a);QCOMPARE(archive_write_open_filename(a,QFile::encodeName(path).constData()),ARCHIVE_OK);
    auto e=archive_entry_new();archive_entry_set_pathname(e,name.toUtf8().constData());archive_entry_set_filetype(e,AE_IFREG);archive_entry_set_perm(e,0644);archive_entry_set_size(e,data.size());archive_write_header(a,e);archive_write_data(a,data.constData(),data.size());archive_entry_free(e);archive_write_close(a);archive_write_free(a);
}
using Files=QList<QPair<QString,QByteArray>>;
// ZIP with several entries, names stored as UTF-8: a name ending in '/' is a folder, data starting with "->" a symbolic link.
void zipEntries(const QString &path,const Files &entries){
    auto a=archive_write_new();archive_write_set_format_zip(a);archive_write_set_options(a,"zip:hdrcharset=UTF-8");QCOMPARE(archive_write_open_filename(a,QFile::encodeName(path).constData()),ARCHIVE_OK);
    for(const auto &[name,data]:entries){
        auto e=archive_entry_new();archive_entry_set_pathname_utf8(e,name.toUtf8().constData());
        if(name.endsWith('/')){archive_entry_set_filetype(e,AE_IFDIR);archive_entry_set_perm(e,0755);archive_entry_set_size(e,0);archive_write_header(a,e);}
        else if(data.startsWith("->")){archive_entry_set_filetype(e,AE_IFLNK);archive_entry_set_perm(e,0777);archive_entry_set_symlink(e,data.mid(2).constData());archive_entry_set_size(e,0);archive_write_header(a,e);}
        else{archive_entry_set_filetype(e,AE_IFREG);archive_entry_set_perm(e,0644);archive_entry_set_size(e,data.size());archive_write_header(a,e);archive_write_data(a,data.constData(),size_t(data.size()));}
        archive_entry_free(e);
    }
    archive_write_close(a);archive_write_free(a);
}
Files prefixed(const QString &prefix,const Files &files){Files out;for(const auto &f:files)out<<qMakePair(prefix+f.first,f.second);return out;}
void writeTree(const QString &base,const Files &files){for(const auto &[name,data]:files){if(name.endsWith('/')){QDir().mkpath(base+"/"+name);continue;}QDir().mkpath(QFileInfo(base+"/"+name).absolutePath());writeFile(base+"/"+name,data);}}
QByteArray zipBytes(const Files &files){QTemporaryDir t;zipEntries(t.path()+"/x.zip",files);return readFile(t.path()+"/x.zip");}
// Imports through the real PackService and returns the written instance.json (or {"error": message}).
QJsonObject importPack(PackService &service,const QJsonObject &pack){
    try{auto dir=service.install(pack,{},"Imported");auto info=ModRepository::read(dir+"/instance.json");info["dir"]=dir;return info;}
    catch(const std::exception &e){return {{"error",QString::fromUtf8(e.what())}};}
}
QJsonObject importPath(PackService &service,const QString &path){return importPack(service,{{"provider","import"},{"path",path}});}
QString describe(const QJsonObject &info){return info.contains("error")?"error: "+info["error"].toString():(info["mcVersion"].toString()+" "+info["loader"].toString()+" "+info["loaderVersion"].toString()).trimmed();}
QByteArray mmcPack(const QString &game,const QString &uid={},const QString &version={}){QJsonArray c{QJsonObject{{"uid","net.minecraft"},{"version",game}}};if(!uid.isEmpty())c.append(QJsonObject{{"uid",uid},{"version",version}});return encode(QJsonObject{{"formatVersion",1},{"components",c}});}
QByteArray mrIndex(const QJsonObject &deps,const QJsonArray &files={}){return encode(QJsonObject{{"formatVersion",1},{"game","minecraft"},{"versionId","1"},{"name","Test"},{"dependencies",deps},{"files",files}});}
QByteArray cfManifest(const QString &game,const QJsonArray &loaders,const QJsonArray &files={}){return encode(QJsonObject{{"minecraft",QJsonObject{{"version",game},{"modLoaders",loaders}}},{"manifestType","minecraftModpack"},{"manifestVersion",1},{"name","Test"},{"version","1"},{"files",files},{"overrides","overrides"}});}
// Trimmed copies of real loader profiles (Forge/NeoForge installer version.json, Fabric/Quilt meta profiles) as Technic carries them.
QByteArray profile(const QString &id,const QString &inherits,const QStringList &libraries,const QStringList &game={}){QJsonArray libs;for(const auto &l:libraries)libs.append(QJsonObject{{"name",l}});QJsonObject o{{"id",id},{"inheritsFrom",inherits},{"libraries",libs}};if(!game.isEmpty())o["arguments"]=QJsonObject{{"game",QJsonArray::fromStringList(game)}};return encode(o);}
}
class CoreTests:public QObject {
    Q_OBJECT
private slots:
    void softwareGraphics(){
        // Lines from real crashes on a virtual machine (Forge 26.3) and on a PC without a graphics driver (1.20.1).
        QVERIFY(SoftwareGl::openGlFailure("FATAL ERROR in native method: [LWJGL] Thread[#3,Render thread,5,main]: No context is current or a function that is not available in the current context was called. The JVM will abort execution."));
        QVERIFY(SoftwareGl::openGlFailure("[LWJGL] GLFW_API_UNAVAILABLE error\nDescription : WGL: The driver does not appear to support OpenGL"));
        QVERIFY(!SoftwareGl::openGlFailure("Caused by: java.lang.IllegalStateException: Failed to find system mod: forge"));
        QVERIFY(SoftwareGl::javaArguments("C:/mesa").first().startsWith("-Dorg.lwjgl.opengl.libname="));
        // Only the requested entries of a 7z archive are written, flat in the destination.
        QTemporaryDir t;const auto archive=t.path()+"/mesa.7z";
        {auto a=archive_write_new();archive_write_set_format_7zip(a);QCOMPARE(archive_write_open_filename(a,QFile::encodeName(archive).constData()),ARCHIVE_OK);
         for(const auto &[name,data]:QList<QPair<QString,QByteArray>>{{"x64/opengl32.dll","gl"},{"x64/libgallium_wgl.dll","gallium"},{"x86/opengl32.dll","32"},{"x64/clon12compiler.dll","big"}}){auto e=archive_entry_new();archive_entry_set_pathname(e,name.toUtf8().constData());archive_entry_set_filetype(e,AE_IFREG);archive_entry_set_perm(e,0644);archive_entry_set_size(e,data.size());archive_write_header(a,e);archive_write_data(a,data.constData(),size_t(data.size()));archive_entry_free(e);}
         archive_write_close(a);archive_write_free(a);}
        Archive::extractFiles(archive,t.path()+"/out",{"x64/opengl32.dll","x64/libgallium_wgl.dll"});
        QCOMPARE(QDir(t.path()+"/out").entryList(QDir::Files),QStringList({"libgallium_wgl.dll","opengl32.dll"}));QCOMPARE(readFile(t.path()+"/out/opengl32.dll"),QByteArray("gl"));
        if(qEnvironmentVariableIsSet("EBALIA_LIVE_TESTS")){QTemporaryDir root;auto folder=SoftwareGl::prepare(root.path());QVERIFY(QFileInfo(folder+"/opengl32.dll").size()>100000);QVERIFY(QFileInfo(folder+"/libgallium_wgl.dll").size()>10000000);QCOMPARE(SoftwareGl::prepare(root.path()),folder);}
    }
    void instanceFoldersCarryTheirNames(){
        QTemporaryDir t;McInstanceManager manager(t.path());const auto root=t.path()+"/mc/instances";
        auto a=manager.createInstance("Mi Survival","1.21.1","vanilla");QCOMPARE(QFileInfo(a).fileName(),QString("Mi Survival"));
        QCOMPARE(QFileInfo(manager.createInstance("Mi Survival","1.21.1","vanilla")).fileName(),QString("Mi Survival (2)"));
        QCOMPARE(QFileInfo(manager.createInstance("a/b:c*?","1.21.1","vanilla")).fileName(),QString("a_b_c__"));
        QCOMPARE(QFileInfo(manager.createInstance("con","1.21.1","vanilla")).fileName(),QString("con_"));
        QCOMPARE(QFileInfo(manager.createInstance("...","1.21.1","vanilla")).fileName(),QString("Instance"));
        QCOMPARE(QFileInfo(manager.copyInstance(a,"Copia")).fileName(),QString("Copia"));
        // A folder from an earlier launcher (random number) keeps working and gets its name.
        const auto old=root+"/4525036e-42bc-4122-aa9d-a8281160c65d";QDir().mkpath(old+"/saves/W");ModRepository::write(old+"/instance.json",QJsonObject{{"name","npsoewe"},{"mcVersion","26.3"},{"loader","forge"},{"ready",true}});
        ModRepository::write(old+"/launch-profile.json",QJsonObject{{"ebaliaProfile",2}});
        bool listed=false;for(const auto &i:manager.instances())listed|=i.name=="npsoewe"&&i.ready;QVERIFY(listed);
        manager.nameFolders();QVERIFY(!QFile::exists(old));QVERIFY(QFile::exists(root+"/npsoewe/saves/W"));
        bool renamed=false;for(const auto &i:manager.instances())renamed|=i.name=="npsoewe"&&i.dir.endsWith("/npsoewe")&&i.ready;QVERIFY(renamed);
        // Renaming the instance renames its folder; the same name keeps it.
        auto info=ModRepository::read(a+"/instance.json");info["name"]="Hardcore";ModRepository::write(a+"/instance.json",info);
        const auto moved=manager.renameFolder(a);QCOMPARE(QFileInfo(moved).fileName(),QString("Hardcore"));QCOMPARE(manager.renameFolder(moved),moved);
    }
    void staleLoaderProfilesInstallAgain(){
        // Forge instances prepared before 1.1.0 lost forge:universal: they must install again instead of crashing.
        QTemporaryDir t;McInstanceManager manager(t.path());
        auto forge=manager.createInstance("Old Forge","26.3","forge","66.0.9"),vanilla=manager.createInstance("Vanilla","26.3","vanilla");
        for(const auto &dir:{forge,vanilla}){auto info=ModRepository::read(dir+"/instance.json");info["ready"]=true;ModRepository::write(dir+"/instance.json",info);ModRepository::write(dir+"/launch-profile.json",QJsonObject{{"id","old"}});}
        QMap<QString,bool> ready;for(const auto &i:manager.instances())ready[i.name]=i.ready;
        QVERIFY(!ready["Old Forge"]);QVERIFY(ready["Vanilla"]);QVERIFY(!ModRepository::read(forge+"/instance.json")["ready"].toBool());
        auto info=ModRepository::read(forge+"/instance.json");info["ready"]=true;ModRepository::write(forge+"/instance.json",info);ModRepository::write(forge+"/launch-profile.json",QJsonObject{{"ebaliaProfile",2}});
        for(const auto &i:manager.instances())if(i.name=="Old Forge")QVERIFY(i.ready);
    }
    void loaderLibrariesKeepEveryClassifier(){
        // Regression: Forge 26.x ships forge:...:universal and forge:...:client; dropping one gave "Failed to find system mod: forge".
        auto lib=[](const QString &name){return QJsonObject{{"name",name}};};
        const QJsonArray game{lib("com.google.guava:guava:31.1-jre"),lib("org.lwjgl:lwjgl:3.3.3"),lib("org.lwjgl:lwjgl:3.3.3:natives-windows"),lib("org.ow2.asm:asm:9.3")};
        const QJsonArray loader{lib("net.minecraftforge:forge:26.3-66.0.9:universal"),lib("net.minecraftforge:forge:26.3-66.0.9:client"),lib("com.google.guava:guava:33.6.0-jre"),lib("org.ow2.asm:asm:9.10.1")};
        QStringList names;for(const auto &l:McInstanceManager::mergeLibraries(game,loader))names<<l.toObject()["name"].toString();
        QCOMPARE(names,QStringList({"net.minecraftforge:forge:26.3-66.0.9:universal","net.minecraftforge:forge:26.3-66.0.9:client","com.google.guava:guava:33.6.0-jre","org.ow2.asm:asm:9.10.1","org.lwjgl:lwjgl:3.3.3","org.lwjgl:lwjgl:3.3.3:natives-windows"}));
    }
    void neoforgeVersionsForEveryNumbering(){
        QCOMPARE(Loaders::neoforgePrefix("1.21.1"),QString("21.1."));QCOMPARE(Loaders::neoforgePrefix("1.20.6"),QString("20.6."));QCOMPARE(Loaders::neoforgePrefix("1.21"),QString("21.0."));
        QCOMPARE(Loaders::neoforgePrefix("26.3"),QString("26.3.0."));QCOMPARE(Loaders::neoforgePrefix("26.1.2"),QString("26.1.2."));
        QStringList neo{"26.3.0.9-beta","26.3.0.52-beta","26.3.0.10-beta"};Loaders::sort("neoforge","26.3",neo);QCOMPARE(neo.first(),QString("26.3.0.52-beta"));
        QStringList mixed{"21.1.9","21.1.100-beta","21.1.77"};Loaders::sort("neoforge","1.21.1",mixed);QCOMPARE(mixed,QStringList({"21.1.77","21.1.9","21.1.100-beta"}));
        QStringList forge{"1.20.1-47.4.9","1.20.1-47.4.26","1.20.1-47.10.0"};Loaders::sort("forge","1.20.1",forge);QCOMPARE(forge.first(),QString("1.20.1-47.10.0"));
        QStringList quilt{"0.20.0-beta.9","0.20.0-beta.10","0.24.0","0.30.1","0.31.0-beta.4","0.30.1-beta.4"};Loaders::sort("quilt","26.3",quilt);
        QCOMPARE(quilt,QStringList({"0.30.1","0.24.0","0.31.0-beta.4","0.30.1-beta.4","0.20.0-beta.10","0.20.0-beta.9"}));
        QStringList neo1201{"1.20.1-47.1.9","1.20.1-47.1.106","1.20.1-47.1.82"};Loaders::sort("neoforge","1.20.1",neo1201);QCOMPARE(neo1201.first(),QString("1.20.1-47.1.106"));
        QCOMPARE(Loaders::installerUrl("neoforge","1.20.1","1.20.1-47.1.106"),QString("https://maven.neoforged.net/releases/net/neoforged/forge/1.20.1-47.1.106/forge-1.20.1-47.1.106-installer.jar"));
        QStringList old{"1.7.10-10.13.4.1614-1.7.10","1.7.10-10.13.4.1558-1.7.10"};Loaders::sort("forge","1.7.10",old);QCOMPARE(old.first(),QString("1.7.10-10.13.4.1614-1.7.10"));
    }
    void libraryRulesUnderstandArchitectureNames(){
        const auto arch=QSysInfo::currentCpuArchitecture();
        const bool x64=arch=="x86_64";
        const QJsonObject amd64{{"rules",QJsonArray{QJsonObject{{"action","allow"},{"os",QJsonObject{{"arch",x64?"x86_64":"arm64"}}}}}}};
        QVERIFY(McInstanceManager::allowedByRules(amd64));
        const QJsonObject other{{"rules",QJsonArray{QJsonObject{{"action","allow"},{"os",QJsonObject{{"arch",x64?"aarch64":"amd64"}}}}}}};
        QVERIFY(!McInstanceManager::allowedByRules(other));
    }
    void lostVersionNativeCommand(){
        QTemporaryDir root;const auto install=root.filePath("versions/alpha");QDir().mkpath(install+"/bin");
        QFile jar(install+"/bin/minecraft.jar");QVERIFY(jar.open(QIODevice::WriteOnly));jar.write("x");jar.close();
        const QJsonObject spec{{"java",8},{"workingDir",""},{"classpath",QJsonArray{"bin/minecraft.jar","@lwjgl2"}},{"mainClass","net.minecraft.client.Minecraft"},
            {"jvmArgs",QJsonArray{"-Xmx1G"}},{"gameArgs",QJsonArray{"${username}","--gameDir","${installDir}"}},{"natives","lwjgl2"}};
        QVERIFY(LostNative::available(spec));
        const auto cmd=LostNative::command(spec,install,root.filePath("libraries"),"Arq");
        QCOMPARE(QFileInfo(cmd.workingDir).absoluteFilePath(),QFileInfo(install).absoluteFilePath());
        const auto cp=cmd.arguments[cmd.arguments.indexOf("-cp")+1].split(QDir::listSeparator());
        QVERIFY(cp.first().endsWith("minecraft.jar"));QVERIFY(cp.join(" ").contains("lwjgl"));QVERIFY(cp.join(" ").contains("jinput-2.0.5.jar"));
        QVERIFY(cmd.arguments.contains("Arq"));QVERIFY(cmd.arguments.contains(QDir::toNativeSeparators(install)));
        QVERIFY(cmd.arguments.indexOf("net.minecraft.client.Minecraft")>cmd.arguments.indexOf("-cp"));
        QVERIFY(std::any_of(cmd.arguments.begin(),cmd.arguments.end(),[](const QString &a){return a.startsWith("-Djava.library.path=")&&a.contains("natives-"+LostNative::system());}));
        // Paths from the catalog may not leave the package.
        auto escape=spec;escape["classpath"]=QJsonArray{"../other.jar"};QVERIFY_EXCEPTION_THROWN(LostNative::command(escape,install,root.filePath("libraries"),"Arq"),std::runtime_error);
        auto missing=spec;missing["classpath"]=QJsonArray{"bin/none.jar"};QVERIFY_EXCEPTION_THROWN(LostNative::command(missing,install,root.filePath("libraries"),"Arq"),std::runtime_error);
        QCOMPARE(LostNative::command(spec,install,root.filePath("libraries"),"  ").arguments.contains("Player"),true);
    }
    void serverListKeepsUnknownFields(){
        QTemporaryDir dir;const auto file=dir.filePath("servers.dat");
        QVERIFY(ServerList::read(file).isEmpty());
        // A list written by Minecraft: one server with a field EBALIA does not use, plus a root field.
        auto str=[](const QByteArray &s){QByteArray b;b.append(char(s.size()>>8));b.append(char(s.size()&255));return b+s;};
        QByteArray nbt;nbt.append(char(10));nbt+=str("");
        nbt.append(char(1));nbt+=str("extra");nbt.append(char(7));
        nbt.append(char(9));nbt+=str("servers");nbt.append(char(10));nbt+=QByteArray::fromHex("00000001");
        nbt.append(char(8));nbt+=str("name");nbt+=str("Hypixel");nbt.append(char(8));nbt+=str("ip");nbt+=str("mc.hypixel.net");
        nbt.append(char(1));nbt+=str("acceptTextures");nbt.append(char(1));nbt.append(char(0));nbt.append(char(0));
        {QFile f(file);QVERIFY(f.open(QIODevice::WriteOnly));f.write(nbt);}
        auto servers=ServerList::read(file);QCOMPARE(servers.size(),1);QCOMPARE(servers[0].address,QString("mc.hypixel.net"));
        ServerList::add(file,"Ñandú","play.example.org:25566");
        servers=ServerList::read(file);QCOMPARE(servers.size(),2);QCOMPARE(servers[1].name,QString("Ñandú"));QCOMPARE(servers[1].address,QString("play.example.org:25566"));
        {QFile f(file);QVERIFY(f.open(QIODevice::ReadOnly));const auto bytes=f.readAll();QVERIFY(bytes.contains("acceptTextures"));QVERIFY(bytes.contains("extra"));}
        ServerList::remove(file,0);servers=ServerList::read(file);QCOMPARE(servers.size(),1);QCOMPARE(servers[0].name,QString("Ñandú"));
        QVERIFY_EXCEPTION_THROWN(ServerList::remove(file,5),std::runtime_error);
        {QFile f(file);QVERIFY(f.open(QIODevice::WriteOnly));f.write(nbt.left(20));}
        QVERIFY(ServerList::read(file).isEmpty());QVERIFY_EXCEPTION_THROWN(ServerList::add(file,"x","y"),std::runtime_error);
    }
    void modpackImportsAndRollback(){
        QTemporaryDir root;auto archive=root.path()+"/test.mrpack";auto hash=QString::fromLatin1(QCryptographicHash::hash("mod",QCryptographicHash::Sha512).toHex());
        QJsonObject index{{"formatVersion",1},{"game","minecraft"},{"dependencies",QJsonObject{{"minecraft","1.20.1"},{"fabric-loader","0.16.0"}}},{"files",QJsonArray{QJsonObject{{"path","mods/test.jar"},{"hashes",QJsonObject{{"sha512",hash}}},{"downloads",QJsonArray{"https://example.test/mod.jar"}}}}}};
        zip(archive,"modrinth.index.json",encode(index));bool corrupt=false;
        PackService packs(root.path(),[&](QUrl){return QByteArray(corrupt?"bad":"mod");});
        auto dir=packs.install({{"provider","import"},{"path",archive}},{},"Imported");QCOMPARE(readFile(dir+"/mods/test.jar"),QByteArray("mod"));auto info=ModRepository::read(dir+"/instance.json");QCOMPARE(info["loader"].toString(),QString("fabric"));QCOMPARE(info["mcVersion"].toString(),QString("1.20.1"));QVERIFY(!info["ready"].toBool());
        corrupt=true;QVERIFY_EXCEPTION_THROWN(packs.install({{"provider","import"},{"path",archive}},{},"Broken"),std::runtime_error);QCOMPARE(QDir(root.path()+"/mc/instances").entryList(QDir::Dirs|QDir::NoDotAndDotDot).size(),1);
        QVERIFY_EXCEPTION_THROWN(PackService::safePath("../outside"),std::runtime_error);QVERIFY_EXCEPTION_THROWN(PackService::safePath("C:\\escape"),std::runtime_error);
    }
    void prismAndExportImport(){
        QTemporaryDir root;auto prism=root.path()+"/PrismLauncher/instances/My Pack";QDir().mkpath(prism+"/.minecraft/mods");QDir().mkpath(prism+"/.minecraft/saves/World");
        writeFile(prism+"/instance.cfg","[General]\nname=My Pack\n");writeFile(prism+"/.minecraft/mods/a.jar","mod");writeFile(prism+"/.minecraft/saves/World/level.dat","level");
        writeFile(prism+"/mmc-pack.json",encode(QJsonObject{{"components",QJsonArray{QJsonObject{{"uid","net.minecraft"},{"version","1.20.1"}},QJsonObject{{"uid","net.fabricmc.fabric-loader"},{"version","0.15.11"}}}}}));
        PackService service(root.path());
        auto fromFolder=service.install({{"provider","import"},{"path",prism}},{},"From folder");auto info=ModRepository::read(fromFolder+"/instance.json");
        QCOMPARE(info["mcVersion"].toString(),QString("1.20.1"));QCOMPARE(info["loader"].toString(),QString("fabric"));QCOMPARE(info["loaderVersion"].toString(),QString("0.15.11"));
        QCOMPARE(readFile(fromFolder+"/mods/a.jar"),QByteArray("mod"));QCOMPARE(readFile(fromFolder+"/saves/World/level.dat"),QByteArray("level"));QVERIFY(QFile::exists(prism+"/.minecraft/mods/a.jar"));
        // A MultiMC-style ZIP wraps the instance in one folder.
        Archive::compress(root.path()+"/PrismLauncher/instances",root.path()+"/export.zip");auto fromZip=service.install({{"provider","import"},{"path",root.path()+"/export.zip"}},{},"From zip");
        QCOMPARE(ModRepository::read(fromZip+"/instance.json")["loader"].toString(),QString("fabric"));QCOMPARE(readFile(fromZip+"/saves/World/level.dat"),QByteArray("level"));
        // EBALIA exports carry their own instance.json.
        McInstanceManager manager(root.path());auto zip=root.path()+"/ebalia.zip";manager.exportInstance(fromZip,zip);auto restored=service.install({{"provider","import"},{"path",zip}},{},"Restored");
        auto again=ModRepository::read(restored+"/instance.json");QCOMPARE(again["mcVersion"].toString(),QString("1.20.1"));QCOMPARE(again["loader"].toString(),QString("fabric"));QCOMPARE(again["name"].toString(),QString("Restored"));QVERIFY(QFile::exists(restored+"/mods/a.jar"));
        writeFile(prism+"/mmc-pack.json",encode(QJsonObject{{"components",QJsonArray{QJsonObject{{"uid","net.minecraft"},{"version","1.7.10"}},QJsonObject{{"uid","com.mumfrey.liteloader"},{"version","1.7.10"}}}}}));
        QVERIFY_EXCEPTION_THROWN(service.install({{"provider","import"},{"path",prism}},{},"Lite"),std::runtime_error);
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
    void liveModpackInstall(){
        if(!qEnvironmentVariableIsSet("EBALIA_LIVE_TESTS"))QSKIP("Opt-in network test");QTemporaryDir root;PackService service(root.path());
        auto results=service.search("modrinth","Fabulously Optimized");QVERIFY(!results.isEmpty());auto pack=results.first().toObject();QCOMPARE(pack["name"].toString(),QString("Fabulously Optimized"));
        QJsonObject build;for(auto v:service.versions(pack))if(v.toObject()["data"].toObject()["version_type"]=="release"){build=v.toObject();break;}QVERIFY(!build.isEmpty());
        auto dir=service.install(pack,build,"FO");auto info=ModRepository::read(dir+"/instance.json");QCOMPARE(info["loader"].toString(),QString("fabric"));QVERIFY(!info["mcVersion"].toString().isEmpty());
        QVERIFY(QDir(dir+"/mods").entryList({"*.jar"},QDir::Files).size()>10);if(QImageReader::supportedImageFormats().contains("webp")){QVERIFY(QFile::exists(dir+"/instance-icon.png"));QCOMPARE(info["icon"].toString(),QString("custom"));} // Modrinth icons are WebP (Qt Image Formats)
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

    void javaCompatibility(){
        QVERIFY(JavaRuntime::compatible(8,8));QVERIFY(!JavaRuntime::compatible(8,17));QVERIFY(!JavaRuntime::compatible(17,8));
        QVERIFY(JavaRuntime::compatible(16,17));QVERIFY(JavaRuntime::compatible(17,21));QVERIFY(!JavaRuntime::compatible(21,17));QVERIFY(!JavaRuntime::compatible(17,0));
        QCOMPARE(JavaDownloader::component(8),QString("jre-legacy"));QCOMPARE(JavaDownloader::component(21),QString("java-runtime-delta"));QVERIFY(JavaDownloader::component(11).isEmpty());
#ifndef Q_OS_WIN
        QTemporaryDir tmp;auto java=tmp.path()+"/jdk-21/bin/java";QDir().mkpath(QFileInfo(java).absolutePath());
        writeFile(java,"#!/bin/sh\nprintf '    java.version = 21.0.2\\n    os.arch = aarch64\\n' >&2\n");QFile::setPermissions(java,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        QCOMPARE(JavaRuntime::select(17,java),java);QCOMPARE(JavaRuntime::select(21,tmp.path()+"/jdk-21"),java);QVERIFY(JavaRuntime::select(8,java).isEmpty());
        // Java 6 style runtimes reject -XshowSettings; the plain -version output is used instead.
        auto old=tmp.path()+"/jre6/bin/java";QDir().mkpath(QFileInfo(old).absolutePath());
        writeFile(old,"#!/bin/sh\n[ \"$1\" = -version ] || exit 1\necho 'java version \"1.6.0_45\"' >&2\n");QFile::setPermissions(old,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        QCOMPARE(JavaRuntime::inspect(old).major,6);
#endif
    }
    void javaRuntimeDownload(){
#ifndef Q_OS_WIN
        QTemporaryDir tmp;const QByteArray java="#!/bin/sh\nprintf '    java.version = 1.8.0_51\\n    os.arch = amd64\\n' >&2\n",data="runtime data",license="license";
        auto sha=[](const QByteArray &d){return QString::fromLatin1(QCryptographicHash::hash(d,QCryptographicHash::Sha1).toHex());};
        auto file=[&](const QString &name,const QByteArray &d,bool executable,const QByteArray &packed={},const QString &packedSha={}){QJsonObject downloads{{"raw",QJsonObject{{"sha1",sha(d)},{"size",d.size()},{"url","https://example.test/raw/"+name}}}};if(!packed.isEmpty())downloads["lzma"]=QJsonObject{{"sha1",packedSha.isEmpty()?sha(packed):packedSha},{"size",packed.size()},{"url","https://example.test/lzma/"+name}};return QJsonObject{{"type","file"},{"executable",executable},{"downloads",downloads}};};
        auto packedJava=lzma(java);
        QJsonObject files{{"bin",QJsonObject{{"type","directory"}}},{"bin/java",file("java",java,true,packedJava)},{"lib/data",file("data",data,false,"broken","0000000000000000000000000000000000000000")},
            {"legal/LICENSE",file("license",license,false)},{"legal/base/LICENSE",QJsonObject{{"type","link"},{"target","../LICENSE"}}}};
        auto manifest=encode(QJsonObject{{"files",files}});QStringList requested;QMutex mutex; // files are fetched from several threads
        auto transport=[&](const QUrl &url)->QByteArray{{QMutexLocker lock(&mutex);requested<<url.toString();}auto path=url.path();
            if(path.endsWith("all.json"))return encode(QJsonObject{{"linux",QJsonObject{{"jre-legacy",QJsonArray{QJsonObject{{"manifest",QJsonObject{{"sha1",sha(manifest)},{"url","https://example.test/manifest.json"}}},{"version",QJsonObject{{"name","8u51"}}}}}}}}});
            if(path=="/manifest.json")return manifest;if(path=="/lzma/java")return packedJava;if(path=="/raw/data")return data;if(path=="/raw/license")return license;if(path=="/lzma/data")return "broken";
            throw std::runtime_error(("Unexpected request "+url.toString()).toStdString());};
        auto path=JavaDownloader::install(tmp.path()+"/java","jre-legacy","linux",{},transport);
        QCOMPARE(path,tmp.path()+"/java/jre-legacy-linux/bin/java");QVERIFY(QFileInfo(path).isExecutable());QVERIFY(!requested.contains("https://example.test/raw/java"));
        QCOMPARE(readFile(tmp.path()+"/java/jre-legacy-linux/lib/data"),data);QCOMPARE(readFile(tmp.path()+"/java/jre-legacy-linux/legal/base/LICENSE"),license);
        QCOMPARE(JavaRuntime::inspect(path).major,8);QCOMPARE(JavaDownloader::installed(tmp.path()+"/java","jre-legacy","linux"),path);
        requested.clear();QCOMPARE(JavaDownloader::install(tmp.path()+"/java","jre-legacy","linux",{},transport),path);QCOMPARE(requested.size(),1); // only the catalog: the runtime is reused
        files["bin/evil"]=QJsonObject{{"type","link"},{"target","../../../outside"}};manifest=encode(QJsonObject{{"files",files}});QDir(tmp.path()+"/java/jre-legacy-linux").removeRecursively();
        QVERIFY_EXCEPTION_THROWN(JavaDownloader::install(tmp.path()+"/java","jre-legacy","linux",{},transport),std::runtime_error);QVERIFY(!QFileInfo::exists(tmp.path()+"/java/jre-legacy-linux"));
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
    void liveCurseForge(){
        if(!qEnvironmentVariableIsSet("EBALIA_LIVE_TESTS"))QSKIP("Opt-in network test");
        QVERIFY2(!ModRepository::curseForgeKey().isEmpty(),"Configure a CurseForge API key before running this test");
        QTemporaryDir root;McInstanceManager manager(root.path());ModRepository repo(root.path());
        auto hits=repo.search("Just Enough Items","1.20.1","forge",0,"curseforge");
        QJsonObject jei;for(const auto &hit:hits)if(hit.toObject()["slug"]=="jei")jei=hit.toObject();
        QVERIFY2(!jei.isEmpty(),"CurseForge search did not return JEI for Minecraft 1.20.1 / Forge");
        auto plan=repo.plan(QJsonArray{QJsonObject{{"project_id",jei["project_id"]},{"name",jei["title"]}}},"1.20.1","forge");
        QVERIFY2(plan["missing"].toArray().isEmpty(),QJsonDocument(plan).toJson().constData());
        QVERIFY(!plan["versions"].toArray().isEmpty());
        auto dir=manager.createInstance("CurseForge live test","1.20.1","forge");repo.apply(dir,plan);
        for(const auto &version:plan["versions"].toArray()){
            const auto file=version.toObject()["file"].toObject();auto data=readFile(dir+"/mods/"+file["filename"].toString());
            QVERIFY(!data.isEmpty());QCOMPARE(QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha1).toHex()),file["hashes"].toObject()["sha1"].toString());
            qInfo().noquote()<<"Downloaded and verified:"<<file["filename"].toString()<<data.size()<<"bytes";
        }
        auto captured=repo.capture(dir,"CurseForge live pack");QVERIFY(!captured["projects"].toArray().isEmpty());
        PackService service(root.path());auto packs=service.search("curseforge","SkyFactory 4");QVERIFY(!packs.isEmpty());
        auto pack=packs.first().toObject();QCOMPARE(pack["provider"].toString(),QString("curseforge"));
        auto versions=service.versions(pack);QVERIFY(!versions.isEmpty());
        qInfo().noquote()<<"Modpack catalog:"<<pack["name"].toString()<<versions.size()<<"versions";
    }
    // ---- Modpack import: every format the Import page lists, with each loader ----
    void importLoaderDetection(){
        QTemporaryDir root;PackService service(root.path(),[](const QUrl &url)->QByteArray{throw std::runtime_error(("unexpected network request: "+url.toString()).toStdString());});
        int n=0;auto next=[&]{return root.path()+"/fixture-"+QString::number(++n);};
        auto zipOf=[&](const Files &files,const QString &ext=".zip"){auto path=next()+ext;zipEntries(path,files);return QJsonObject{{"provider","import"},{"path",path}};};
        auto folderOf=[&](const Files &files){auto path=next();writeTree(path,files);return QJsonObject{{"provider","import"},{"path",path}};};
        struct Case{QString label;QJsonObject pack;QString expected;bool mod=true;};
        QList<Case> cases;
        // Modrinth (.mrpack): loader keys of "dependencies".
        auto mr=[&](QJsonObject deps){return zipOf({{"modrinth.index.json",mrIndex(deps)},{"overrides/mods/a.jar","mod"}},".mrpack");};
        cases<<Case{"mrpack fabric",mr({{"minecraft","1.21.1"},{"fabric-loader","0.16.7"}}),"1.21.1 fabric 0.16.7"}
             <<Case{"mrpack quilt",mr({{"minecraft","1.20.1"},{"quilt-loader","0.29.1"}}),"1.20.1 quilt 0.29.1"}
             <<Case{"mrpack forge",mr({{"minecraft","1.20.1"},{"forge","47.1.44"}}),"1.20.1 forge 47.1.44"}
             <<Case{"mrpack forge 1.12.2",mr({{"minecraft","1.12.2"},{"forge","14.23.5.2860"}}),"1.12.2 forge 14.23.5.2860"}
             <<Case{"mrpack neoforge",mr({{"minecraft","1.21.1"},{"neoforge","21.1.230"}}),"1.21.1 neoforge 21.1.230"}
             <<Case{"mrpack neoforge 1.20.1",mr({{"minecraft","1.20.1"},{"neoforge","47.1.106"}}),"1.20.1 neoforge 47.1.106"}
             <<Case{"mrpack vanilla",mr({{"minecraft","1.21.1"}}),"1.21.1 vanilla"};
        // CurseForge (.zip with manifest.json): modLoaders ids exactly as CurseForge writes them.
        auto cf=[&](QString game,QJsonArray loaders){return zipOf({{"manifest.json",cfManifest(game,loaders)},{"modlist.html","<ul></ul>"},{"overrides/mods/a.jar","mod"}});};
        auto id=[](QString id,bool primary=true){return QJsonObject{{"id",id},{"primary",primary}};};
        cases<<Case{"curseforge forge",cf("1.20.1",{id("forge-47.4.10")}),"1.20.1 forge 47.4.10"}
             <<Case{"curseforge forge 1.12.2",cf("1.12.2",{id("forge-14.23.5.2860")}),"1.12.2 forge 14.23.5.2860"}
             <<Case{"curseforge fabric",cf("1.20.1",{id("fabric-0.19.5")}),"1.20.1 fabric 0.19.5"}
             <<Case{"curseforge quilt",cf("1.20.1",{id("quilt-0.22.0")}),"1.20.1 quilt 0.22.0"}
             <<Case{"curseforge neoforge 1.21.1",cf("1.21.1",{id("neoforge-21.1.77")}),"1.21.1 neoforge 21.1.77"}
             <<Case{"curseforge neoforge 1.20.1",cf("1.20.1",{id("neoforge-1.20.1-47.1.106")}),"1.20.1 neoforge 47.1.106"}
             <<Case{"curseforge primary loader",cf("1.20.1",{id("fabric-0.15.11",false),id("forge-47.2.0")}),"1.20.1 forge 47.2.0"}
             <<Case{"curseforge fabric with game suffix",cf("1.20.1",{id("fabric-0.15.11-1.20.1")}),"1.20.1 fabric 0.15.11"};
        // Prism Launcher / PolyMC / MultiMC components.
        auto prism=[&](QString game,QString uid,QString version,QString gameDir="minecraft"){return folderOf({{"instance.cfg","[General]\nInstanceType=OneSix\nname=Test\n"},{"mmc-pack.json",mmcPack(game,uid,version)},{gameDir+"/mods/a.jar","mod"}});};
        cases<<Case{"prism fabric",prism("1.20.1","net.fabricmc.fabric-loader","0.15.11"),"1.20.1 fabric 0.15.11"}
             <<Case{"prism quilt",prism("1.20.1","org.quiltmc.quilt-loader","0.26.0"),"1.20.1 quilt 0.26.0"}
             <<Case{"prism forge",prism("1.20.1","net.minecraftforge","47.2.0",".minecraft"),"1.20.1 forge 47.2.0"}
             <<Case{"prism forge 1.12.2",prism("1.12.2","net.minecraftforge","14.23.5.2860",".minecraft"),"1.12.2 forge 14.23.5.2860"}
             <<Case{"prism neoforge",prism("1.21.1","net.neoforged","21.1.77"),"1.21.1 neoforge 21.1.77"}
             <<Case{"prism neoforge 1.20.1",prism("1.20.1","net.neoforged","47.1.106"),"1.20.1 neoforge 47.1.106"}
             <<Case{"prism vanilla",prism("1.21.1",{},{}),"1.21.1 vanilla"};
        // CurseForge app instance folders: baseModLoader objects as api.curseforge.com/v1/minecraft/modloader/<name> returns them.
        auto cfApp=[&](QString game,QJsonValue loader){return folderOf({{"minecraftinstance.json",encode(QJsonObject{{"name","Test"},{"gameVersion",game},{"baseModLoader",loader}})},{"mods/a.jar","mod"}});};
        auto base=[](QString name,int type,QString version,QString game){return QJsonObject{{"name",name},{"type",type},{"forgeVersion",version},{"minecraftVersion",game}};};
        cases<<Case{"curseforge app forge",cfApp("1.20.1",base("forge-47.2.0",1,"47.2.0","1.20.1")),"1.20.1 forge 47.2.0"}
             <<Case{"curseforge app fabric",cfApp("1.20.1",base("fabric-0.15.11-1.20.1",4,"0.15.11","1.20.1")),"1.20.1 fabric 0.15.11"}
             <<Case{"curseforge app quilt",cfApp("1.20.1",base("quilt-0.30.1-1.20.1",5,"0.30.1","1.20.1")),"1.20.1 quilt 0.30.1"}
             <<Case{"curseforge app neoforge",cfApp("1.21.1",base("neoforge-21.1.77",6,"21.1.77","1.21.1")),"1.21.1 neoforge 21.1.77"}
             <<Case{"curseforge app neoforge 1.20.1",cfApp("1.20.1",base("neoforge-1.20.1-47.1.99",6,"1.20.1-47.1.99","1.20.1")),"1.20.1 neoforge 47.1.99"}
             <<Case{"curseforge app vanilla",cfApp("1.20.1",QJsonValue::Null),"1.20.1 vanilla"};
        // FTB App instance folders: modLoader holds the installed version id (FTB-App InstanceInstaller, *InstallTask.getModLoaderTarget()).
        auto ftbApp=[&](QString game,QString modLoader,QString provider,QByteArray versionJson={}){Files f{{"instance.json",encode(QJsonObject{{"uuid","00000000-0000-0000-0000-000000000000"},{"id",1},{"versionId",1},{"name","FTB"},{"version","1.0"},{"mcVersion",game},{"modLoader",modLoader}})},{"mods/a.jar","mod"}};if(!versionJson.isEmpty())f<<qMakePair(QString(".ftbapp/version.json"),versionJson);auto pack=folderOf(f);pack["provider"]=provider;return pack;};
        auto targets=[](QString game,QString loader,QString version){return encode(QJsonObject{{"targets",QJsonArray{QJsonObject{{"type","game"},{"name","minecraft"},{"version",game}},QJsonObject{{"type","modloader"},{"name",loader},{"version",version}}}}});};
        for(QString provider:{"import","import_ftb"}){
            cases<<Case{"ftb app neoforge ("+provider+")",ftbApp("1.21.1","neoforge-21.1.77",provider),"1.21.1 neoforge 21.1.77"}
                 <<Case{"ftb app forge ("+provider+")",ftbApp("1.20.1","1.20.1-forge-47.2.0",provider),"1.20.1 forge 47.2.0"}
                 <<Case{"ftb app forge 1.12.2 ("+provider+")",ftbApp("1.12.2","1.12.2-forge-14.23.5.2860",provider),"1.12.2 forge 14.23.5.2860"}
                 <<Case{"ftb app fabric ("+provider+")",ftbApp("1.20.1","fabric-loader-1.20.1-0.15.11",provider),"1.20.1 fabric 0.15.11"}
                 <<Case{"ftb app vanilla ("+provider+")",ftbApp("1.20.1","1.20.1",provider),"1.20.1 vanilla"}
                 <<Case{"ftb app version.json ("+provider+")",ftbApp("1.20.1","1.20.1-forge-47.2.0",provider,targets("1.20.1","forge","47.2.0")),"1.20.1 forge 47.2.0"};
        }
        // ATLauncher instance folders and their ZIPs (instance.json: "id" is the game, launcher.loaderVersion the loader).
        auto atl=[&](QString game,QString type,QString version,bool zipped){QJsonObject launcher{{"name","ATL"},{"pack","ATL"},{"version","1"}};if(!type.isEmpty())launcher["loaderVersion"]=QJsonObject{{"type",type},{"version",version},{"rawVersion",version}};Files f{{"instance.json",encode(QJsonObject{{"id",game},{"type","release"},{"mainClass","x"},{"launcher",launcher}})},{"mods/a.jar","mod"}};return zipped?zipOf(f):folderOf(f);};
        cases<<Case{"atlauncher fabric",atl("1.20.1","Fabric","0.15.11",false),"1.20.1 fabric 0.15.11"}
             <<Case{"atlauncher quilt zip",atl("1.20.1","Quilt","0.26.0",true),"1.20.1 quilt 0.26.0"}
             <<Case{"atlauncher forge zip",atl("1.20.1","Forge","47.2.0",true),"1.20.1 forge 47.2.0"}
             <<Case{"atlauncher neoforge",atl("1.21.1","NeoForge","21.1.77",false),"1.21.1 neoforge 21.1.77"}
             <<Case{"atlauncher vanilla",atl("1.21.1",{},{},false),"1.21.1 vanilla"};
        // Technic (.zip with bin/modpack.jar or bin/version.json); profiles trimmed from the real installers and meta servers.
        auto technic=[&](QByteArray versionJson,bool inJar=true,bool onlyBin=false){Files f;if(inJar)f<<qMakePair(QString("bin/modpack.jar"),zipBytes({{"version.json",versionJson}}));else f<<qMakePair(QString("bin/version.json"),versionJson);if(!onlyBin)f<<qMakePair(QString("mods/a.jar"),QByteArray("mod"))<<qMakePair(QString("config/a.cfg"),QByteArray("cfg"));return zipOf(f);};
        auto forge1710=profile("1.7.10-Forge10.13.4.1614-1.7.10","1.7.10",{"net.minecraftforge:forge:1.7.10-10.13.4.1614-1.7.10","net.minecraft:launchwrapper:1.12"});
        auto forge1201=profile("1.20.1-forge-47.2.0","1.20.1",{"net.minecraftforge:forgespi:7.0.1","net.minecraftforge:fmlloader:1.20.1-47.2.0"},{"--launchTarget","forgeclient","--fml.forgeVersion","47.2.0","--fml.mcVersion","1.20.1","--fml.forgeGroup","net.minecraftforge"});
        auto neo1211=profile("neoforge-21.1.77","1.21.1",{"net.neoforged.fancymodloader:loader:4.0.31@jar","net.neoforged:mergetool:2.0.3:api@jar"},{"--fml.neoForgeVersion","21.1.77","--fml.fmlVersion","4.0.31","--fml.mcVersion","1.21.1","--launchTarget","forgeclient"});
        auto neo1201=profile("1.20.1-forge-47.1.106","1.20.1",{"net.neoforged.fancymodloader:loader:47.2.2","net.minecraftforge:forgespi:7.0.1"},{"--launchTarget","forgeclient","--fml.forgeVersion","47.1.106","--fml.fmlVersion","47.2.2","--fml.mcVersion","1.20.1"});
        auto fabric=profile("fabric-loader-0.15.11-1.20.1","1.20.1",{"net.fabricmc:intermediary:1.20.1","net.fabricmc:fabric-loader:0.15.11"});
        auto quilt=profile("quilt-loader-0.26.0-1.20.1","1.20.1",{"org.quiltmc:hashed:1.20.1","org.quiltmc:quilt-loader:0.26.0"});
        cases<<Case{"technic forge 1.7.10",technic(forge1710),"1.7.10 forge 10.13.4.1614-1.7.10"}
             <<Case{"technic forge 1.20.1",technic(forge1201),"1.20.1 forge 47.2.0"}
             <<Case{"technic neoforge 1.21.1",technic(neo1211),"1.21.1 neoforge 21.1.77"}
             <<Case{"technic neoforge 1.20.1",technic(neo1201),"1.20.1 neoforge 47.1.106"}
             <<Case{"technic fabric",technic(fabric),"1.20.1 fabric 0.15.11"}
             <<Case{"technic quilt bin/version.json",technic(quilt,false),"1.20.1 quilt 0.26.0"}
             <<Case{"technic zip with only bin/",technic(forge1201,true,true),"1.20.1 forge 47.2.0",false};
        // GDLauncher (config.json) and Modrinth App 0.7 (profile.json) instance folders.
        cases<<Case{"gdlauncher forge",folderOf({{"config.json",encode(QJsonObject{{"loader",QJsonObject{{"loaderType","forge"},{"mcVersion","1.16.5"},{"loaderVersion","1.16.5-36.2.39"}}}})},{"mods/a.jar","mod"}}),"1.16.5 forge 36.2.39"}
             <<Case{"gdlauncher fabric",folderOf({{"config.json",encode(QJsonObject{{"loader",QJsonObject{{"loaderType","fabric"},{"mcVersion","1.20.1"},{"loaderVersion","0.15.11"}}}})},{"mods/a.jar","mod"}}),"1.20.1 fabric 0.15.11"}
             <<Case{"modrinth app profile",folderOf({{"profile.json",encode(QJsonObject{{"path","Test"},{"metadata",QJsonObject{{"name","Test"},{"game_version","1.20.1"},{"loader","forge"},{"loader_version",QJsonObject{{"id","1.20.1-47.2.0"}}}}}})},{"mods/a.jar","mod"}}),"1.20.1 forge 47.2.0"};
        // EBALIA exports keep their own instance.json.
        {McInstanceManager manager(root.path());auto made=manager.createInstance("Exported","1.21.1","neoforge","21.1.77");writeFile(made+"/mods/a.jar","mod");auto path=next()+".zip";manager.exportInstance(made,path);cases<<Case{"ebalia export",QJsonObject{{"provider","import"},{"path",path}},"1.21.1 neoforge 21.1.77"};}
        QStringList failures;
        for(const auto &c:cases){
            auto info=importPack(service,c.pack);auto got=describe(info);
            if(got!=c.expected)failures<<c.label+": expected \""+c.expected+"\", got \""+got+"\"";
            else if(c.mod&&readFile(info["dir"].toString()+"/mods/a.jar")!="mod")failures<<c.label+": mods/a.jar was not imported";
        }
        for(const auto &f:failures)qWarning().noquote()<<"IMPORT"<<f;
        QVERIFY2(failures.isEmpty(),qPrintable(QString::number(failures.size())+" of "+QString::number(cases.size())+" imports are wrong (see warnings)"));
    }
    void importArchiveLayouts(){
        QTemporaryDir root;PackService service(root.path(),[](const QUrl &url)->QByteArray{throw std::runtime_error(("unexpected network request: "+url.toString()).toStdString());});
        auto instance=[](QString gameDir){return Files{{"instance.cfg","[General]\nname=My Pack\n"},{"mmc-pack.json",mmcPack("1.20.1","net.fabricmc.fabric-loader","0.15.11")},{gameDir+"/mods/a.jar","mod"},{gameDir+"/saves/World/level.dat","level"}};};
        auto cfPack=Files{{"manifest.json",cfManifest("1.20.1",{QJsonObject{{"id","fabric-0.15.11"},{"primary",true}}})},{"overrides/mods/a.jar","mod"},{"overrides/saves/World/level.dat","level"}};
        auto mac=Files{{"__MACOSX/My Pack/._instance.cfg","apple"},{"__MACOSX/._My Pack","apple"}};
        QList<QPair<QString,Files>> layouts{
            {"prism export, minecraft/ at the root",instance("minecraft")},
            {"prism export, .minecraft/ at the root",instance(".minecraft")},
            {"multimc export inside one folder",prefixed("My Pack/",instance(".minecraft"))},
            {"zip made on Windows (backslashes)",prefixed("My Pack\\",Files{{"instance.cfg","[General]\n"},{"mmc-pack.json",mmcPack("1.20.1","net.fabricmc.fabric-loader","0.15.11")},{"minecraft\\mods\\a.jar","mod"},{"minecraft\\saves\\World\\level.dat","level"}})},
            {"macOS Finder zip (__MACOSX next to the folder)",prefixed("My Pack/",instance("minecraft"))+mac},
            {".DS_Store next to the folder",prefixed("My Pack/",instance("minecraft"))+Files{{".DS_Store","ds"}}},
            {"folder inside a folder",prefixed("Export/My Pack/",instance("minecraft"))},
            {"curseforge zip inside one folder",prefixed("Pack/",cfPack)},
            {"curseforge zip inside one folder with __MACOSX",prefixed("Pack/",cfPack)+Files{{"__MACOSX/Pack/._manifest.json","apple"}}},
        };
        QStringList failures;int i=0;
        for(const auto &[label,files]:layouts){
            auto path=root.path()+"/layout-"+QString::number(++i)+".zip";zipEntries(path,files);auto info=importPath(service,path);auto dir=info["dir"].toString();
            if(describe(info)!="1.20.1 fabric 0.15.11")failures<<label+": got \""+describe(info)+"\"";
            else if(readFile(dir+"/mods/a.jar")!="mod"||readFile(dir+"/saves/World/level.dat")!="level")failures<<label+": game files not at the instance root";
            else if(QFile::exists(dir+"/__MACOSX")||QFile::exists(dir+"/My Pack")||QFile::exists(dir+"/minecraft")||QFile::exists(dir+"/.minecraft"))failures<<label+": wrapper folders copied into the instance";
        }
        for(const auto &f:failures)qWarning().noquote()<<"LAYOUT"<<f;
        QVERIFY2(failures.isEmpty(),qPrintable(QString::number(failures.size())+" of "+QString::number(layouts.size())+" layouts fail (see warnings)"));
    }
    void importModrinthFiles(){
        // Modelled on the real "The Pixelmon Modpack 9.2.3" index: Windows separators, and client-"optional" mods that required mods depend on.
        QTemporaryDir root;QMap<QString,QByteArray> served;
        PackService service(root.path(),[&](const QUrl &url){if(!served.contains(url.toString()))throw std::runtime_error(("404 "+url.toString()).toStdString());return served[url.toString()];});
        QJsonArray files;
        auto file=[&](QString path,QString client,QByteArray data,bool mirror=false){
            auto url="https://cdn.modrinth.com/data/"+QString::number(files.size())+"/"+QFileInfo(QString(path).replace('\\','/')).fileName();served[url]=data;QJsonArray urls{url};if(mirror)urls.prepend("https://cdn.example.org/gone.jar");
            files.append(QJsonObject{{"path",path},{"hashes",QJsonObject{{"sha1",QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha1).toHex())},{"sha512",QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha512).toHex())}}},{"env",QJsonObject{{"client",client},{"server","optional"}}},{"downloads",urls},{"fileSize",data.size()}});
        };
        file("mods\\Pixelmon-1.20.1-9.2.3-universal.jar","required","pixelmon");
        file("mods\\konkrete_forge_1.6.1-2_MC_1.20.jar","optional","konkrete");
        file("mods\\journeymap-1.20.1-5.9.12-forge.jar","optional","journeymap");
        file("mods\\fancymenu_forge_2.14.9_MC_1.20.1.jar","required","fancymenu");
        file("mods\\server-only.jar","unsupported","server");
        file("mods/mirrored.jar","required","mirror",true);
        auto path=root.path()+"/pixelmon.mrpack";
        zipEntries(path,{{"modrinth.index.json",mrIndex({{"minecraft","1.20.1"},{"forge","47.1.44"}},files)},{"overrides/config/a.toml","base"},{"overrides/options.txt","base"},{"client-overrides/config/a.toml","client"},{"server-overrides/server.properties","server"}});
        auto info=importPath(service,path);QVERIFY2(!info.contains("error"),qPrintable(describe(info)));auto dir=info["dir"].toString();
        QCOMPARE(describe(info),QString("1.20.1 forge 47.1.44"));
        QCOMPARE(readFile(dir+"/mods/Pixelmon-1.20.1-9.2.3-universal.jar"),QByteArray("pixelmon"));QCOMPARE(readFile(dir+"/mods/fancymenu_forge_2.14.9_MC_1.20.1.jar"),QByteArray("fancymenu"));
        QCOMPARE(readFile(dir+"/config/a.toml"),QByteArray("client"));QCOMPARE(readFile(dir+"/options.txt"),QByteArray("base"));
        QVERIFY(!QFile::exists(dir+"/mods/server-only.jar"));QVERIFY(!QFile::exists(dir+"/server.properties"));
        // FancyMenu requires Konkrete: client-"optional" files are installed, as the Modrinth App and Prism Launcher do by default.
        QCOMPARE(readFile(dir+"/mods/konkrete_forge_1.6.1-2_MC_1.20.jar"),QByteArray("konkrete"));
        QCOMPARE(readFile(dir+"/mods/journeymap-1.20.1-5.9.12-forge.jar"),QByteArray("journeymap"));
        // "downloads" lists mirrors: a dead first URL falls back to the next one.
        QCOMPARE(readFile(dir+"/mods/mirrored.jar"),QByteArray("mirror"));
    }
    void importCurseForgeFiles(){
        // CurseForge manifests list resource packs and shaders next to mods (3 of 41 files in the real "Into the Backrooms" Fabric pack, 14 of 18 in "X-Ray Unlimited").
        QTemporaryDir root;QMap<QString,QByteArray> served;
        auto cfFile=[&](int project,int id,QString name,QByteArray data,int classId){
            QJsonObject f{{"id",id},{"modId",project},{"fileName",name},{"displayName",name},{"downloadUrl","https://edge.forgecdn.net/files/"+QString::number(id/1000)+"/"+QString::number(id%1000)+"/"+name},{"hashes",QJsonArray{QJsonObject{{"algo",1},{"value",QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha1).toHex())}}}}};
            served[f["downloadUrl"].toString()]=data;served["/v1/mods/"+QString::number(project)+"/files/"+QString::number(id)]=encode(QJsonObject{{"data",f}});
            served["/v1/mods/"+QString::number(project)]=encode(QJsonObject{{"data",QJsonObject{{"id",project},{"classId",classId},{"name",name}}}});
        };
        cfFile(1,1001,"sodium.jar","sodium",6);cfFile(2,2002,"FreshAnimations_v1.10.4.zip","rp",12);cfFile(3,3003,"ComplementaryShaders.zip","shader",6552);cfFile(4,4004,"optional.jar","opt",6);
        PackService service(root.path(),[&](const QUrl &url){auto key=url.host()=="api.curseforge.com"?url.path():url.toString();if(!served.contains(key))throw std::runtime_error(("404 "+url.toString()).toStdString());return served[key];});
        auto ref=[](int p,int f,bool required=true){return QJsonObject{{"projectID",p},{"fileID",f},{"required",required}};};
        auto path=root.path()+"/cf.zip";zipEntries(path,{{"manifest.json",cfManifest("1.20.1",{QJsonObject{{"id","fabric-0.15.11"},{"primary",true}}},{ref(1,1001),ref(2,2002),ref(3,3003),ref(4,4004,false)})},{"overrides/options.txt","resourcePacks:[\"vanilla\",\"file/FreshAnimations_v1.10.4.zip\"]"}});
        auto info=importPath(service,path);QVERIFY2(!info.contains("error"),qPrintable(describe(info)));auto dir=info["dir"].toString();
        QCOMPARE(readFile(dir+"/mods/sodium.jar"),QByteArray("sodium"));QVERIFY(!QFile::exists(dir+"/mods/optional.jar"));
        QCOMPARE(readFile(dir+"/resourcepacks/FreshAnimations_v1.10.4.zip"),QByteArray("rp"));QVERIFY(!QFile::exists(dir+"/mods/FreshAnimations_v1.10.4.zip"));
        QCOMPARE(readFile(dir+"/shaderpacks/ComplementaryShaders.zip"),QByteArray("shader"));QVERIFY(!QFile::exists(dir+"/mods/ComplementaryShaders.zip"));
    }
    void importUnicodeNames(){
        QTemporaryDir root;PackService service(root.path(),[](const QUrl &)->QByteArray{throw std::runtime_error("offline");});
        auto path=root.path()+"/unicode.mrpack";
        zipEntries(path,{{"modrinth.index.json",mrIndex({{"minecraft","1.20.1"},{"fabric-loader","0.15.11"}})},{"overrides/saves/Mundo Ñandú ★/level.dat","level"},{"overrides/config/日本語.txt","jp"},{"overrides/resourcepacks/Café.zip","rp"}});
        auto check=[&](const QString &label){
            auto info=importPath(service,path);QVERIFY2(!info.contains("error"),qPrintable(label+": "+describe(info)));auto dir=info["dir"].toString();
            QVERIFY2(readFile(dir+"/saves/Mundo Ñandú ★/level.dat")=="level",qPrintable(label+": "+QDir(dir+"/saves").entryList(QDir::Dirs|QDir::NoDotAndDotDot).join(", ")));
            QCOMPARE(readFile(dir+"/config/日本語.txt"),QByteArray("jp"));QCOMPARE(readFile(dir+"/resourcepacks/Café.zip"),QByteArray("rp"));
        };
        check("UTF-8 locale");
        // Windows keeps narrow file names in the ANSI code page: emulate a C library locale that is not UTF-8.
        struct Restore{QByteArray saved=setlocale(LC_CTYPE,nullptr);~Restore(){setlocale(LC_CTYPE,saved.constData());}}restore;
        QVERIFY(setlocale(LC_CTYPE,"C"));check("C locale");
    }
    void importLauncherFoldersAndMods(){
        QTemporaryDir root;PackService service(root.path(),[](const QUrl &url)->QByteArray{throw std::runtime_error(("offline: "+url.toString()).toStdString());});
        QDir().mkpath(root.path()+"/mc");writeFile(root.path()+"/mc/manifest.json",encode(QJsonObject{{"versions",QJsonArray{QJsonObject{{"id","1.21.1"},{"type","release"}},QJsonObject{{"id","1.20.4"},{"type","release"}},QJsonObject{{"id","1.20.1"},{"type","release"}},QJsonObject{{"id","1.20"},{"type","release"}}}}}));
        auto fabricJar=[](QString id,QString mc){return zipBytes({{"fabric.mod.json",encode(QJsonObject{{"schemaVersion",1},{"id",id},{"name",id},{"depends",QJsonObject{{"minecraft",mc},{"fabricloader",">=0.15"}}}})}});};
        auto forgeJar=[](QString name,QString range){return zipBytes({{"META-INF/mods.toml",QString("modLoader=\"javafml\"\n[[mods]]\nmodId=\"x\"\ndisplayName=\"%1\"\n[[dependencies.x]]\nmodId=\"forge\"\nversionRange=\"[47,)\"\n[[dependencies.x]]\nmodId=\"minecraft\"\nmandatory=true\nversionRange=\"%2\"\n").arg(name,range).toUtf8()}});};
        // TLauncher version folder, zipped as is: merged Forge profile without inheritsFrom.
        auto tlauncher=encode(QJsonObject{{"id","EsquizosMineZ"},{"mainClass","cpw.mods.bootstraplauncher.BootstrapLauncher"},{"libraries",QJsonArray{QJsonObject{{"name","net.minecraftforge:fmlloader:1.20.1-47.2.0"}}}},{"arguments",QJsonObject{{"game",QJsonArray{"--launchTarget","forgeclient","--fml.forgeVersion","47.2.0","--fml.mcVersion","1.20.1","--fml.forgeGroup","net.minecraftforge"}}}}});
        auto path=root.path()+"/tlauncher.zip";zipEntries(path,prefixed("Ebalia Prueba/",{{"EsquizosMineZ.json",tlauncher},{"EsquizosMineZ.jar","client"},{"TLauncherAdditional.json","{}"},{"options.txt","fov:0"},{"mods/journeymap.jar",forgeJar("JourneyMap","[1.20.1,1.20.2)")},{"config/a.toml","cfg"},{"saves/World/level.dat","level"},{"natives/lwjgl.dll","dll"}}));
        auto info=importPath(service,path);auto dir=info["dir"].toString();QCOMPARE(describe(info),QString("1.20.1 forge 47.2.0"));
        QCOMPARE(readFile(dir+"/saves/World/level.dat"),QByteArray("level"));QCOMPARE(readFile(dir+"/options.txt"),QByteArray("fov:0"));QVERIFY(QFile::exists(dir+"/mods/journeymap.jar"));
        for(auto name:{"EsquizosMineZ.json","EsquizosMineZ.jar","TLauncherAdditional.json","natives"})QVERIFY2(!QFile::exists(dir+"/"+name),name);
        QVERIFY(info["compatibilityWarnings"].toArray().isEmpty());
        // Official Minecraft Launcher .minecraft: the profile played last; game files and accounts stay out.
        auto profiles=encode(QJsonObject{{"profiles",QJsonObject{{"a",QJsonObject{{"lastVersionId","1.20.1"},{"lastUsed","2026-01-01T10:00:00.000Z"}}},{"b",QJsonObject{{"lastVersionId","fabric-loader-0.15.11-1.20.1"},{"lastUsed","2026-05-01T10:00:00.000Z"}}}}}});
        path=root.path()+"/official.zip";zipEntries(path,prefixed(".minecraft/",{{"launcher_profiles.json",profiles},{"launcher_accounts.json","{\"accessToken\":\"secret\"}"},{"versions/1.20.1/1.20.1.json",encode(QJsonObject{{"id","1.20.1"},{"mainClass","x"}})},{"versions/fabric-loader-0.15.11-1.20.1/fabric-loader-0.15.11-1.20.1.json",profile("fabric-loader-0.15.11-1.20.1","1.20.1",{"net.fabricmc:intermediary:1.20.1","net.fabricmc:fabric-loader:0.15.11"})},{"assets/indexes/5.json","{}"},{"mods/sodium.jar",fabricJar("sodium","~1.20.1")},{"saves/W/level.dat","level"}}));
        info=importPath(service,path);dir=info["dir"].toString();QCOMPARE(describe(info),QString("1.20.1 fabric 0.15.11"));
        for(auto name:{"launcher_accounts.json","launcher_profiles.json","versions","assets"})QVERIFY2(!QFile::exists(dir+"/"+name),name);QCOMPARE(readFile(dir+"/saves/W/level.dat"),QByteArray("level"));
        // Only mods: the newest release every mod accepts.
        path=root.path()+"/mods.zip";zipEntries(path,{{"mods/sodium.jar",fabricJar("sodium",">=1.20 <1.21")},{"mods/lithium.jar",fabricJar("lithium","~1.20.1")},{"config/x.json","{}"}});
        QCOMPARE(describe(importPath(service,path)),QString("1.20.1 fabric"));
        path=root.path()+"/forge-mods.zip";zipEntries(path,{{"mods/create.jar",forgeJar("Create","[1.20.1,1.20.2)")},{"mods/jei.jar",forgeJar("JEI","[1.20,1.21)")}});
        QCOMPARE(describe(importPath(service,path)),QString("1.20.1 forge"));
        // A pack's own version is kept and mods that do not fit are reported.
        path=root.path()+"/mixed.mrpack";zipEntries(path,{{"modrinth.index.json",mrIndex({{"minecraft","1.20.1"},{"forge","47.2.0"}})},{"overrides/mods/sodium.jar",fabricJar("Sodium","~1.20.1")},{"overrides/mods/create.jar",forgeJar("Create","[1.21,1.22)")}});
        info=importPath(service,path);QCOMPARE(describe(info),QString("1.20.1 forge 47.2.0"));
        const auto warnings=info["compatibilityWarnings"].toArray();QCOMPARE(warnings.size(),2);
        QVERIFY(warnings[0].toString().contains("Create")||warnings[1].toString().contains("Create"));QVERIFY(warnings[0].toString().contains("Sodium")||warnings[1].toString().contains("Sodium"));
        // Nothing that names a version.
        path=root.path()+"/world.zip";zipEntries(path,{{"saves/W/level.dat","level"}});QVERIFY(describe(importPath(service,path)).startsWith("error: Could not detect"));
    }
    void minecraftFoldersAreFoundAndImported(){
#ifdef Q_OS_LINUX
        // ~/.minecraft as TLauncher leaves it: versions with their own game folder (mods, saves) and a shared one.
        QTemporaryDir home;const auto previous=qgetenv("HOME");qputenv("HOME",home.path().toUtf8());auto restore=qScopeGuard([&]{qputenv("HOME",previous);});
        const auto mc=home.path()+"/.minecraft";
        auto forgeJar=zipBytes({{"META-INF/mods.toml",QByteArray("[[mods]]\nmodId=\"journeymap\"\ndisplayName=\"JourneyMap\"\n[[dependencies.journeymap]]\nmodId=\"minecraft\"\nversionRange=\"[1.20.1,1.20.2)\"\n")}});
        writeTree(mc,{{"TlauncherProfiles.json","{}"},
            {"versions/EsquizosMineZ/EsquizosMineZ.json",encode(QJsonObject{{"id","EsquizosMineZ"},{"mainClass","cpw.mods.bootstraplauncher.BootstrapLauncher"},{"libraries",QJsonArray{QJsonObject{{"name","net.minecraftforge:fmlloader:1.20.1-47.2.0"}}}},{"arguments",QJsonObject{{"game",QJsonArray{"--fml.forgeVersion","47.2.0","--fml.mcVersion","1.20.1"}}}}})},
            {"versions/EsquizosMineZ/EsquizosMineZ.jar","client"},{"versions/EsquizosMineZ/mods/journeymap.jar",forgeJar},{"versions/EsquizosMineZ/saves/W/level.dat","level"},
            {"versions/1.21.1/1.21.1.json",encode(QJsonObject{{"id","1.21.1"},{"mainClass","x"},{"libraries",QJsonArray{}}})},
            {"versions/fabric-loader-0.16.7-1.21.1/fabric-loader-0.16.7-1.21.1.json",profile("fabric-loader-0.16.7-1.21.1","1.21.1",{"net.fabricmc:intermediary:1.21.1","net.fabricmc:fabric-loader:0.16.7"})},
            {"launcher_profiles.json",encode(QJsonObject{{"profiles",QJsonObject{{"f",QJsonObject{{"lastVersionId","fabric-loader-0.16.7-1.21.1"},{"lastUsed","2026-09-01T00:00:00.000Z"}}}}}})},
            {"mods/sodium.jar",zipBytes({{"fabric.mod.json",encode(QJsonObject{{"id","sodium"},{"depends",QJsonObject{{"minecraft","~1.21.1"}}}})}})}});
        QMap<QString,QJsonObject> found;for(const auto &v:PackService::localInstances()){auto o=v.toObject();found[o["path"].toString()]=o;}
        QVERIFY(found.contains(mc+"/versions/EsquizosMineZ"));QVERIFY(found.contains(mc));QVERIFY(!found.contains(mc+"/versions/1.21.1")); // no game folder of its own
        auto version=found[mc+"/versions/EsquizosMineZ"];QCOMPARE(version["source"].toString(),QString("TLauncher"));QCOMPARE(describe(version),QString("1.20.1 forge 47.2.0"));
        QCOMPARE(describe(found[mc]),QString("1.21.1 fabric 0.16.7"));
        // Importing the listed folder creates the instance with that version and loader.
        QTemporaryDir root;PackService service(root.path(),[](const QUrl &u)->QByteArray{throw std::runtime_error(u.toString().toStdString());});
        auto info=importPath(service,mc+"/versions/EsquizosMineZ");QCOMPARE(describe(info),QString("1.20.1 forge 47.2.0"));QVERIFY(QFile::exists(info["dir"].toString()+"/mods/journeymap.jar"));QVERIFY(!QFile::exists(info["dir"].toString()+"/EsquizosMineZ.jar"));
        info=importPath(service,mc);QCOMPARE(describe(info),QString("1.21.1 fabric 0.16.7"));QVERIFY(QFile::exists(info["dir"].toString()+"/mods/sodium.jar"));QVERIFY(!QFile::exists(info["dir"].toString()+"/versions"));
#else
        QSKIP("Uses the Linux location of .minecraft");
#endif
    }
    void pastedInstancesBecomeInstances(){
        QTemporaryDir root;PackService service(root.path(),[](const QUrl &u)->QByteArray{throw std::runtime_error(u.toString().toStdString());});McInstanceManager manager(root.path());
        const auto instances=manager.instancesRoot();
        const auto ebalia=manager.createInstance("Mine","1.21.1","vanilla");
        QDir().mkpath(instances+"/Broken");writeFile(instances+"/Broken/instance.json","{not json");
        writeTree(instances+"/Prism Pack",{{"instance.cfg","[General]\nname=Prism Survival\nOverrideMemory=true\nMaxMemAlloc=6144\n"},{"mmc-pack.json",mmcPack("1.20.1","net.fabricmc.fabric-loader","0.15.11")},{".minecraft/mods/a.jar","mod"},{".minecraft/saves/W/level.dat","level"}});
        writeTree(instances+"/EsquizosMineZ",{{"EsquizosMineZ.json",encode(QJsonObject{{"id","EsquizosMineZ"},{"mainClass","x"},{"libraries",QJsonArray{QJsonObject{{"name","net.minecraftforge:fmlloader:1.20.1-47.2.0"}}}},{"arguments",QJsonObject{{"game",QJsonArray{"--fml.forgeVersion","47.2.0","--fml.mcVersion","1.20.1"}}}}})},{"mods/b.jar","mod"}});
        zipEntries(instances+"/Pack.mrpack",{{"modrinth.index.json",mrIndex({{"minecraft","1.21.1"},{"neoforge","21.1.77"}})},{"overrides/config/c.toml","cfg"}});
        auto pasted=PackService::pastedEntries(instances);std::sort(pasted.begin(),pasted.end());
        QCOMPARE(pasted,QStringList({instances+"/EsquizosMineZ",instances+"/Pack.mrpack",instances+"/Prism Pack"}));
        QMap<QString,QJsonObject> made;for(const auto &p:pasted){auto dir=service.adopt(p);auto info=ModRepository::read(dir+"/instance.json");info["dir"]=dir;made[QFileInfo(dir).fileName()]=info;}
        QCOMPARE(made.keys(),QStringList({"EsquizosMineZ","Pack","Prism Survival"}));
        QCOMPARE(describe(made["Prism Survival"]),QString("1.20.1 fabric 0.15.11"));QCOMPARE(made["Prism Survival"]["xmx"].toInt(),6144);QCOMPARE(readFile(made["Prism Survival"]["dir"].toString()+"/saves/W/level.dat"),QByteArray("level"));
        QCOMPARE(describe(made["EsquizosMineZ"]),QString("1.20.1 forge 47.2.0"));QCOMPARE(describe(made["Pack"]),QString("1.21.1 neoforge 21.1.77"));
        QVERIFY(!QFile::exists(instances+"/Prism Pack"));QVERIFY(!QFile::exists(instances+"/Pack.mrpack"));QVERIFY(QDir(root.path()+"/mc/trash").entryList(QDir::Dirs|QDir::Files|QDir::NoDotAndDotDot).size()==3);
        QVERIFY(PackService::pastedEntries(instances).isEmpty());QVERIFY(QFile::exists(ebalia+"/instance.json"));QVERIFY(QFile::exists(instances+"/Broken/instance.json"));
        // Something that cannot become an instance stays where it is.
        QDir().mkpath(instances+"/Fotos");writeFile(instances+"/Fotos/a.png","png");
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,service.adopt(instances+"/Fotos"));QVERIFY(QFile::exists(instances+"/Fotos/a.png"));
    }
    void modVersionRanges(){
        ModCompat::Mod fabric;fabric.loader="fabric";
        auto fits=[&](QStringList ranges,QString mc,bool maven=false){fabric.ranges=ranges;fabric.maven=maven;return ModCompat::accepts(fabric,mc);};
        QVERIFY(fits({"~1.20.1"},"1.20.4"));QVERIFY(!fits({"~1.20.1"},"1.21"));QVERIFY(!fits({"~1.20.1"},"1.20"));
        QVERIFY(fits({">=1.20 <1.21"},"1.20.6"));QVERIFY(!fits({">=1.20 <1.21"},"1.21.1"));QVERIFY(fits({"1.20.x"},"1.20.2"));QVERIFY(fits({"*"},"26.3"));
        QVERIFY(fits({"1.20.1","1.20.2"},"1.20.2"));QVERIFY(!fits({"1.20.1"},"1.20.2"));QVERIFY(fits({">=1.21.2-alpha.24.38.a"},"1.21.4"));QVERIFY(fits({"^1.20"},"1.21"));
        QVERIFY(fits({"[1.20.1,1.21)"},"1.20.6",true));QVERIFY(!fits({"[1.20.1,1.21)"},"1.21",true));QVERIFY(fits({"[1.20.1]"},"1.20.1",true));QVERIFY(!fits({"[1.20.1]"},"1.20.2",true));
        QVERIFY(fits({"[1.18,1.19),[1.20,1.21)"},"1.20.1",true));QVERIFY(!fits({"[1.18,1.19),[1.20,1.21)"},"1.19.2",true));QVERIFY(fits({"[26.3,)"},"26.3",true));QVERIFY(fits({"1.20.1"},"1.19",true)); // bare: a preference
        QList<ModCompat::Mod> mods;ModCompat::Mod a;a.name="A";a.loader="fabric";a.ranges={"~1.20.1"};ModCompat::Mod b;b.name="B";b.loader="fabric";b.ranges={"1.21.1"};mods<<a<<b;
        auto r=ModCompat::detect(mods,{"1.21.1","1.20.4","1.20.1"});QVERIFY(!r.compatible());QCOMPARE(r.problems.size(),1);
    }
    void graphicsDriverLinks(){
        using GpuInfo::describe;
        QCOMPARE(describe("NVIDIA GeForce RTX 5060").vendor,QString("NVIDIA"));QVERIFY(!describe("NVIDIA GeForce RTX 5060").integrated);
        QVERIFY(describe("Intel(R) UHD Graphics 620").integrated);QVERIFY(describe("Intel(R) Arc(TM) Graphics").integrated);QVERIFY(!describe("Intel(R) Arc(TM) B580 Graphics").integrated);
        QVERIFY(describe("AMD Radeon(TM) Graphics").integrated);QVERIFY(describe("AMD Radeon 780M Graphics").integrated);QVERIFY(!describe("AMD Radeon RX 7600").integrated);
        QVERIFY(describe("Microsoft Basic Display Adapter").virtualAdapter);QVERIFY(describe("VMware SVGA 3D").virtualAdapter);
        // A laptop: the crash in the NVIDIA driver points at the NVIDIA card, otherwise the dedicated one is chosen.
        const QList<GpuInfo::Gpu> laptop{describe("Intel(R) UHD Graphics 630"),describe("NVIDIA GeForce RTX 4060 Laptop GPU")};
        QCOMPARE(GpuInfo::pick(laptop,"Intel").name,QString("Intel(R) UHD Graphics 630"));QCOMPARE(GpuInfo::pick(laptop).name,QString("NVIDIA GeForce RTX 4060 Laptop GPU"));
        // NVIDIA: product id from NVIDIA's list, then the newest driver's page (responses trimmed from the real services).
        QMap<QString,QByteArray> web{{"https://www.nvidia.com/Download/API/lookupValueSearch.aspx?TypeID=3",
            "<LookupValueSearch><LookupValues><LookupValue ParentID=\"131\">\n<Name>NVIDIA GeForce RTX 5060 Ti</Name>\n<Value>1076</Value>\n</LookupValue><LookupValue ParentID=\"131\">\n<Name>NVIDIA GeForce RTX 5060</Name>\n<Value>1078</Value>\n</LookupValue><LookupValue ParentID=\"129\">\n<Name>GeForce RTX 4060 Laptop GPU</Name>\n<Value>1007</Value>\n</LookupValue></LookupValues></LookupValueSearch>"}};
        QStringList asked;auto fetch=[&](const QUrl &u)->QByteArray{asked<<u.toString();const auto q=QUrlQuery(u);
            if(u.host()=="gfwsl.geforce.com"){const auto id=q.queryItemValue("pfid");return encode(QJsonObject{{"IDS",QJsonArray{QJsonObject{{"downloadInfo",QJsonObject{{"Version",id=="1078"?"617.42":"612.10"},{"DetailsURL","https://www.nvidia.com/en-us/drivers/details/"+id+"/"}}}}}}});}
            if(web.contains(u.toString()))return web[u.toString()];throw std::runtime_error("404");};
        auto link=GpuInfo::driverLink(describe("NVIDIA GeForce RTX 5060"),fetch,"windows");QVERIFY(link.exact);QCOMPARE(link.url,QString("https://www.nvidia.com/en-us/drivers/details/1078/"));QVERIFY(link.title.contains("617.42"));
        QVERIFY(asked.last().contains("psid=131")&&asked.last().contains("pfid=1078"));
        QCOMPARE(GpuInfo::driverLink(describe("NVIDIA GeForce RTX 4060 Laptop GPU"),fetch,"windows").url,QString("https://www.nvidia.com/en-us/drivers/details/1007/"));
        link=GpuInfo::driverLink(describe("NVIDIA GeForce RTX 9999"),fetch,"windows");QVERIFY(!link.exact);QCOMPARE(link.url,GpuInfo::autoDetect("NVIDIA").url); // unknown model: NVIDIA App
        link=GpuInfo::driverLink(describe("NVIDIA GeForce RTX 5060"),[](const QUrl &)->QByteArray{throw std::runtime_error("offline");},"windows");QCOMPARE(link.url,GpuInfo::autoDetect("NVIDIA").url);
        // AMD: the model's page when it exists, AMD's detection tool otherwise and for Radeon inside Ryzen processors.
        const QString rx7600="https://www.amd.com/en/support/downloads/drivers.html/graphics/radeon-rx/radeon-rx-7000-series/amd-radeon-rx-7600.html";web[rx7600]="<html>";
        web["https://www.amd.com/en/support/downloads/drivers.html/graphics/radeon-rx/radeon-rx-9000-series/amd-radeon-rx-9070-xt.html"]="<html>";
        QCOMPARE(GpuInfo::driverLink(describe("AMD Radeon RX 7600"),fetch,"windows").url,rx7600);
        QVERIFY(GpuInfo::driverLink(describe("AMD Radeon(TM) RX 9070 XT"),fetch,"windows").exact);
        QCOMPARE(GpuInfo::driverLink(describe("AMD Radeon RX 7650 GRE"),fetch,"windows").url,GpuInfo::autoDetect("AMD").url);
        QCOMPARE(GpuInfo::driverLink(describe("AMD Radeon 780M Graphics"),fetch,"windows").url,GpuInfo::autoDetect("AMD").url);
        // Intel: Arc cards, 7th–10th generation graphics, Intel's assistant for the rest.
        QVERIFY(GpuInfo::driverLink(describe("Intel(R) Arc(TM) A770 Graphics"),fetch,"windows").url.contains("785597"));
        QVERIFY(GpuInfo::driverLink(describe("Intel(R) UHD Graphics 620"),fetch,"windows").url.contains("776137"));
        QCOMPARE(GpuInfo::driverLink(describe("Intel(R) Iris(R) Xe Graphics"),fetch,"windows").url,GpuInfo::autoDetect("Intel").url);
        // Linux: NVIDIA's Linux driver; AMD and Intel come with the system (no page); macOS updates itself.
        QVERIFY(GpuInfo::driverLink(describe("NVIDIA GeForce RTX 5060"),fetch,"linux").url.contains("/drivers/unix/"));QVERIFY(GpuInfo::driverLink(describe("AMD Radeon RX 7600"),fetch,"linux").url.isEmpty());
        QVERIFY(GpuInfo::manufacturerSupport({"HP","HP Pavilion Laptop 15"}).url.contains("support.hp.com"));QVERIFY(GpuInfo::manufacturerSupport({"LENOVO","82XV"}).url.contains("lenovo"));
        QVERIFY(GpuInfo::manufacturerSupport({"Micro-Star International Co., Ltd.","MS-7C91"}).url.contains("msi.com"));
        if(qEnvironmentVariableIsSet("EBALIA_LIVE_TESTS")){auto live=[](const QUrl &u){return ModRepository::fetch(u);};
            for(auto name:{"NVIDIA GeForce RTX 5060","NVIDIA GeForce RTX 3060","AMD Radeon RX 7600","AMD Radeon RX 6700 XT"}){auto l=GpuInfo::driverLink(describe(name),live,"windows");qInfo().noquote()<<name<<"->"<<l.url<<l.title;QVERIFY2(l.exact,name);}}QVERIFY(GpuInfo::manufacturerSupport({"System manufacturer","System Product Name"}).url.isEmpty());
    }
    void crashSummaries(){
        // Lines from real crash logs (Forge 26.3 with Biomes O' Plenty, an AMD driver crash, a virtual machine, Fabric).
        auto f=CrashReport::analyze("[log4j] Missing or unsupported mandatory dependencies:\n\tMod ID: 'terrablender', Requested by: 'biomesoplenty', Expected range: '[26.3.0.0.6,)', Actual version: '[MISSING]'\n\tMod ID: 'minecraft', Requested by: 'x', Expected range: '[1.0]', Actual version: '26.3'");
        QCOMPARE(f.size(),1);QCOMPARE(f[0].kind,CrashReport::Finding::MissingMod);QCOMPARE(f[0].mod,QString("terrablender"));QCOMPARE(f[0].requiredBy,QString("biomesoplenty"));QCOMPARE(f[0].versions,QString("[26.3.0.0.6,)"));
        f=CrashReport::analyze("# Problematic frame:\n# C  [atio6axx.dll+0x192b60]\n");QCOMPARE(f.size(),1);QCOMPARE(f[0].kind,CrashReport::Finding::GraphicsDriver);QCOMPARE(f[0].vendor,QString("AMD"));
        f=CrashReport::analyze("# C  [nvoglv64.dll+0x1]");QVERIFY(f.isEmpty()); // without "Problematic frame" it is not the crash site
        f=CrashReport::analyze("# Problematic frame:\n# C  [nvoglv64.dll+0xabc]");QCOMPARE(f[0].vendor,QString("NVIDIA"));
        f=CrashReport::analyze("FATAL ERROR in native method: [LWJGL] Thread[#3,Render thread,5,main]: No context is current or a function that is not available in the current context was called.");QCOMPARE(f[0].kind,CrashReport::Finding::NoOpenGL);
        f=CrashReport::analyze("\t - Mod 'Sodium Extra' (sodium-extra) 0.5.1 requires any version of fabric-api, which is missing!\n\t - Mod 'Iris' (iris) 1.7 requires version 0.5.0 or later of mod 'Sodium' (sodium), which is missing!");
        QCOMPARE(f.size(),2);QCOMPARE(f[0].mod,QString("fabric-api"));QCOMPARE(f[0].requiredBy,QString("sodium-extra"));QCOMPARE(f[1].mod,QString("sodium"));QCOMPARE(f[1].versions,QString("0.5.0 or later"));
        f=CrashReport::analyze("java.lang.UnsupportedClassVersionError: x has been compiled by a more recent version of the Java Runtime (class file version 65.0), this version of the Java Runtime only recognizes class file versions up to 61.0");QCOMPARE(f[0].kind,CrashReport::Finding::JavaVersion);QCOMPARE(f[0].versions,QString("21"));
        QVERIFY(CrashReport::analyze("Caused by: java.lang.IllegalStateException: Failed to find system mod: forge").isEmpty());
    }
    void importRejectsUnsafePaths(){
        QTemporaryDir root;PackService service(root.path(),[](const QUrl &){return QByteArray("evil");});
        auto mr=[&](QString path){return mrIndex({{"minecraft","1.20.1"},{"fabric-loader","0.15.11"}},{QJsonObject{{"path",path},{"hashes",QJsonObject{{"sha512",QString::fromLatin1(QCryptographicHash::hash("evil",QCryptographicHash::Sha512).toHex())}}},{"downloads",QJsonArray{"https://cdn.modrinth.com/evil.jar"}}}});};
        QList<QPair<QString,Files>> evil{
            {"index ../",{{"modrinth.index.json",mr("../../escape-1.jar")}}},
            {"index backslash ..",{{"modrinth.index.json",mr("mods\\..\\..\\escape-2.jar")}}},
            {"index absolute",{{"modrinth.index.json",mr(root.path()+"/escape-3.jar")}}},
            {"index drive letter",{{"modrinth.index.json",mr("C:/escape-4.jar")}}},
            {"index UNC",{{"modrinth.index.json",mr("\\\\server\\share\\escape-5.jar")}}},
            {"entry ../",{{"modrinth.index.json",mr("mods/ok.jar")},{"overrides/../../escape-6.txt","x"}}},
            {"entry backslash ..",{{"modrinth.index.json",mr("mods/ok.jar")},{"overrides\\..\\..\\escape-7.txt","x"}}},
            {"entry absolute",{{"modrinth.index.json",mr("mods/ok.jar")},{root.path()+"/escape-8.txt","x"}}},
            {"entry symlink",{{"modrinth.index.json",mr("mods/ok.jar")},{"overrides/mods","->"+root.path().toUtf8()},{"overrides/mods/escape-9.txt","x"}}},
            {"curseforge overrides outside",{{"manifest.json",encode(QJsonObject{{"minecraft",QJsonObject{{"version","1.20.1"},{"modLoaders",QJsonArray{}}}},{"files",QJsonArray{}},{"overrides","../.."}})}}},
        };
        int i=0;QStringList failures;
        for(const auto &[label,files]:evil){auto path=root.path()+"/evil-"+QString::number(++i)+".zip";zipEntries(path,files);auto info=importPath(service,path);if(!info.contains("error"))failures<<label+": accepted";}
        QDirIterator it(root.path(),QDir::Files|QDir::Hidden,QDirIterator::Subdirectories);while(it.hasNext()){auto f=it.next();if(it.fileName().startsWith("escape-"))failures<<"written outside: "+f;}
        QCOMPARE(QDir(root.path()+"/mc/instances").entryList(QDir::Dirs|QDir::NoDotAndDotDot).size(),0);
        QVERIFY2(failures.isEmpty(),qPrintable(failures.join("; ")));
    }
    void importRealPacks(){
        // Opt-in: EBALIA_IMPORT_FIXTURES=<folder with real .mrpack/.zip packs>; downloads every file from Modrinth/CurseForge.
        const auto fixtures=qEnvironmentVariable("EBALIA_IMPORT_FIXTURES");if(fixtures.isEmpty())QSKIP("Set EBALIA_IMPORT_FIXTURES to a folder of real packs");
        const QMap<QString,QString> expected{{"modrinth-fabric.mrpack","1.21.1 fabric 0.16.7"},{"modrinth-quilt.mrpack","1.20.1 quilt 0.29.1"},{"modrinth-neoforge.mrpack","1.21.1 neoforge 21.1.230"},{"modrinth-forge.mrpack","1.20.1 forge 47.1.44"},
            {"curseforge-forge.zip","1.20.1 forge 47.4.10"},{"curseforge-fabric.zip","1.20.1 fabric 0.19.5"},{"curseforge-quilt.zip","1.20.1 quilt 0.22.0"},{"curseforge-neoforge.zip","1.20.1 neoforge 47.1.106"},
            {"technic-forge-1.7.10.zip","1.7.10 forge 10.13.4.1614-1.7.10"},{"technic-neoforge-21.1.77.zip","1.21.1 neoforge 21.1.77"},{"technic-forge-1.20.1.zip","1.20.1 forge 47.2.0"},{"technic-neoforge-1.20.1.zip","1.20.1 neoforge 47.1.106"}};
        QTemporaryDir root(fixtures+"/run-XXXXXX");PackService service(root.path());QStringList failures;
        for(const auto &file:QDir(fixtures).entryList({"*.mrpack","*.zip"},QDir::Files,QDir::Name)){
            QElapsedTimer timer;timer.start();auto info=importPath(service,fixtures+"/"+file);auto got=describe(info);auto dir=info["dir"].toString();
            auto count=[&](QString sub){return QDir(dir+"/"+sub).entryList(QDir::Files).size();};
            qInfo().noquote()<<file<<"->"<<got<<"| mods"<<count("mods")<<"resourcepacks"<<count("resourcepacks")<<"shaderpacks"<<count("shaderpacks")<<"|"<<timer.elapsed()/1000<<"s";
            if(expected.contains(file)&&got!=expected[file])failures<<file+": expected \""+expected[file]+"\", got \""+got+"\"";
        }
        QVERIFY2(failures.isEmpty(),qPrintable(failures.join("; ")));
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
