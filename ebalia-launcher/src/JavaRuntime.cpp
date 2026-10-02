#include "JavaRuntime.hpp"
#include <QtCore>
#include <algorithm>
namespace {
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
}
QStringList JavaRuntime::candidates(const QStringList &extraRoots){
    QStringList result;QSet<QString> seen;
    auto add=[&](const QString &value){auto path=executable(value);QFileInfo f(path);if(!f.isFile()||!f.isExecutable())return;auto key=f.canonicalFilePath();
#ifdef Q_OS_WIN
        key=key.toLower();
#endif
        if(!seen.contains(key)){seen.insert(key);result<<f.absoluteFilePath();}};
    for(auto name:{"EBALIA_JAVA_PATHS","PRISMLAUNCHER_JAVA_PATHS"})for(auto path:qEnvironmentVariable(name).split(QDir::listSeparator(),Qt::SkipEmptyParts))add(path);
    for(auto name:{"JAVA_HOME","JDK_HOME","JRE_HOME"}){auto value=qEnvironmentVariable(name);if(!value.isEmpty())add(value);}
    for(auto path:qEnvironmentVariable("PATH").split(QDir::listSeparator(),Qt::SkipEmptyParts)){
#ifdef Q_OS_WIN
        add(QDir(path).filePath("java.exe"));
#else
        add(QDir(path).filePath("java"));
#endif
    }
    auto home=QDir::homePath();QStringList roots=extraRoots;
    roots<<home+"/.gradle/jdks"<<home+"/.jdks"<<home+"/.sdkman/candidates/java"<<home+"/.jabba/jdk";
    for(auto base:QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))for(auto product:{"PrismLauncher","prismlauncher","MultiMC","multimc","UltimMC","ultimmc","minecraft",".minecraft"})roots<<base+"/"+product+"/java"<<base+"/"+product+"/runtime";
    // Reuse only explicitly configured runtime paths, never accounts or session data.
    for(auto base:QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))for(auto product:{"PrismLauncher","prismlauncher","MultiMC","UltimMC"}){
        auto folder=base+"/"+product;for(auto file:{"prismlauncher.cfg","multimc.cfg","ultimmc.cfg"}){auto path=folder+"/"+file;if(QFileInfo::exists(path)){QSettings cfg(path,QSettings::IniFormat);add(cfg.value("JavaPath").toString());}}
        for(auto instance:QDir(folder+"/instances").entryList(QDir::Dirs|QDir::NoDotAndDotDot)){auto path=folder+"/instances/"+instance+"/instance.cfg";if(QFileInfo::exists(path)){QSettings cfg(path,QSettings::IniFormat);add(cfg.value("JavaPath").toString());}}
    }
    roots<<home+"/.minecraft/runtime"<<home+"/.var/app/org.prismlauncher.PrismLauncher/data/PrismLauncher/java";
    auto data=qEnvironmentVariable("EBALIA_DATA_DIR",QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));roots<<data+"/java"<<data+"/runtimes"<<data+"/mc/java";
#ifdef Q_OS_WIN
    for(auto base:{qEnvironmentVariable("ProgramFiles"),qEnvironmentVariable("ProgramFiles(x86)"),qEnvironmentVariable("LOCALAPPDATA")})if(!base.isEmpty())for(auto vendor:{"Java","Eclipse Adoptium","Eclipse Foundation","AdoptOpenJDK","Microsoft","Zulu","BellSoft","Amazon Corretto","Programs/Eclipse Adoptium"})roots<<base+"/"+vendor;
    for(auto hive:{"HKEY_LOCAL_MACHINE","HKEY_CURRENT_USER"})for(auto format:{QSettings::Registry64Format,QSettings::Registry32Format})for(auto vendor:{"JavaSoft","Eclipse Adoptium","Eclipse Foundation","AdoptOpenJDK","Microsoft/JDK","Azul Systems/Zulu","BellSoft","Amazon Corretto"}){
        QSettings registry(QString(hive)+"\\SOFTWARE\\"+QString(vendor).replace('/','\\'),format);
        std::function<void(int)> scan=[&](int depth){for(auto key:registry.childKeys())if(key.compare("JavaHome",Qt::CaseInsensitive)==0||key.compare("Path",Qt::CaseInsensitive)==0||key.compare("InstallationPath",Qt::CaseInsensitive)==0)add(registry.value(key).toString());if(depth==0)return;for(auto group:registry.childGroups()){registry.beginGroup(group);scan(depth-1);registry.endGroup();}};scan(5);
    }
#elif defined(Q_OS_MACOS)
    roots<<"/Library/Java/JavaVirtualMachines"<<home+"/Library/Java/JavaVirtualMachines"<<"/opt/homebrew/opt"<<"/usr/local/opt";
    QProcess helper;helper.start("/usr/libexec/java_home",{"-V"});if(helper.waitForFinished(2000)){auto lines=QString::fromUtf8(helper.readAllStandardError()+helper.readAllStandardOutput()).split('\n');for(auto line:lines){auto idx=line.indexOf("/Library/");if(idx>=0)add(line.mid(idx).trimmed());}}
#else
    roots<<"/usr/lib/jvm"<<"/usr/lib64/jvm"<<"/usr/java"<<"/usr/local/lib/jvm"<<"/opt/java"<<"/opt/jdk"<<"/opt/jdks";
    for(auto base:{QString("/run/current-system/sw"),QString("/nix/var/nix/profiles/default"),home+"/.nix-profile","/etc/profiles/per-user/"+qEnvironmentVariable("USER")}){add(base+"/bin/java");roots<<base+"/lib/jvm"<<base+"/lib/openjdk";}
#endif
    QSet<QString> visited;int count=0;
    std::function<void(QString,int)> scan=[&](QString dir,int depth){if(depth<0||++count>5000)return;QFileInfo f(dir);if(!f.isDir())return;auto canonical=f.canonicalFilePath();if(visited.contains(canonical))return;visited.insert(canonical);add(dir);
        for(const auto &entry:QDir(dir).entryInfoList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Name)){auto n=entry.fileName();if(QStringList{"lib","jmods","legal","include","man","conf","logs","instances","assets"}.contains(n))continue;
#ifdef Q_OS_WIN
            if(n=="bin"){add(entry.filePath()+"/java.exe");continue;}
#else
            if(n=="bin"){add(entry.filePath()+"/java");continue;}
#endif
            scan(entry.filePath(),depth-1);
        }};
    roots.removeDuplicates();for(auto root:roots)scan(root,5);return result;
}
JavaInstallation JavaRuntime::parse(const QString &path,const QString &output){
    JavaInstallation result;result.path=path;
    auto property=[&](QString key){return QRegularExpression("(?:^|\\n)\\s*"+QRegularExpression::escape(key)+"\\s*=\\s*([^\\r\\n]+)").match(output).captured(1).trimmed();};
    result.version=property("java.version");result.architecture=property("os.arch");result.vendor=property("java.vendor");
    if(result.version.isEmpty())result.version=QRegularExpression("(?:openjdk|java)(?: version)?\\s+\"?([0-9][^\\s\"]*)",QRegularExpression::CaseInsensitiveOption).match(output).captured(1);
    auto version=QRegularExpression("^(?:1\\.)?(\\d+)").match(result.version);result.major=version.captured(1).toInt();if(result.architecture.isEmpty())result.architecture=output.contains("64-Bit")?"64-bit":"unknown";return result;
}
JavaInstallation JavaRuntime::inspect(const QString &path){
    auto bin=executable(path);QProcess process;auto env=QProcessEnvironment::systemEnvironment();for(auto key:{"JAVA_TOOL_OPTIONS","_JAVA_OPTIONS","JDK_JAVA_OPTIONS"})env.remove(key);process.setProcessEnvironment(env);process.setProcessChannelMode(QProcess::MergedChannels);process.start(bin,{"-XshowSettings:properties","-version"});if(!process.waitForFinished(4000)){process.kill();process.waitForFinished(1000);return {bin};}if(process.exitStatus()!=QProcess::NormalExit||process.exitCode()!=0)return {bin};return parse(bin,QString::fromUtf8(process.readAll()));
}
QList<JavaInstallation> JavaRuntime::discover(const QStringList &roots){QList<JavaInstallation> out;for(auto p:candidates(roots)){auto i=inspect(p);if(i.major)out<<i;}std::stable_sort(out.begin(),out.end(),[](auto a,auto b){return a.major>b.major;});return out;}
QString JavaRuntime::select(int required,const QString &overridePath){if(!overridePath.trimmed().isEmpty()){auto i=inspect(overridePath);return i.major==required?i.path:QString();}for(auto p:candidates()){auto i=inspect(p);if(i.major==required)return i.path;}return {};}
