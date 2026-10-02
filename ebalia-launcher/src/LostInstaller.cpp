#include "LostInstaller.hpp"
#include "Download.hpp"
#include "Archive.hpp"
#include "ModRepository.hpp"
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
