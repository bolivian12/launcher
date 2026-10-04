#include "LauncherUpdater.hpp"
#include "Download.hpp"
#include "Archive.hpp"
#include "UpdateChecker.hpp"
#include <QtCore>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <signal.h>
#include <errno.h>
#endif
namespace {
void fail(const QString &message) { throw std::runtime_error(message.toStdString()); }
QByteArray run(const QString &program,const QStringList &args,int timeout,const QProcessEnvironment &env=QProcessEnvironment::systemEnvironment()) {
 QProcess process;process.setProcessEnvironment(env);process.start(program,args);
 if(!process.waitForStarted(10000))fail("Could not start "+program);
 if(!process.waitForFinished(timeout)){process.kill();process.waitForFinished();fail("Update preparation timed out.");}
 if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)fail("Update preparation failed: "+QString::fromUtf8(process.readAllStandardError()).right(1200));
 return process.readAllStandardOutput();
}
QProcessEnvironment environment(const QString &root,bool probe=false) {
 auto env=QProcessEnvironment::systemEnvironment();env.insert("EBALIA_DATA_DIR",root);
 // Reuse the new package's libraries, not the old Qt deployment.
 for(const auto &key:QStringList{"QT_PLUGIN_PATH","QT_QPA_PLATFORM_PLUGIN_PATH","QML2_IMPORT_PATH","DYLD_LIBRARY_PATH","DYLD_FRAMEWORK_PATH"})env.remove(key);
 if(probe){env.insert("QT_QPA_PLATFORM","offscreen");env.insert("EBALIA_NO_NETWORK","1");}
 return env;
}
bool writeJson(const QString &file,const QJsonObject &value) {
 QSaveFile out(file);if(!out.open(QIODevice::WriteOnly))return false;
 const auto bytes=QJsonDocument(value).toJson();return out.write(bytes)==bytes.size()&&out.commit();
}
QJsonObject readJson(const QString &file) {
 QFile in(file);if(!in.open(QIODevice::ReadOnly)||in.size()>65536)return {};
 return QJsonDocument::fromJson(in.readAll()).object();
}
bool validPrepared(const QJsonObject &p,const QString &root) {
 const auto version=p["version"].toString();
 if(!QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+$").match(version).hasMatch())return false;
 const auto directory=QFileInfo(root+"/updates").canonicalFilePath();
 const auto executable=QFileInfo(p["executable"].toString());
 if(p["installed"].toBool())return executable.isFile()&&executable.canonicalFilePath()==QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
 if(directory.isEmpty()||!QDir::cleanPath(executable.absoluteFilePath()).startsWith(directory+"/")||!executable.isFile())return false;
 const auto canonical=executable.canonicalFilePath();
 if(!canonical.startsWith(directory+"/")&&!(p["nix"].toBool()&&canonical.startsWith("/nix/store/")))return false;
 return true;
}
}
namespace LauncherUpdater {
bool trustedAsset(const QString &raw,const QString &tag) {
 if(!QRegularExpression("^v?[0-9]+\\.[0-9]+\\.[0-9]+$").match(tag).hasMatch())return false;
 const QUrl url(raw);
 if(url.scheme()!="https"||!url.userInfo().isEmpty()||url.hasQuery()||url.hasFragment())return false;
 const auto path=url.path();
 if(path.contains("..")||path.contains('\\'))return false;
 if(url.host()=="github.com")return path.startsWith("/ebalia-real/launcher/releases/download/"+tag+"/");
 return url.host()=="gitgud.io"&&path.startsWith("/api/v4/projects/51367/packages/generic/ebalia-launcher/"+tag+"/");
}
QString assetName(const QString &platform,const QString &architecture) {
 if(platform=="windows"&&architecture=="x86_64")return "ebalia-windows-x64.zip";
 if(platform=="linux"&&architecture=="x86_64")return "ebalia-linux-x64-update.zip";
 if(platform=="macos"&&architecture=="arm64")return "ebalia-macos-arm64.dmg";
 return {};
}
QByteArray checksum(const QByteArray &manifest,const QString &name) {
 QByteArray result;
 for(auto line:manifest.split('\n')) {
  const auto match=QRegularExpression("^([a-fA-F0-9]{64})[ \\t]+\\*?(.+)$").match(QString::fromUtf8(line.trimmed()));
  if(match.hasMatch()&&match.captured(2)==name){if(!result.isEmpty())fail("Duplicate package checksum.");result=match.captured(1).toLatin1().toLower();}
 }
 if(result.isEmpty())fail("The release does not contain a checksum for this package.");
 return result;
}
QJsonObject prepare(const QJsonObject &release,const QString &root,Progress progress) {
 auto report=[&](int n,const QString &s){if(progress)progress(n,s);};
 const auto tag=release["tag_name"].toString();
 if(!UpdateChecker::newerStable(tag,QCoreApplication::applicationVersion()))fail("This release is not newer than the installed version.");
 QString version=tag;if(version.startsWith('v'))version.remove(0,1);
 const auto updates=QFileInfo(root+"/updates").absoluteFilePath();QDir().mkpath(updates);
 QLockFile lock(updates+"/prepare.lock");if(!lock.tryLock())fail("Another update is already being prepared.");
 const auto stage=updates+"/"+version+"-"+QUuid::createUuid().toString(QUuid::WithoutBraces);
 if(!QDir().mkpath(stage))fail("Could not create the update folder.");
 QJsonObject prepared{{"version",version},{"nix",false},{"stage",stage},{"parentPid",double(QCoreApplication::applicationPid())}};
#if defined(Q_OS_WIN)
 const QString destination=QCoreApplication::applicationDirPath();
#elif defined(Q_OS_MACOS)
 const QString destination=QDir::cleanPath(QCoreApplication::applicationDirPath()+"/../..");
#else
 const QString destination=QDir::cleanPath(QCoreApplication::applicationDirPath()+"/..");
#endif
 prepared["destination"]=destination;
 try {
#if defined(Q_OS_LINUX)
  const bool nix=QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath().startsWith("/nix/store/");
  if(nix) {
   auto program=QStandardPaths::findExecutable("nix");if(program.isEmpty())fail("Nix is required to update this installation.");
   report(-1,"Nix: preparing the new version. The first build may take several minutes.");
   run(program,{"--extra-experimental-features","nix-command flakes","build","github:ebalia-real/launcher/"+tag+"?dir=ebalia-launcher","--out-link",stage+"/result"},60*60*1000);
   prepared["executable"]=stage+"/result/bin/ebalia-launcher";prepared["nix"]=true;
  } else
#endif
  {
#if defined(Q_OS_WIN)
   const QString platform="windows";
#elif defined(Q_OS_MACOS)
   const QString platform="macos";
#else
   const QString platform="linux";
#endif
   #ifndef Q_OS_MACOS
   if(!QFileInfo::exists(destination+"/update-files.json"))fail("This installation does not support automatic replacement yet. Install version 1.1.0 once to enable future in-app updates.");
#endif
   if(!QFileInfo(destination).isWritable())fail("The launcher folder is not writable. Install it in a folder owned by your user to enable automatic updates.");
   const auto name=assetName(platform,QSysInfo::buildCpuArchitecture());
   if(name.isEmpty())fail("No automatic update package is available for this architecture.");
   QString assetUrl,manifestUrl;
   QJsonArray assets=release["assets"].isArray()?release["assets"].toArray():release["assets"].toObject()["links"].toArray();
   for(auto value:assets){auto a=value.toObject();auto url=a["browser_download_url"].toString();if(url.isEmpty())url=a["url"].toString();
    if(!trustedAsset(url,tag))continue;
    if(a["name"].toString()==name)assetUrl=url;
    if(a["name"].toString()=="SHA256SUMS.txt")manifestUrl=url;
   }
   if(assetUrl.isEmpty()||manifestUrl.isEmpty())fail("The update package is not ready yet. Please retry later.");
   report(0,"Checking release integrity…");
   Download::file(QUrl(manifestUrl),stage+"/SHA256SUMS.txt");QFile sums(stage+"/SHA256SUMS.txt");
   if(!sums.open(QIODevice::ReadOnly)||sums.size()>65536)fail("Invalid checksum manifest.");
   const auto digest=checksum(sums.readAll(),name);
   Download::file(QUrl(assetUrl),stage+"/"+name,digest,QCryptographicHash::Sha256,[&](qint64 done,qint64 total){report(total>0?int(done*90/total):-1,"Downloading update…");});
   report(92,"Preparing the new version…");
#ifdef Q_OS_MACOS
   const auto mount=stage+"/mount";QDir().mkpath(mount);
   run("/usr/bin/hdiutil",{"attach","-nobrowse","-readonly","-mountpoint",mount,stage+"/"+name},120000);
   try {
    auto apps=QDir(mount).entryList({"*.app"},QDir::Dirs);
    if(apps.size()!=1)fail("The update does not contain a single application.");
    auto bundle=stage+"/"+apps.front();run("/usr/bin/ditto",{mount+"/"+apps.front(),bundle},180000);
    prepared["executable"]=bundle+"/Contents/MacOS/ebalia-launcher";
   }catch(...){try{run("/usr/bin/hdiutil",{"detach",mount},30000);}catch(...){}throw;}
   run("/usr/bin/hdiutil",{"detach",mount},30000);
#else
   Archive::extract(stage+"/"+name,stage+"/app");
#ifdef Q_OS_WIN
   prepared["executable"]=stage+"/app/ebalia-launcher.exe";
#else
   const auto exe=stage+"/app/bin/ebalia-launcher";
   QFile::setPermissions(exe,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner|QFile::ReadGroup|QFile::ExeGroup|QFile::ReadOther|QFile::ExeOther);
   prepared["executable"]=exe;
#endif
#endif
   QFile::remove(stage+"/"+name);
  }
  if(!validPrepared(prepared,root))fail("The prepared executable is invalid.");
  report(96,"Verifying that the new launcher starts…");
  const auto output=run(prepared["executable"].toString(),{"--update-probe"},30000,environment(root,true));
  if(QJsonDocument::fromJson(output.trimmed()).object()["version"].toString()!=version)fail("The new launcher failed its startup check.");
  if(!prepared["nix"].toBool()) {
#ifndef Q_OS_MACOS
   auto source=QFileInfo(prepared["executable"].toString()).absolutePath();
#ifdef Q_OS_LINUX
   source=QDir::cleanPath(source+"/..");
#endif
   QFile manifest(QDir::cleanPath(source)+"/update-files.json");
   if(!manifest.open(QIODevice::ReadOnly)||manifest.size()>1024*1024||!QJsonDocument::fromJson(manifest.readAll()).object()["files"].isArray())fail("The update package has no valid file manifest.");
#endif
  }
  report(100,"Update ready. Restarting…");return prepared;
 }catch(...){QDir(stage).removeRecursively();throw;}
}
QString writePending(const QJsonObject &prepared,const QString &root) {
 if(!validPrepared(prepared,root))return {};
 const auto name=QUuid::createUuid().toString(QUuid::WithoutBraces)+".json";
 return writeJson(root+"/updates/"+name,prepared)?name:QString();
}
QJsonObject readPending(const QString &name,const QString &root) {
 if(!QRegularExpression("^[0-9a-f-]{36}\\.json$").match(name).hasMatch())return {};
 auto p=readJson(root+"/updates/"+name);
 if(!validPrepared(p,root)||p["version"].toString()!=QCoreApplication::applicationVersion()||QFileInfo(p["executable"].toString()).canonicalFilePath()!=QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath()) {
  // Nix wrappers exec a hidden binary in the same package.
  const auto target=QFileInfo(p["executable"].toString()).canonicalFilePath();
  const auto current=QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath();
  if(!validPrepared(p,root)||!p["nix"].toBool()||p["version"].toString()!=QCoreApplication::applicationVersion()||QFileInfo(target).absolutePath()!=QFileInfo(current).absolutePath())return {};
 }
 return p;
}
bool activate(const QJsonObject &prepared,const QString &root){
 if(!validPrepared(prepared,root)||!writeJson(root+"/updates/active.json",prepared))return false;
 QJsonArray folders,pendingFiles;
 const auto keep=prepared["nix"].toBool()?QFileInfo(prepared["stage"].toString()).canonicalFilePath():QString();
 for(const auto &name:QDir(root+"/updates").entryList(QDir::Dirs|QDir::NoDotAndDotDot))
  if(QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+-[0-9a-f-]{36}$").match(name).hasMatch()&&QFileInfo(root+"/updates/"+name).canonicalFilePath()!=keep)folders.append(name);
 for(const auto &name:QDir(root+"/updates").entryList({"*.json"},QDir::Files))if(QRegularExpression("^[0-9a-f-]{36}\\.json$").match(name).hasMatch())pendingFiles.append(name);
 writeJson(root+"/updates/cleanup.json",QJsonObject{{"version",prepared["version"]},{"folders",folders},{"pending",pendingFiles}});
 return true;
}
bool launch(const QJsonObject &prepared,const QString &root,const QString &pending) {
 if(!validPrepared(prepared,root))return false;
 QProcess process;process.setProcessEnvironment(environment(root));process.setProgram(prepared["executable"].toString());
 if(!pending.isEmpty())process.setArguments({prepared["installed"].toBool()?"--finish-update":"--apply-update",pending});
 process.setWorkingDirectory(QFileInfo(prepared["executable"].toString()).absolutePath());return process.startDetached();
}
namespace {
QStringList ownedFiles(const QString &folder) {
 QFile file(folder+"/update-files.json");
 if(!file.open(QIODevice::ReadOnly)||file.size()>1024*1024)fail("Cannot read the installed file manifest.");
 auto object=QJsonDocument::fromJson(file.readAll()).object();
 if(object["format"].toInt()!=1||!object["files"].isArray())fail("Invalid installed file manifest.");
 QStringList files;
 for(auto value:object["files"].toArray()) {
  const auto name=value.toString();
  if(name.isEmpty()||QDir::isAbsolutePath(name)||name.contains(':')||name.contains('\\')||name.split('/').contains("..")||name.split('/').contains("."))fail("Unsafe installed file path.");
  auto part=folder;
  for(auto component:name.split('/')){part+="/"+component;if(QFileInfo(part).isSymLink())fail("The installation contains a symbolic link.");}
  if(!files.contains(name))files.append(name);
 }
 if(!files.contains("update-files.json"))fail("Incomplete installed file manifest.");
 return files;
}
void copyFile(const QString &source,const QString &destination) {
 QDir().mkpath(QFileInfo(destination).absolutePath());
 if(!QFile::copy(source,destination)){QFile::remove(destination);fail("Could not install "+destination);}
 QFile::setPermissions(destination,QFile::permissions(source));
}
}
bool waitForParent(const QJsonObject &prepared) {
 const auto pid=qint64(prepared["parentPid"].toDouble());
 if(pid<=0||pid==QCoreApplication::applicationPid())return true;
#ifdef Q_OS_WIN
 HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,DWORD(pid));
 if(!process)return GetLastError()==ERROR_INVALID_PARAMETER;
 const bool exited=WaitForSingleObject(process,60000)==WAIT_OBJECT_0;CloseHandle(process);return exited;
#else
 QElapsedTimer timer;timer.start();
 while(timer.elapsed()<60000){if(kill(pid_t(pid),0)!=0&&errno==ESRCH)return true;QThread::msleep(100);}
 return false;
#endif
}
bool applyPrepared(const QJsonObject &prepared,const QString &root) {
 const auto destination=prepared["destination"].toString();
 const auto stage=prepared["stage"].toString();
 const auto updateRoot=QFileInfo(root+"/updates").canonicalFilePath();
 if(updateRoot.isEmpty()||QFileInfo(stage).canonicalFilePath().isEmpty()||!QFileInfo(stage).canonicalFilePath().startsWith(updateRoot+"/"))fail("Invalid staging directory.");
 if(prepared["nix"].toBool())return activate(prepared,root);
 if(!QDir::isAbsolutePath(destination)||!QFileInfo(destination).isWritable())fail("The installation directory is not writable.");
 QJsonObject installed=prepared;installed["installed"]=true;installed["parentPid"]=double(QCoreApplication::applicationPid());
 const auto backup=stage+"/previous";
#ifdef Q_OS_MACOS
 if(!destination.endsWith(".app"))fail("Invalid application bundle destination.");
 const auto source=QDir::cleanPath(QFileInfo(prepared["executable"].toString()).absolutePath()+"/../..");
 if(!QDir().rename(destination,backup))fail("Could not prepare the old application for replacement.");
 try {
  run("/usr/bin/ditto",{source,destination},180000);
  installed["executable"]=destination+"/Contents/MacOS/ebalia-launcher";
 }catch(...){QDir(destination).removeRecursively();QDir().rename(backup,destination);throw;}
#else
 auto source=QFileInfo(prepared["executable"].toString()).absolutePath();
#ifdef Q_OS_LINUX
 source=QDir::cleanPath(source+"/..");
#endif
 const auto oldFiles=ownedFiles(destination),newFiles=ownedFiles(source);
 QStringList moved,copied;
 try {
  for(const auto &name:newFiles)if(QFileInfo::exists(destination+"/"+name)&&!oldFiles.contains(name))fail("Update would overwrite a file not owned by the launcher: "+name);
  for(const auto &name:oldFiles)if(QFileInfo::exists(destination+"/"+name)) {
   QDir().mkpath(QFileInfo(backup+"/"+name).absolutePath());
   if(!QFile::rename(destination+"/"+name,backup+"/"+name))fail("Could not replace a launcher file. Close other launcher processes.");
   moved.append(name);
  }
  for(const auto &name:newFiles){copyFile(source+"/"+name,destination+"/"+name);copied.append(name);}
 }catch(...){for(const auto &name:copied)QFile::remove(destination+"/"+name);for(const auto &name:moved)QFile::rename(backup+"/"+name,destination+"/"+name);throw;}
#ifdef Q_OS_WIN
 installed["executable"]=destination+"/ebalia-launcher.exe";
#else
 installed["executable"]=destination+"/bin/ebalia-launcher";
#endif
#endif
 // Verify the copied deployment too, before committing the restart.
 try {
  auto output=run(installed["executable"].toString(),{"--update-probe"},30000,environment(root,true));
  if(QJsonDocument::fromJson(output.trimmed()).object()["version"].toString()!=prepared["version"].toString())fail("The installed update did not pass its startup check.");
  const auto name=QUuid::createUuid().toString(QUuid::WithoutBraces)+".json";
  if(!writeJson(root+"/updates/"+name,installed))fail("Could not record the update restart.");
  QProcess child;child.setProcessEnvironment(environment(root));child.setProgram(installed["executable"].toString());child.setArguments({"--finish-update",name});
  if(!child.startDetached())fail("Could not restart the installed launcher.");
  return true;
 }catch(...){
#ifdef Q_OS_MACOS
  QDir(destination).removeRecursively();QDir().rename(backup,destination);
#else
  for(const auto &name:newFiles)QFile::remove(destination+"/"+name);
  for(const auto &name:oldFiles)if(QFileInfo::exists(backup+"/"+name)){QDir().mkpath(QFileInfo(destination+"/"+name).absolutePath());QFile::rename(backup+"/"+name,destination+"/"+name);}
#endif
  throw;
 }
}
void cleanupInstalled(const QString &root) {
 const auto updates=root+"/updates";
 const auto cleanup=readJson(updates+"/cleanup.json");
 if(cleanup["version"].toString()!=QCoreApplication::applicationVersion())return;
 bool complete=true;
 for(const auto &value:cleanup["folders"].toArray()) {
  const auto name=value.toString();
  if(!QRegularExpression("^[0-9]+\\.[0-9]+\\.[0-9]+-[0-9a-f-]{36}$").match(name).hasMatch())continue;
  const QFileInfo directory(updates+"/"+name);
  if(directory.isSymLink()||!directory.exists())continue;
  if(!QDir(directory.absoluteFilePath()).removeRecursively())complete=false;
 }
 for(const auto &value:cleanup["pending"].toArray()) {
  const auto name=value.toString();if(QRegularExpression("^[0-9a-f-]{36}\\.json$").match(name).hasMatch())QFile::remove(updates+"/"+name);
 }
 if(!readJson(updates+"/active.json")["nix"].toBool())QFile::remove(updates+"/active.json");
 if(complete)QFile::remove(updates+"/cleanup.json");
 QDir().rmdir(updates);
}
bool forwardToInstalled(const QString &root) {
 const auto active=readJson(root+"/updates/active.json");
 return validPrepared(active,root)&&UpdateChecker::newerStable(active["version"].toString(),QCoreApplication::applicationVersion())&&launch(active,root);
}
}
