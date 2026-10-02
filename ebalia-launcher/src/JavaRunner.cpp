#include "JavaRunner.hpp"
#include "JavaRuntime.hpp"
#include "ModRepository.hpp"
#include <QSettings>
#include "VersionManager.hpp"
#include <QDirIterator>
#include <QFileInfo>
#include <QDir>
#include <QProcessEnvironment>
#include <QStandardPaths>

JavaRunner::JavaRunner(QObject *parent)
    : QObject(parent)
    , m_process(nullptr)
{
}

JavaRunner::~JavaRunner()
{
    if (m_process) {
        m_process->close();
        m_process->deleteLater();
    }
}

void JavaRunner::locateJava()
{
    m_javaPaths = findJava();
    if (!m_javaPaths.isEmpty()) {
        emit javaFound(m_javaPaths.first());
    } else {
        emit javaNotFound();
    }
}

QStringList JavaRunner::findJava() { return JavaRuntime::candidates(); }

void JavaRunner::launch(const VersionInfo &version, const QString &installDir)
{
    if(m_process && m_process->state()!=QProcess::NotRunning){emit processError("A lost version is already running.");return;}
    QString target;try{auto saved=ModRepository::read(installDir+"/.installed.json")["launchFile"].toString();if(!saved.isEmpty()&&!QDir::isAbsolutePath(saved)&&!saved.split('/').contains(".."))target=QDir(installDir).filePath(saved);}catch(...){}
    if(target.isEmpty())target=QDir(installDir).filePath(version.workingDir+"/"+version.launchCommand);
    if(!QFileInfo(target).isFile())target=QDir(installDir).filePath(version.launchCommand);
    if(!QFileInfo(target).isFile()){
        QStringList matches;QDirIterator files(installDir,QDir::Files,QDirIterator::Subdirectories);
        while(files.hasNext()){files.next();if(files.fileName().compare(QFileInfo(version.launchCommand).fileName(),Qt::CaseInsensitive)==0)matches<<files.filePath();}
        if(matches.size()!=1){emit processError("The original launch file is missing or ambiguous. Reinstall this version.");return;}target=matches.first();
    }
    auto env=QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_WIN
    auto java8=JavaRuntime::select(8);if(!java8.isEmpty()){env.insert("JAVA_HOME",QFileInfo(QFileInfo(java8).absolutePath()).absolutePath());env.insert("PATH",QFileInfo(java8).absolutePath()+";"+env.value("PATH"));}
    if(version.launcher=="UltimMC")env.insert("APPDATA",QDir::toNativeSeparators(installDir));
#else
    auto prefix=qEnvironmentVariable("EBALIA_WINEPREFIX");if(!prefix.isEmpty())env.insert("WINEPREFIX",prefix);
#endif
    QString program;QStringList args;auto suffix=QFileInfo(target).suffix().toLower();
    if(suffix=="exe"||suffix=="bat"||suffix=="cmd"){
#ifdef Q_OS_WIN
        if(suffix=="exe")program=target;
        else{program="cmd.exe";args={"/c",target};}
#else
        program=QStandardPaths::findExecutable("wine");
        if(program.isEmpty()){emit processError("This Windows package requires Wine. Install Wine or run it on Windows.");return;}
        if(suffix=="exe")args={target};else args={"cmd","/c",target};
#endif
    }else if(suffix=="jar"){
        auto java=findJava();if(java.isEmpty()){emit processError("Install Java for this version before launching.");return;}
        program=java.first();args={"-Xmx1024m","-jar",target};
    }else if(suffix=="sh") {program="/bin/sh";args={target};}
    else {emit processError("Unsupported launch file: "+version.launchCommand);return;}
    if(m_process)m_process->deleteLater();m_process=new QProcess(this);
    m_process->setProcessEnvironment(env);
    m_process->setWorkingDirectory(QFileInfo(target).absolutePath());m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setStandardOutputFile(installDir+"/launcher.log");
    connect(m_process,&QProcess::started,this,&JavaRunner::processStarted);
    connect(m_process,&QProcess::errorOccurred,this,&JavaRunner::onErrorOccurred);
    connect(m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,&JavaRunner::onFinished);
    m_process->start(program,args);
}

void JavaRunner::onErrorOccurred(QProcess::ProcessError error)
{
    Q_UNUSED(error)
    emit processError("Process error: " + (m_process ? m_process->errorString() : "unknown"));
}

void JavaRunner::onFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(status)
    emit processFinished(exitCode);

    if (m_process) {
        m_process->deleteLater();
        m_process = nullptr;
    }
}
