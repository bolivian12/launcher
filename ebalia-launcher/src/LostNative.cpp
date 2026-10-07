#include "LostNative.hpp"
#include "Download.hpp"
#include "Archive.hpp"
#include <QtCore>
#include <stdexcept>
namespace {
[[noreturn]] void fail(const QString &message){throw std::runtime_error(message.toStdString());}
struct Artifact {const char *path,*sha1;};
// From Mojang's Minecraft 1.12.2 version JSON: macOS uses the 2.9.2 build, every other system 2.9.4.
const Artifact commonJars[]{
    {"net/java/jinput/jinput/2.0.5/jinput-2.0.5.jar","39c7796b469a600f72380316f6b1f11db6c2c7c4"},
    {"net/java/jutils/jutils/1.0.0/jutils-1.0.0.jar","e12fe1fda814bd348c1579329c86943d2cd3c6a6"},
};
const Artifact lwjglJars[]{
    {"org/lwjgl/lwjgl/lwjgl/2.9.4-nightly-20150209/lwjgl-2.9.4-nightly-20150209.jar","697517568c68e78ae0b4544145af031c81082dfe"},
    {"org/lwjgl/lwjgl/lwjgl_util/2.9.4-nightly-20150209/lwjgl_util-2.9.4-nightly-20150209.jar","d51a7c040a721d13efdfbd34f8b257b2df882ad0"},
};
const Artifact lwjglJarsMac[]{
    {"org/lwjgl/lwjgl/lwjgl/2.9.2-nightly-20140822/lwjgl-2.9.2-nightly-20140822.jar","7707204c9ffa5d91662de95f0a224e2f721b22af"},
    {"org/lwjgl/lwjgl/lwjgl_util/2.9.2-nightly-20140822/lwjgl_util-2.9.2-nightly-20140822.jar","f0e612c840a7639c1f77f68d72a28dae2f0c8490"},
};
const Artifact nativesWindows[]{
    {"org/lwjgl/lwjgl/lwjgl-platform/2.9.4-nightly-20150209/lwjgl-platform-2.9.4-nightly-20150209-natives-windows.jar","b84d5102b9dbfabfeb5e43c7e2828d98a7fc80e0"},
    {"net/java/jinput/jinput-platform/2.0.5/jinput-platform-2.0.5-natives-windows.jar","385ee093e01f587f30ee1c8a2ee7d408fd732e16"},
};
const Artifact nativesLinux[]{
    {"org/lwjgl/lwjgl/lwjgl-platform/2.9.4-nightly-20150209/lwjgl-platform-2.9.4-nightly-20150209-natives-linux.jar","931074f46c795d2f7b30ed6395df5715cfd7675b"},
    {"net/java/jinput/jinput-platform/2.0.5/jinput-platform-2.0.5-natives-linux.jar","7ff832a6eb9ab6a767f1ade2b548092d0fa64795"},
};
const Artifact nativesMac[]{
    {"org/lwjgl/lwjgl/lwjgl-platform/2.9.2-nightly-20140822/lwjgl-platform-2.9.2-nightly-20140822-natives-osx.jar","79f5ce2fea02e77fe47a3c745219167a542121d7"},
    {"net/java/jinput/jinput-platform/2.0.5/jinput-platform-2.0.5-natives-osx.jar","53f9c919f34d2ca9de8c51fc4e1e8282029a9232"},
};
// Mojang's launchwrapper, as listed by the official a1.1.2_01 and b1.7.3 version JSONs: AlphaVanillaTweaker
// starts an applet-era game and points it at its game folder.
const Artifact launchwrapperJars[]{
    {"net/minecraft/launchwrapper/1.5/launchwrapper-1.5.jar","5150b9c2951f0fde987ce9c33496e26add1de224"},
    {"net/sf/jopt-simple/jopt-simple/4.5/jopt-simple-4.5.jar","6065cc95c661255349c1d0756657be17c29a4fd3"},
    {"org/ow2/asm/asm-all/4.1/asm-all-4.1.jar","054986e962b88d8660ae4566475658469595ef58"},
};
QList<Artifact> launchwrapper(){QList<Artifact> out;for(const auto &a:launchwrapperJars)out<<a;return out;}
QList<Artifact> lwjgl2Jars(){
    QList<Artifact> out;
    if(LostNative::system()=="osx")for(const auto &a:lwjglJarsMac)out<<a;else for(const auto &a:lwjglJars)out<<a;
    for(const auto &a:commonJars)out<<a;
    return out;
}
QList<Artifact> lwjgl2Natives(){
    QList<Artifact> out;const auto os=LostNative::system();
    if(os=="windows")for(const auto &a:nativesWindows)out<<a;else if(os=="osx")for(const auto &a:nativesMac)out<<a;else for(const auto &a:nativesLinux)out<<a;
    return out;
}
QStringList strings(const QJsonValue &value){QStringList out;for(const auto &v:value.toArray())out<<v.toString();return out;}
// Relative paths from the catalog never leave the package.
QString inside(const QString &root,const QString &relative){
    const auto clean=QDir::cleanPath(relative);
    if(relative.isEmpty()||clean=="."||clean.isEmpty())return QDir::cleanPath(root);
    if(QDir::isAbsolutePath(relative)||relative.contains('\\')||clean.startsWith("..")||clean.split('/').contains(".."))fail("Invalid path in the lost version description: "+relative);
    return QDir::cleanPath(root+"/"+clean);
}
QString nativesDir(const QString &installDir){return installDir+"/.ebalia/natives-"+LostNative::system();}
}
namespace LostNative {
bool available(const QJsonObject &spec){return !spec["mainClass"].toString().isEmpty();}
QString system(){
#if defined(Q_OS_WIN)
    return "windows";
#elif defined(Q_OS_MACOS)
    return "osx";
#else
    return "linux";
#endif
}
void prepare(const QJsonObject &spec,const QString &installDir,const QString &librariesDir,std::function<void(int,int)> progress){
    if(!available(spec))fail("This lost version has no launch description.");
    QList<Artifact> files;const auto entries=strings(spec["classpath"]);
    if(entries.contains("@lwjgl2"))files<<lwjgl2Jars();
    if(entries.contains("@launchwrapper"))files<<launchwrapper();
    const bool natives=spec["natives"].toString()=="lwjgl2";
    if(natives)files<<lwjgl2Natives();
    int done=0;
    for(const auto &file:files){
        if(progress)progress(done++,files.size());
        Download::file(QUrl(QString("https://libraries.minecraft.net/")+file.path),librariesDir+"/"+file.path,QByteArray(file.sha1),QCryptographicHash::Sha1);
    }
    if(natives){
        // Extract into a fresh folder so a moved or copied installation never keeps another system's files.
        const auto target=nativesDir(installDir);const auto marker=target+"/.complete";
        if(!QFile::exists(marker)){
            QDir(target).removeRecursively();
            for(const auto &file:lwjgl2Natives())Archive::extract(librariesDir+"/"+file.path,target);
            QDir(target+"/META-INF").removeRecursively();
            QFile f(marker);if(!f.open(QIODevice::WriteOnly))fail("Could not prepare the native libraries.");
        }
    }
    // The game cannot start in a folder that does not exist, and its private home must exist too.
    QDir().mkpath(inside(installDir,spec["workingDir"].toString()));QDir().mkpath(installDir+"/.ebalia/home");
    if(progress)progress(files.size(),files.size());
}
Command command(const QJsonObject &spec,const QString &installDir,const QString &librariesDir,const QString &username){
    if(!available(spec))fail("This lost version has no launch description.");
    Command out;out.workingDir=inside(installDir,spec["workingDir"].toString());
    // Old clients keep worlds and options in ~/.minecraft (%APPDATA%\.minecraft on Windows). A home of their own keeps
    // them inside the version, away from the player's regular Minecraft, and moves with a portable installation.
    const auto home=QDir::toNativeSeparators(installDir+"/.ebalia/home");
    const auto natives=nativesDir(installDir);
    const auto name=username.trimmed().isEmpty()?QString("Player"):username.trimmed();
    auto expand=[&](QString value){
        value.replace("${username}",name).replace("${installDir}",QDir::toNativeSeparators(installDir))
             .replace("${workingDir}",QDir::toNativeSeparators(out.workingDir)).replace("${natives}",QDir::toNativeSeparators(natives));
        return value;
    };
    QStringList classpath;
    for(const auto &entry:strings(spec["classpath"])){
        if(entry=="@lwjgl2"){for(const auto &a:lwjgl2Jars())classpath<<QDir::toNativeSeparators(librariesDir+"/"+a.path);continue;}
        if(entry=="@launchwrapper"){for(const auto &a:launchwrapper())classpath<<QDir::toNativeSeparators(librariesDir+"/"+a.path);continue;}
        const auto path=inside(installDir,entry);
        if(!QFileInfo(path).exists())fail("A file of this lost version is missing: "+entry+". Reinstall it.");
        classpath<<QDir::toNativeSeparators(path);
    }
    out.arguments<<"-Duser.home="+home;out.environment.insert("APPDATA",home);
    for(const auto &arg:strings(spec["jvmArgs"]))out.arguments<<expand(arg);
    if(spec["natives"].toString()=="lwjgl2")
        out.arguments<<"-Djava.library.path="+QDir::toNativeSeparators(natives)<<"-Dorg.lwjgl.librarypath="+QDir::toNativeSeparators(natives)<<"-Dnet.java.games.input.librarypath="+QDir::toNativeSeparators(natives);
    if(!classpath.isEmpty())out.arguments<<"-cp"<<classpath.join(QDir::listSeparator());
    out.arguments<<spec["mainClass"].toString();
    for(const auto &arg:strings(spec["gameArgs"]))out.arguments<<expand(arg);
    // Launchers such as MultiMC's NewLaunch read their instructions from standard input.
    for(const auto &line:strings(spec["stdin"]))out.input+=expand(line).toUtf8()+"\n";
    return out;
}
}
