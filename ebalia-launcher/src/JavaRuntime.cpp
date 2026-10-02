#include "JavaRuntime.hpp"
#include <QtCore>
#include <QtConcurrent>
#include <algorithm>
#include <functional>
namespace {
#ifdef Q_OS_WIN
const QString javaName="java.exe";
#else
const QString javaName="java";
#endif
QString executable(QString path){
    path=QDir::fromNativeSeparators(path.trimmed());if(path.startsWith('"')&&path.endsWith('"'))path=path.mid(1,path.size()-2);
    if(QFileInfo(path).isDir()){
#ifdef Q_OS_WIN
        path=QDir(path).filePath("bin/java.exe");
#else
        auto mac=QDir(path).filePath("Contents/Home/bin/java");path=QFileInfo::exists(mac)?mac:QDir(path).filePath("bin/java");
#endif
    }
#ifdef Q_OS_WIN
    if(path.endsWith("javaw.exe",Qt::CaseInsensitive)){auto console=QFileInfo(path).dir().filePath("java.exe");if(QFileInfo::exists(console))path=console;}
#endif
    if(!QFileInfo(path).isAbsolute()){auto found=QStandardPaths::findExecutable(path);if(!found.isEmpty())path=found;}
    return path;
}
// Successful inspections are reused while the executable is unchanged; failures are retried (a cold JVM start can time out once).
struct Cached{QString stamp;JavaInstallation java;};
QMutex cacheMutex;
QHash<QString,Cached> cache;
QString stamp(const QFileInfo &f){return QString::number(f.lastModified().toMSecsSinceEpoch())+":"+QString::number(f.size());}
QString run(const QString &bin,const QStringList &args){
    QProcess process;auto env=QProcessEnvironment::systemEnvironment();for(auto key:{"JAVA_TOOL_OPTIONS","_JAVA_OPTIONS","JDK_JAVA_OPTIONS"})env.remove(key);
    process.setProcessEnvironment(env);process.setProcessChannelMode(QProcess::MergedChannels);process.start(bin,args);
    // The first start of a JVM can take several seconds while an antivirus scans it.
    if(!process.waitForFinished(15000)){process.kill();process.waitForFinished(1000);return {};}
    if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)return {};
    return QString::fromLocal8Bit(process.readAll());
}
}
QString JavaRuntime::dataDir(){return qEnvironmentVariable("EBALIA_DATA_DIR",QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));}
QStringList JavaRuntime::candidates(const QStringList &extraRoots){
    QStringList result;QSet<QString> seen;
    auto add=[&](const QString &value){if(value.trimmed().isEmpty())return;auto path=executable(value);QFileInfo f(path);if(!f.isFile()||!f.isExecutable())return;auto key=f.canonicalFilePath();
#ifdef Q_OS_WIN
        key=key.toLower();
#endif
        if(!seen.contains(key)){seen.insert(key);result<<f.absoluteFilePath();}};
    // Paths saved by Prism/MultiMC may be relative to their data folder.
    auto configured=[&](const QString &folder,QString value){if((value.contains('/')||value.contains('\\'))&&QDir::isRelativePath(QDir::fromNativeSeparators(value)))value=folder+"/"+value;add(value);};
    for(auto name:{"EBALIA_JAVA_PATHS","PRISMLAUNCHER_JAVA_PATHS"})for(auto path:qEnvironmentVariable(name).split(QDir::listSeparator(),Qt::SkipEmptyParts))add(path);
    add(QSettings().value("mc/defaultJava").toString());
    for(auto name:{"JAVA_HOME","JDK_HOME","JRE_HOME"})add(qEnvironmentVariable(name));
    for(auto path:qEnvironmentVariable("PATH").split(QDir::listSeparator(),Qt::SkipEmptyParts))add(QDir(path).filePath(javaName));
    const auto home=QDir::homePath();
    auto env=[](const char *name,const QString &fallback){auto value=qEnvironmentVariable(name);return value.isEmpty()?fallback:value;};
    QStringList roots=extraRoots;QList<QPair<QString,int>> shallow;
    // Java downloaded by IntelliJ, Gradle toolchains, SDKMAN, asdf, mise, jabba and JBang.
    roots<<home+"/.jdks"<<env("GRADLE_USER_HOME",home+"/.gradle")+"/jdks"<<env("SDKMAN_DIR",home+"/.sdkman")+"/candidates/java"<<env("ASDF_DATA_DIR",home+"/.asdf")+"/installs/java"
         <<env("MISE_DATA_DIR",home+"/.local/share/mise")+"/installs/java"<<home+"/.jabba/jdk"<<home+"/.jbang/cache/jdks";
    // Runtimes managed by Minecraft launchers. Only Java folders and configured Java paths are read, never accounts or sessions.
    auto data=QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
#ifdef Q_OS_WIN
    const auto appData=qEnvironmentVariable("APPDATA"),local=qEnvironmentVariable("LOCALAPPDATA");
    data.prepend(appData); // Prism, ATLauncher, Modrinth App and .minecraft live in Roaming, not in GenericDataLocation.
    roots<<appData+"/.minecraft/runtime"<<local+"/Packages/Microsoft.4297127D64EC6_8wekyb3d8bbwe/LocalCache/Local/runtime"<<appData+"/.technic/runtimes"
         <<home+"/curseforge/minecraft/Install/runtime"<<local+"/.ftba/bin"<<local+"/.ftba/runtime"<<appData+"/.tlauncher/jvms";
    for(auto base:{qEnvironmentVariable("ProgramFiles(x86)"),qEnvironmentVariable("ProgramFiles")})if(!base.isEmpty())roots<<base+"/Minecraft Launcher/runtime";
#elif defined(Q_OS_MACOS)
    roots<<home+"/Library/Application Support/minecraft/runtime"<<home+"/Documents/curseforge/minecraft/Install/runtime"<<home+"/Library/Application Support/.ftba/bin"
         <<home+"/Library/Application Support/technic/runtimes";
#else
    roots<<home+"/.minecraft/runtime"<<home+"/curseforge/minecraft/Install/runtime"<<home+"/.ftba/bin"<<home+"/.ftba/runtime"<<home+"/.technic/runtimes"
         <<home+"/.var/app/org.prismlauncher.PrismLauncher/data/PrismLauncher/java"<<home+"/.var/app/com.modrinth.ModrinthApp/data/ModrinthApp/meta/java_versions";
#endif
    for(const auto &base:data){
        if(base.isEmpty())continue;
        roots<<base+"/.minecraft/runtime"<<base+"/ATLauncher/runtimes"<<base+"/ModrinthApp/meta/java_versions"<<base+"/com.modrinth.theseus/meta/java_versions"
             <<base+"/gdlauncher_carbon/data/java"<<base+"/gdlauncher_next/datastore/java";
        for(auto product:{"PrismLauncher","prismlauncher","PolyMC","polymc","MultiMC","multimc","UltimMC","ultimmc"}){
            auto folder=base+"/"+product;roots<<folder+"/java";
            for(auto file:{"prismlauncher.cfg","polymc.cfg","multimc.cfg","ultimmc.cfg"}){auto path=folder+"/"+file;if(QFileInfo::exists(path)){QSettings cfg(path,QSettings::IniFormat);configured(folder,cfg.value("JavaPath").toString());}}
            for(auto instance:QDir(folder+"/instances").entryList(QDir::Dirs|QDir::NoDotAndDotDot)){auto path=folder+"/instances/"+instance+"/instance.cfg";if(QFileInfo::exists(path)){QSettings cfg(path,QSettings::IniFormat);configured(folder,cfg.value("JavaPath").toString());}}
        }
    }
    const auto own=dataDir();roots<<own+"/mc/java"<<own+"/java"<<own+"/runtimes";
#ifdef Q_OS_WIN
    for(auto base:{qEnvironmentVariable("ProgramFiles"),qEnvironmentVariable("ProgramW6432"),qEnvironmentVariable("ProgramFiles(x86)"),local+"/Programs"})if(!base.isEmpty())
        for(auto vendor:{"Java","Eclipse Adoptium","Eclipse Foundation","AdoptOpenJDK","Microsoft","Zulu","BellSoft","Amazon Corretto","Semeru","IBM","RedHat","OpenJDK","ojdkbuild","GraalVM"})roots<<base+"/"+vendor;
    for(auto scoop:{env("SCOOP",home+"/scoop"),env("SCOOP_GLOBAL",qEnvironmentVariable("ProgramData")+"/scoop")})shallow<<qMakePair(scoop+"/apps",2);
    for(auto hive:{"HKEY_LOCAL_MACHINE","HKEY_CURRENT_USER"})for(auto format:{QSettings::Registry64Format,QSettings::Registry32Format})
        for(auto vendor:{"JavaSoft","Eclipse Adoptium","Eclipse Foundation","AdoptOpenJDK","Microsoft/JDK","Azul Systems/Zulu","BellSoft","Amazon Corretto","Semeru"}){
        QSettings registry(QString(hive)+"\\SOFTWARE\\"+QString(vendor).replace('/','\\'),format);
        std::function<void(int)> scan=[&](int depth){for(auto key:registry.childKeys())if(key.compare("JavaHome",Qt::CaseInsensitive)==0||key.compare("Path",Qt::CaseInsensitive)==0||key.compare("InstallationPath",Qt::CaseInsensitive)==0)add(registry.value(key).toString());if(depth==0)return;for(auto group:registry.childGroups()){registry.beginGroup(group);scan(depth-1);registry.endGroup();}};scan(5);
    }
#elif defined(Q_OS_MACOS)
    roots<<"/Library/Java/JavaVirtualMachines"<<home+"/Library/Java/JavaVirtualMachines"<<"/opt/local/Library/Java/JavaVirtualMachines"<<"/Library/Internet Plug-Ins/JavaAppletPlugin.plugin/Contents";
    shallow<<qMakePair(QString("/opt/homebrew/opt"),3)<<qMakePair(QString("/usr/local/opt"),3);
    QProcess helper;helper.start("/usr/libexec/java_home",{"-V"});if(helper.waitForFinished(5000)){auto lines=QString::fromUtf8(helper.readAllStandardError()+helper.readAllStandardOutput()).split('\n');for(auto line:lines){auto idx=line.indexOf("/Library/");if(idx>=0)add(line.mid(idx).trimmed());}}
#else
    roots<<"/usr/lib/jvm"<<"/usr/lib64/jvm"<<"/usr/lib32/jvm"<<"/usr/java"<<"/usr/local/lib/jvm"<<"/usr/local/java"<<"/opt/java"<<"/opt/jdk"<<"/opt/jdks"<<"/opt/ibm"<<"/app/jdk";
    // Gentoo, AOSC and manual installs use vendor-named folders directly in /opt or /usr/lib.
    static const QRegularExpression vendorDir("jdk|jre|java|openj9|temurin|zulu|corretto|graalvm|liberica|semeru|icedtea",QRegularExpression::CaseInsensitiveOption);
    for(auto base:{"/opt","/usr/lib","/usr/lib64"})for(auto name:QDir(base).entryList(QDir::Dirs|QDir::NoDotAndDotDot))if(vendorDir.match(name).hasMatch())roots<<QString(base)+"/"+name;
    for(auto base:{QString("/run/current-system/sw"),QString("/nix/var/nix/profiles/default"),home+"/.nix-profile","/etc/profiles/per-user/"+qEnvironmentVariable("USER")}){add(base+"/bin/java");roots<<base+"/lib/jvm"<<base+"/lib/openjdk";}
#endif
    static const QStringList skipped{"lib","jmods","legal","include","man","conf","logs","instances","assets","libraries","saves","mods","demo","sample","src",".git","node_modules"};
    QSet<QString> visited;
    std::function<void(const QString &,int,int &)> scan=[&](const QString &dir,int depth,int &budget){
        if(depth<0||--budget<0)return;QFileInfo f(dir);if(!f.isDir())return;auto canonical=f.canonicalFilePath();if(visited.contains(canonical))return;visited.insert(canonical);add(dir);
        for(const auto &entry:QDir(dir).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name)){
            auto n=entry.fileName();if(n=="bin"){add(entry.filePath()+"/"+javaName);continue;}
            if(!skipped.contains(n))scan(entry.filePath(),depth-1,budget);
        }};
    roots.removeDuplicates();for(const auto &root:roots){int budget=2000;scan(root,5,budget);}
    for(const auto &root:shallow){int budget=2000;scan(root.first,root.second,budget);}
    return result;
}
JavaInstallation JavaRuntime::parse(const QString &path,const QString &output){
    JavaInstallation result;result.path=path;
    auto property=[&](QString key){return QRegularExpression("(?:^|\\n)\\s*"+QRegularExpression::escape(key)+"\\s*=\\s*([^\\r\\n]+)").match(output).captured(1).trimmed();};
    result.version=property("java.version");result.architecture=property("os.arch");result.vendor=property("java.vendor");
    if(result.version.isEmpty())result.version=QRegularExpression("(?:openjdk|java)(?: version)?\\s+\"?([0-9][^\\s\"]*)",QRegularExpression::CaseInsensitiveOption).match(output).captured(1);
    auto version=QRegularExpression("^(?:1\\.)?(\\d+)").match(result.version);result.major=version.captured(1).toInt();if(result.architecture.isEmpty())result.architecture=output.contains("64-Bit")?"64-bit":"unknown";return result;
}
JavaInstallation JavaRuntime::inspect(const QString &path){
    auto bin=executable(path);QFileInfo f(bin);if(!f.isFile())return {bin};
    const auto key=f.canonicalFilePath(),version=stamp(f);
    {QMutexLocker lock(&cacheMutex);auto it=cache.constFind(key);if(it!=cache.cend()&&it->stamp==version){auto java=it->java;java.path=bin;return java;}}
    auto output=run(bin,{"-XshowSettings:properties","-version"});
    if(output.isEmpty())output=run(bin,{"-version"}); // Java 6 and some embedded runtimes do not support -XshowSettings.
    if(output.isEmpty())return {bin};
    auto java=parse(bin,output);if(java.major){QMutexLocker lock(&cacheMutex);cache.insert(key,{version,java});}return java;
}
QList<JavaInstallation> JavaRuntime::discover(const QStringList &roots){
    auto paths=candidates(roots);QThreadPool pool;pool.setMaxThreadCount(qBound(2,QThread::idealThreadCount(),8));
    auto inspected=QtConcurrent::blockingMapped<QList<JavaInstallation>>(&pool,paths,[](const QString &path){return inspect(path);});
    QList<JavaInstallation> out;for(const auto &java:inspected)if(java.major)out<<java;
    std::stable_sort(out.begin(),out.end(),[](const JavaInstallation &a,const JavaInstallation &b){if(a.major!=b.major)return a.major>b.major;return a.architecture.contains("64")&&!b.architecture.contains("64");});
    return out;
}
bool JavaRuntime::compatible(int required,int major){return major>0&&(major==required||(required>=16&&major>required));}
QString JavaRuntime::select(int required,const QString &overridePath){
    if(!overridePath.trimmed().isEmpty()){auto java=inspect(overridePath);return compatible(required,java.major)?java.path:QString();}
    auto all=discover();int best=0;
    for(const auto &java:all)if(compatible(required,java.major)&&(!best||java.major==required||(best!=required&&java.major<best)))best=java.major;
    for(const auto &java:all)if(java.major==best&&best)return java.path; // 64-bit first within the same major
    return {};
}
