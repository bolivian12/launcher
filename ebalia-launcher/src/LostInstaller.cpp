#include "LostInstaller.hpp"
#include "Download.hpp"
#include "Archive.hpp"
#include "ModRepository.hpp"
#include "JavaRuntime.hpp"
#include "JavaDownloader.hpp"
#include <QtCore>
#include <stdexcept>
void LostInstaller::install(const VersionInfo &v,const QString &destination,std::function<void(qint64,qint64)> progress){
    auto fail=[](QString s){throw std::runtime_error(s.toStdString());};auto root=QFileInfo(QFileInfo(destination).absolutePath()).absolutePath();auto cache=root+"/cache/lost";QDir().mkpath(cache);QDir().mkpath(QFileInfo(destination).absolutePath());QLockFile lock(cache+"/install.lock");if(!lock.tryLock())fail("Another lost-version installation is in progress.");
    auto name=QUrl(v.downloadUrl).fileName();if(!ModRepository::safeName(name))fail("Invalid archive name");auto archive=cache+"/"+name;
    if(!QFile::exists(archive)||QFileInfo(archive).size()!=v.archiveSize||v.archiveSize<=0)Download::file(QUrl(v.downloadUrl),archive,v.archiveSha256.toLatin1(),QCryptographicHash::Sha256,progress);
    QTemporaryDir stage(QFileInfo(destination).absolutePath()+"/.lost-XXXXXX");if(!stage.isValid())fail("Could not create staging directory");auto content=stage.path()+"/content";try{Archive::extract(archive,content,v.archivePrefix);}catch(...){QFile::remove(archive);throw;}
    QStringList matches;QDirIterator files(content,QDir::Files|QDir::Hidden,QDirIterator::Subdirectories);while(files.hasNext()){files.next();if(files.fileName().compare(QFileInfo(v.launchCommand).fileName(),Qt::CaseInsensitive)==0)matches<<files.filePath();}if(matches.size()!=1)fail("The archive launch file is missing or ambiguous.");
    ModRepository::write(content+"/.installed.json",{{"id",v.id},{"source",v.downloadUrl},{"launchFile",QDir(content).relativeFilePath(matches.first())}});
    QString backup;if(QFile::exists(destination)){backup=destination+".backup-"+QString::number(QDateTime::currentMSecsSinceEpoch());if(!QDir().rename(destination,backup))fail("Could not preserve previous installation");}
    if(!QDir().rename(content,destination)){if(!backup.isEmpty())QDir().rename(backup,destination);fail("Could not install archive");}
}
bool LostInstaller::windowsPackage(const VersionInfo &v){auto suffix=QFileInfo(v.launchCommand).suffix().toLower();return suffix=="exe"||suffix=="bat"||suffix=="cmd";}
QString LostInstaller::winePrefix(const QString &dataDir){auto prefix=qEnvironmentVariable("EBALIA_WINEPREFIX");return prefix.isEmpty()?dataDir+"/wine":prefix;}
QString LostInstaller::java(const QString &dataDir,bool windows,std::function<void(int,int)> progress){
    const auto root=dataDir+"/mc/java";
#ifdef Q_OS_WIN
    Q_UNUSED(windows)
    auto found=JavaRuntime::select(8);if(!found.isEmpty())return found;
    return JavaDownloader::install(root,"jre-legacy",{},progress);
#else
    // Inside Wine only a Windows Java works; the Linux or macOS Java of this computer cannot replace it.
    if(windows){auto existing=JavaDownloader::installed(root,"jre-legacy","windows-x64");return existing.isEmpty()?JavaDownloader::install(root,"jre-legacy","windows-x64",progress):existing;}
    auto found=JavaRuntime::select(8);if(!found.isEmpty()||QFile::exists("/etc/NIXOS"))return found;
    return JavaDownloader::install(root,"jre-legacy",{},progress);
#endif
}
QString LostInstaller::wine(){for(auto name:{"wine","wine64"}){auto path=QStandardPaths::findExecutable(name);if(!path.isEmpty())return path;}for(auto path:{"/usr/lib/wine/wine64","/opt/wine-stable/bin/wine","/Applications/Wine Stable.app/Contents/Resources/wine/bin/wine"})if(QFileInfo(path).isExecutable())return path;return {};}
