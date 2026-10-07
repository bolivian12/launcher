#include <QApplication>
#include <QTimer>
#include <QIcon>
#include <QDir>
#include <QStandardPaths>
#include <QLockFile>
#include <QMessageBox>
#include <QThreadPool>
#include "MainWindow.hpp"
#include "UpdateChecker.hpp"
#include "LauncherUpdater.hpp"
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>
#include <QJsonDocument>
#include <QPointer>
#include <QStatusBar>
#include <QPushButton>
#include <QDesktopServices>
#include "Language.hpp"
#include <QTranslator>
#include <QJsonObject>
#include "ModRepository.hpp"
#include "JavaRuntime.hpp"
#include "JavaDownloader.hpp"
#include "LostInstaller.hpp"
#include "LostNative.hpp"
#include "JavaRunner.hpp"
#include "VersionManager.hpp"
#include <cstdio>
#include <cstdlib>
#include <QSettings>
#include <QLocale>
#include <QFileInfo>
// Portable mode: a portable.txt beside the program (beside the AppImage file or
// the .app bundle) keeps every instance, account and setting in an
// "ebalia-data" folder next to it, so the whole launcher can live on a USB drive.
static QString portableRoot(){
    if(qEnvironmentVariableIsSet("EBALIA_PORTABLE")&&qEnvironmentVariableIsSet("EBALIA_DATA_DIR"))return qEnvironmentVariable("EBALIA_DATA_DIR");
    QStringList bases{QCoreApplication::applicationDirPath()};
#if defined(Q_OS_MACOS)
    bases={QDir::cleanPath(QCoreApplication::applicationDirPath()+"/../../..")};
#elif defined(Q_OS_LINUX)
    const auto appImage=qEnvironmentVariable("APPIMAGE");
    if(!appImage.isEmpty())bases={QFileInfo(appImage).absolutePath()};
    else bases<<QDir::cleanPath(QCoreApplication::applicationDirPath()+"/..");
#endif
    for(const auto &base:bases)if(QFileInfo::exists(base+"/portable.txt"))return base+"/ebalia-data";
    return {};
}
static void setEnvironment(const char *name,const QString &value){
#ifdef Q_OS_WIN
    _wputenv_s(reinterpret_cast<const wchar_t*>(QString::fromLatin1(name).utf16()),reinterpret_cast<const wchar_t*>(value.utf16())); // keeps non-ASCII folders intact
#else
    qputenv(name,QFile::encodeName(value));
#endif
}
class ButtonTranslator:public QTranslator { public: QString translate(const char *,const char *s,const char *,int) const override {return Language::standard(QString::fromUtf8(s));} bool isEmpty() const override {return false;} };
int main(int argc,char **argv){
    QApplication::setOrganizationName("EBALIA");QApplication::setApplicationName("EBALIA Launcher");QApplication::setApplicationVersion(EBALIA_APP_VERSION);
    QApplication app(argc,argv);
    if(const auto portable=portableRoot();!portable.isEmpty()&&QDir().mkpath(portable)) {
        setEnvironment("EBALIA_DATA_DIR",portable);setEnvironment("EBALIA_PORTABLE","1");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,portable+"/settings");
    }
    app.setStyle("Fusion");app.setWindowIcon(QIcon(":/icon.png"));
    app.setDesktopFileName("ebalia-launcher");
    if(app.arguments().contains("--update-probe")) {fprintf(stdout,"{\"version\":\"%s\"}\n",EBALIA_APP_VERSION);return 0;}
    auto root=qEnvironmentVariable("EBALIA_DATA_DIR");if(root.isEmpty())root=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(root);
    const int applying=app.arguments().indexOf("--apply-update"),finishing=app.arguments().indexOf("--finish-update");
    const int updateArg=applying>=0?applying:finishing;
    QJsonObject pending;
    if(updateArg>=0) {
        if(updateArg+1>=app.arguments().size())return 1;
        pending=LauncherUpdater::readPending(app.arguments()[updateArg+1],root);
        if(pending.isEmpty()||!LauncherUpdater::waitForParent(pending))return 1;
    }else if(!app.arguments().contains("--selftest")&&LauncherUpdater::forwardToInstalled(root))return 0;
    Language::current=QSettings().value("ui/language",QLocale::system().name().left(2)).toString();
    if(qEnvironmentVariableIsSet("EBALIA_LANGUAGE"))Language::current=qEnvironmentVariable("EBALIA_LANGUAGE");
    if(!Language::available().contains(Language::current))Language::current="en";
    QLockFile lock(root+"/launcher.lock");if(!lock.tryLock(updateArg>=0?30000:0)){QMessageBox::information(nullptr,"EBALIA",Language::text("EBALIA ya está abierto con esta carpeta de datos.","EBALIA is already running with this data folder.","O EBALIA já está aberto com esta pasta de dados."));return 1;}
    if(applying>=0&&!pending["nix"].toBool()) {
        try {return LauncherUpdater::applyPrepared(pending,root)?0:1;}
        catch(const std::exception &e){
            QMessageBox::critical(nullptr,"EBALIA · Update",QString::fromUtf8(e.what()));
            lock.unlock();
#ifdef Q_OS_WIN
            const auto old=pending["destination"].toString()+"/ebalia-launcher.exe";
#elif defined(Q_OS_MACOS)
            const auto old=pending["destination"].toString()+"/Contents/MacOS/ebalia-launcher";
#else
            const auto old=pending["appimage"].toBool()?pending["destination"].toString():pending["destination"].toString()+"/bin/ebalia-launcher";
#endif
            QProcess::startDetached(old,{});return 1;
        }
    }
    // Command-line test modes: on Windows the launcher is a GUI program whose standard error may not reach the caller.
    if(const auto testLog=qEnvironmentVariable("EBALIA_TEST_LOG");!testLog.isEmpty()){if(std::freopen(QFile::encodeName(testLog).constData(),"a",stderr))std::setvbuf(stderr,nullptr,_IONBF,0);}
    auto cli=app.arguments();if(cli.contains("--javatest")){for(auto j:JavaRuntime::discover())fprintf(stdout,"Java %s | %s | %s\n",j.version.toUtf8().constData(),j.architecture.toUtf8().constData(),j.path.toUtf8().constData());return 0;}
    int javaDownload=cli.indexOf("--javadownload");if(javaDownload>=0&&javaDownload+1<cli.size()){try{auto java=JavaDownloader::install(root+"/mc/java",JavaDownloader::component(cli[javaDownload+1].toInt()),{},[](int done,int total){fprintf(stderr,"\rJava %d / %d",done,total);});auto check=JavaRuntime::inspect(java);fprintf(stdout,"\nJava %s | %s | %s\n",check.version.toUtf8().constData(),check.architecture.toUtf8().constData(),java.toUtf8().constData());return check.major?0:1;}catch(const std::exception &e){fprintf(stderr,"%s\n",e.what());return 1;}}
    int lostLaunch=cli.indexOf("--lost-launchtest");if(lostLaunch>=0&&lostLaunch+1<cli.size()){
        VersionManager vm;vm.loadVersions();VersionInfo version;for(auto v:vm.getVersions())if(v.id==cli[lostLaunch+1])version=v;if(version.id.isEmpty()||!vm.isVersionInstalled(version)){fprintf(stderr,"Version not installed\n");return 1;}
        const bool native=LostNative::available(version.native);const auto libraries=root+"/mc/libraries";
        QString java;try{java=LostInstaller::java(root,!native&&LostInstaller::windowsPackage(version),[](int done,int total){fprintf(stderr,"\rJava %d / %d",done,total);});if(native)LostNative::prepare(version.native,vm.getInstallPath(version),libraries);}catch(const std::exception &e){fprintf(stderr,"%s\n",e.what());return 1;}
        fprintf(stderr,"\nJava 8: %s\n",java.toUtf8().constData());JavaRunner runner;bool started=false;
        QObject::connect(&runner,&JavaRunner::processStarted,&app,[&]{started=true;fprintf(stderr,"PROCESS STARTED\n");const int seconds=qEnvironmentVariableIntValue("EBALIA_LAUNCHTEST_SECONDS");QTimer::singleShot((seconds>0?seconds:40)*1000,&runner,[&]{runner.stop();});});
        QObject::connect(&runner,&JavaRunner::processFinished,&app,[&](int code){fprintf(stderr,"PROCESS ENDED code=%d\n",code);app.exit(started?0:1);});
        QObject::connect(&runner,&JavaRunner::processError,&app,[&](const QString &e){fprintf(stderr,"ERROR: %s\n",e.toUtf8().constData());if(!started)app.exit(1);});
        QTimer::singleShot(0,&runner,[&]{runner.launch(version,vm.getInstallPath(version),java,"EbaliaTest",libraries);});auto code=app.exec();
        QFile log(vm.getInstallPath(version)+"/launcher.log");if(log.open(QIODevice::ReadOnly))fprintf(stderr,"--- launcher.log ---\n%s\n",log.readAll().right(4000).constData());return code;
    }
    int lostTest=cli.indexOf("--lost-installtest");if(lostTest>=0&&lostTest+1<cli.size()){VersionManager vm;vm.loadVersions();for(auto v:vm.getVersions())if(v.id==cli[lostTest+1]){try{LostInstaller::install(v,vm.getInstallPath(v),[](qint64 a,qint64 b){if(b>0)fprintf(stderr,"\rLost archive %lld / %lld MB",(long long)(a/1024/1024),(long long)(b/1024/1024));});fprintf(stderr,"\nInstalled %s\n",vm.getInstallPath(v).toUtf8().constData());return 0;}catch(const std::exception &e){fprintf(stderr,"%s\n",e.what());return 1;}}return 1;}
    int installTest=cli.indexOf("--installtest");
    if(installTest>=0 && installTest+1<cli.size()){
        McInstanceManager manager(root);auto version=cli[installTest+1];auto loader=installTest+2<cli.size()?cli[installTest+2]:QString("vanilla");
        auto dir=manager.createInstance("Integration "+version+" "+loader,version,loader);
        auto java=qEnvironmentVariable("EBALIA_TEST_JAVA");if(!java.isEmpty()){auto info=ModRepository::read(dir+"/instance.json");info["javaPath"]=java;ModRepository::write(dir+"/instance.json",info);}
        if(qEnvironmentVariableIsSet("EBALIA_TEST_SOFTWARE_GL")){auto info=ModRepository::read(dir+"/instance.json");info["softwareRendering"]=true;ModRepository::write(dir+"/instance.json",info);}
        int last=-1;
        QObject::connect(&manager,&McInstanceManager::installProgress,&app,[&](const QString &,int pct,const QString &stage){if(last!=pct){fprintf(stderr,"%d%% %s\n",pct,stage.toUtf8().constData());last=pct;}});
        QObject::connect(&manager,&McInstanceManager::installDone,&app,[&](const QString &path,bool ok,const QString &error){fprintf(stderr,"DONE %s %s\n%s\n",ok?"OK":"FAILED",error.toUtf8().constData(),path.toUtf8().constData());app.exit(ok?0:1);});
        QTimer::singleShot(0,&manager,[&]{manager.installInstance(dir);});auto code=app.exec();QThreadPool::globalInstance()->waitForDone();return code;
    }
    int launchTest=cli.indexOf("--launchtest");
    if(launchTest>=0 && launchTest+1<cli.size()){
        McInstanceManager manager(root);auto dir=cli[launchTest+1];bool timed=false;
        QObject::connect(&manager,&McInstanceManager::launchFailed,&app,[&](const QString &,const QString &e){fprintf(stderr,"LAUNCH FAILED: %s\n",e.toUtf8().constData());app.exit(1);});
        QObject::connect(&manager,&McInstanceManager::gameStarted,&app,[&](const QString &){fprintf(stderr,"PROCESS STARTED\n");const int seconds=qEnvironmentVariableIntValue("EBALIA_LAUNCHTEST_SECONDS");QTimer::singleShot((seconds>0?seconds:30)*1000,&manager,[&]{timed=true;manager.killInstance(dir);});});
        QObject::connect(&manager,&McInstanceManager::gameEnded,&app,[&](const QString &,int code){fprintf(stderr,"PROCESS ENDED code=%d timed=%d\n",code,timed);app.exit(timed?0:1);});
        QTimer::singleShot(0,&manager,[&]{manager.launch(dir,"EbaliaTest","8e952cccdf9b33a495fe23c4bb5427a3",{},"legacy");});return app.exec();
    }
    ButtonTranslator buttons;app.installTranslator(&buttons);
    MainWindow window;window.show();
    QTimer::singleShot(45000,&window,[root]{LauncherUpdater::cleanupInstalled(root);});
    if(updateArg>=0) {
        if(!LauncherUpdater::activate(pending,root)){QMessageBox::warning(&window,"EBALIA",Language::text("No se pudo terminar la actualización. Se conservó la instalación anterior.","Could not finish the update. The previous installation has been retained.","Não foi possível concluir a atualização. A instalação anterior foi mantida."));}
        else {QTimer::singleShot(4000,&window,[root]{LauncherUpdater::cleanupInstalled(root);});QTimer::singleShot(30000,&window,[root]{LauncherUpdater::cleanupInstalled(root);});}
    }
    UpdateChecker updater(&window);QTimer updateTimer;updateTimer.setInterval(6*60*60*1000);
    QObject::connect(&updateTimer,&QTimer::timeout,&updater,&UpdateChecker::check);updateTimer.start();
    auto updateTask=new QWidget(&window);auto updateLayout=new QVBoxLayout(updateTask);updateLayout->setContentsMargins(8,4,8,4);
    auto updateText=new QLabel;updateText->setWordWrap(true);auto updateProgress=new QProgressBar;updateProgress->setRange(0,100);updateProgress->setFixedHeight(10);
    updateLayout->addWidget(updateText);updateLayout->addWidget(updateProgress);updateTask->setMaximumWidth(340);updateTask->hide();window.statusBar()->addPermanentWidget(updateTask);
    auto watcher=new QFutureWatcher<QJsonObject>(&window);
    auto installUpdate=[&] {
        if(window.property("launcherUpdating").toBool())return;
        if(!window.canUpdate()){QMessageBox::information(&window,"EBALIA",Language::text("Cerrá las partidas y esperá a que terminen las instalaciones antes de actualizar.","Close games and wait for installations to finish before updating.","Feche os jogos e aguarde o fim das instalações antes de atualizar."));return;}
        window.setProperty("launcherUpdating",true);window.centralWidget()->setEnabled(false);window.setLauncherUpdate(MainWindow::LauncherUpdate::Installing);updateTask->show();
        auto release=updater.release();
        watcher->setFuture(QtConcurrent::run([release,root,&window,updateText,updateProgress] {
            try {return LauncherUpdater::prepare(release,root,[&window,updateText,updateProgress](int percent,const QString &message){
                QMetaObject::invokeMethod(&window,[updateText,updateProgress,percent,message]{updateText->setText(Language::message(message));updateProgress->setRange(0,percent<0?0:100);if(percent>=0)updateProgress->setValue(percent);},Qt::QueuedConnection);
            });}catch(const std::exception &e){return QJsonObject{{"error",QString::fromUtf8(e.what())}};}
        }));
    };
    QObject::connect(watcher,&QFutureWatcher<QJsonObject>::finished,&window,[&] {
        const auto result=watcher->result();
        auto error=result["error"].toString();
        if(error.isEmpty()) {
            auto pendingFile=LauncherUpdater::writePending(result,root);
            if(!pendingFile.isEmpty()&&LauncherUpdater::launch(result,root,pendingFile)) {
                window.setProperty("launcherUpdating",false);window.close();return;
            }
            error=Language::text("No se pudo reiniciar con la versión nueva. Tu instalación actual no cambió.","Could not restart the new version. Your current installation is unchanged.","Não foi possível reiniciar com a nova versão. Sua instalação atual não mudou.");
        }
        window.setProperty("launcherUpdating",false);window.centralWidget()->setEnabled(true);window.setLauncherUpdate(MainWindow::LauncherUpdate::Available);updateTask->hide();
        QMessageBox::warning(&window,"EBALIA",Language::message(error));
    });
    QString promptedVersion;
    QObject::connect(&updater,&UpdateChecker::updateAvailable,&window,[&](const QString &version){
        window.setLauncherUpdate(MainWindow::LauncherUpdate::Available,version);
        if(promptedVersion!=version){promptedVersion=version;UpdateChecker::showUpdatePrompt(&window,version,installUpdate);}
    });
    QObject::connect(&updater,&UpdateChecker::upToDate,&window,[&]{window.setLauncherUpdate(MainWindow::LauncherUpdate::Current);});
    QObject::connect(&updater,&UpdateChecker::checkFailed,&window,[&]{window.setLauncherUpdate(MainWindow::LauncherUpdate::Failed);});
    QObject::connect(&window,&MainWindow::installLauncherUpdateRequested,&window,installUpdate);
    QTimer::singleShot(5000,&updater,&UpdateChecker::check);
    auto args=app.arguments();int test=args.indexOf("--selftest");
    if(test>=0){if(test+1<args.size())window.showPage(args[test+1].toInt());QTimer::singleShot(700,&window,[&]{auto path=qEnvironmentVariable("EBALIA_SCREENSHOT","/tmp/ebalia-main.png");window.grab().save(path);app.quit();});}
    int result=app.exec();QThreadPool::globalInstance()->waitForDone();return result;
}
