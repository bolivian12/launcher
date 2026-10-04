#include "BedrockInstaller.hpp"
#include "Download.hpp"
#include "Archive.hpp"
#include <QtCore>
#include <stdexcept>
namespace {
void run(const QString &exe,const QStringList &args) {
    QProcess p;p.start(exe,args);
    if(!p.waitForStarted(10000))throw std::runtime_error("No se pudo iniciar: "+exe.toStdString());
    if(!p.waitForFinished(1800000)){p.kill();p.waitForFinished();throw std::runtime_error("La instalación superó el tiempo máximo.");}
    if(p.exitStatus()!=QProcess::NormalExit||p.exitCode()!=0)
        throw std::runtime_error(QString("Falló %1: %2").arg(exe,QString::fromUtf8(p.readAllStandardError()).right(1600)).toStdString());
}
}
QString BedrockInstaller::install(const QString &root) {
    QDir().mkpath(root);
    QLockFile lock(root+"/install.lock");
    if(!lock.tryLock())throw std::runtime_error("Ya hay una instalación de Bedrock en curso.");
#if defined(Q_OS_LINUX)
    auto flatpak=QStandardPaths::findExecutable("flatpak");
    if(flatpak.isEmpty())throw std::runtime_error("Falta Flatpak. En NixOS habilitá services.flatpak.enable = true y reconstruí el sistema. Luego volvé a pulsar Instalar.");
    run(flatpak,{"remote-add","--user","--if-not-exists","flathub","https://flathub.org/repo/flathub.flatpakrepo"});
    run(flatpak,{"install","--user","--noninteractive","--assumeyes","flathub","io.mrarm.mcpelauncher"});
    return "flatpak";
#elif defined(Q_OS_WIN) || defined(Q_OS_MACOS)
#ifdef Q_OS_WIN
    const QString repo="BedrockLauncher/BedrockLauncher",suffix=".zip";
#else
    const QString repo="minecraft-linux/macos-builder",suffix=".dmg";
#endif
    QTemporaryDir stage(root+"/download-XXXXXX");
    if(!stage.isValid())throw std::runtime_error("No se pudo crear la carpeta temporal.");
    Download::file(QUrl("https://api.github.com/repos/"+repo+"/releases/latest"),stage.path()+"/release.json");
    QFile metadata(stage.path()+"/release.json");metadata.open(QIODevice::ReadOnly);
    auto release=QJsonDocument::fromJson(metadata.readAll()).object();
    if(release.isEmpty()||release["draft"].toBool()||release["prerelease"].toBool())throw std::runtime_error("No se encontró una publicación estable.");
    QJsonObject asset;
    for(auto v:release["assets"].toArray())if(v.toObject()["name"].toString().endsWith(suffix,Qt::CaseInsensitive)){if(!asset.isEmpty())throw std::runtime_error("Hay varios paquetes: selección automática no disponible.");asset=v.toObject();}
    auto url=QUrl(asset["browser_download_url"].toString());
    if(url.scheme()!="https"||url.host()!="github.com"||!url.path().startsWith("/"+repo+"/releases/download/"))throw std::runtime_error("Origen del paquete inesperado.");
    const auto file=stage.path()+"/package"+suffix;
    auto digest=asset["digest"].toString();
    QByteArray hash;
    if(digest.startsWith("sha256:"))hash=digest.mid(7).toLatin1();
    Download::file(url,file,hash);
    const auto target=root+"/provider-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
#ifdef Q_OS_WIN
    Archive::extract(file,target);
    QDirIterator it(target,{"BedrockLauncher.exe"},QDir::Files,QDirIterator::Subdirectories);
    if(!it.hasNext())throw std::runtime_error("El paquete no contiene BedrockLauncher.exe.");
    auto executable=it.next();
    // Upstream requires the desktop runtime, separate from the game license.
    auto dotnet=QStandardPaths::findExecutable("dotnet");
    if(dotnet.isEmpty())dotnet=qEnvironmentVariable("ProgramFiles")+"/dotnet/dotnet.exe";
    QProcess runtimes;runtimes.start(dotnet,{"--list-runtimes"});
    bool runtimeReady=runtimes.waitForStarted(5000)&&runtimes.waitForFinished(10000)
        &&runtimes.readAllStandardOutput().contains("Microsoft.WindowsDesktop.App 8.");
    if(!runtimeReady) {
        auto winget=QStandardPaths::findExecutable("winget");
        if(winget.isEmpty())throw std::runtime_error("Falta .NET Desktop Runtime 8 y no se encontró winget para instalarlo. Instalá App Installer y volvé a intentar.");
        run(winget,{"install","--id","Microsoft.DotNet.DesktopRuntime.8","--exact","--accept-source-agreements","--accept-package-agreements","--disable-interactivity"});
    }
    return executable;
#else
    const auto mount=stage.path()+"/mount";
    QDir().mkpath(mount);
    run("/usr/bin/hdiutil",{"attach","-nobrowse","-readonly","-mountpoint",mount,file});
    QString app;
    try {
        auto entries=QDir(mount).entryList({"*.app"},QDir::Dirs);
        if(entries.size()!=1)throw std::runtime_error("No se encontró una aplicación única en el DMG.");
        QDir().mkpath(target);app=target+"/"+entries.front();
        run("/usr/bin/ditto",{mount+"/"+entries.front(),app});
    } catch(...) { try {run("/usr/bin/hdiutil",{"detach",mount});} catch(...) {} throw; }
    run("/usr/bin/hdiutil",{"detach",mount});
    return app;
#endif
#else
    throw std::runtime_error("Sistema no compatible con la instalación automática.");
#endif
}
