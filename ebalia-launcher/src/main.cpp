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
#include "JavaRunner.hpp"
#include "VersionManager.hpp"
#include <cstdio>
class ButtonTranslator:public QTranslator { public: QString translate(const char *,const char *s,const char *,int) const override {return Language::standard(QString::fromUtf8(s));} bool isEmpty() const override {return false;} };
int main(int argc,char **argv){
    QApplication::setOrganizationName("EBALIA");QApplication::setApplicationName("EBALIA Launcher");QApplication::setApplicationVersion(EBALIA_APP_VERSION);
    QApplication app(argc,argv);app.setStyle("Fusion");app.setWindowIcon(QIcon(":/icon.png"));
    app.setDesktopFileName("ebalia-launcher");
    auto root=qEnvironmentVariable("EBALIA_DATA_DIR");if(root.isEmpty())root=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(root);
    QLockFile lock(root+"/launcher.lock");if(!lock.tryLock()){QMessageBox::information(nullptr,"EBALIA","EBALIA is already running with this data folder.");return 1;}
    auto cli=app.arguments();if(cli.contains("--javatest")){for(auto j:JavaRuntime::discover())fprintf(stdout,"Java %s | %s | %s\n",j.version.toUtf8().constData(),j.architecture.toUtf8().constData(),j.path.toUtf8().constData());return 0;}
    int javaDownload=cli.indexOf("--javadownload");if(javaDownload>=0&&javaDownload+1<cli.size()){try{auto java=JavaDownloader::install(root+"/mc/java",JavaDownloader::component(cli[javaDownload+1].toInt()),{},[](int done,int total){fprintf(stderr,"\rJava %d / %d",done,total);});auto check=JavaRuntime::inspect(java);fprintf(stdout,"\nJava %s | %s | %s\n",check.version.toUtf8().constData(),check.architecture.toUtf8().constData(),java.toUtf8().constData());return check.major?0:1;}catch(const std::exception &e){fprintf(stderr,"%s\n",e.what());return 1;}}
    int lostLaunch=cli.indexOf("--lost-launchtest");if(lostLaunch>=0&&lostLaunch+1<cli.size()){
        VersionManager vm;vm.loadVersions();VersionInfo version;for(auto v:vm.getVersions())if(v.id==cli[lostLaunch+1])version=v;if(version.id.isEmpty()||!vm.isVersionInstalled(version)){fprintf(stderr,"Version not installed\n");return 1;}
        QString java;try{java=LostInstaller::java(root,LostInstaller::windowsPackage(version),[](int done,int total){fprintf(stderr,"\rJava %d / %d",done,total);});}catch(const std::exception &e){fprintf(stderr,"%s\n",e.what());return 1;}
        fprintf(stderr,"\nJava 8: %s\n",java.toUtf8().constData());JavaRunner runner;bool started=false;
        QObject::connect(&runner,&JavaRunner::processStarted,&app,[&]{started=true;fprintf(stderr,"PROCESS STARTED\n");QTimer::singleShot(40000,&runner,[&]{runner.stop();});});
        QObject::connect(&runner,&JavaRunner::processFinished,&app,[&](int code){fprintf(stderr,"PROCESS ENDED code=%d\n",code);app.exit(started?0:1);});
        QObject::connect(&runner,&JavaRunner::processError,&app,[&](const QString &e){fprintf(stderr,"ERROR: %s\n",e.toUtf8().constData());if(!started)app.exit(1);});
        QTimer::singleShot(0,&runner,[&]{runner.launch(version,vm.getInstallPath(version),java);});auto code=app.exec();
        QFile log(vm.getInstallPath(version)+"/launcher.log");if(log.open(QIODevice::ReadOnly))fprintf(stderr,"--- launcher.log ---\n%s\n",log.readAll().right(4000).constData());return code;
    }
    int lostTest=cli.indexOf("--lost-installtest");if(lostTest>=0&&lostTest+1<cli.size()){VersionManager vm;vm.loadVersions();for(auto v:vm.getVersions())if(v.id==cli[lostTest+1]){try{LostInstaller::install(v,vm.getInstallPath(v),[](qint64 a,qint64 b){if(b>0)fprintf(stderr,"\rLost archive %lld / %lld MB",(long long)(a/1024/1024),(long long)(b/1024/1024));});fprintf(stderr,"\nInstalled %s\n",vm.getInstallPath(v).toUtf8().constData());return 0;}catch(const std::exception &e){fprintf(stderr,"%s\n",e.what());return 1;}}return 1;}
    int installTest=cli.indexOf("--installtest");
    if(installTest>=0 && installTest+1<cli.size()){
        McInstanceManager manager(root);auto version=cli[installTest+1];auto loader=installTest+2<cli.size()?cli[installTest+2]:QString("vanilla");
        auto dir=manager.createInstance("Integration "+version+" "+loader,version,loader);
        auto java=qEnvironmentVariable("EBALIA_TEST_JAVA");if(!java.isEmpty()){auto info=ModRepository::read(dir+"/instance.json");info["javaPath"]=java;ModRepository::write(dir+"/instance.json",info);}
        int last=-1;
        QObject::connect(&manager,&McInstanceManager::installProgress,&app,[&](const QString &,int pct,const QString &stage){if(last!=pct){fprintf(stderr,"%d%% %s\n",pct,stage.toUtf8().constData());last=pct;}});
        QObject::connect(&manager,&McInstanceManager::installDone,&app,[&](const QString &path,bool ok,const QString &error){fprintf(stderr,"DONE %s %s\n%s\n",ok?"OK":"FAILED",error.toUtf8().constData(),path.toUtf8().constData());app.exit(ok?0:1);});
        QTimer::singleShot(0,&manager,[&]{manager.installInstance(dir);});auto code=app.exec();QThreadPool::globalInstance()->waitForDone();return code;
    }
    int launchTest=cli.indexOf("--launchtest");
    if(launchTest>=0 && launchTest+1<cli.size()){
        McInstanceManager manager(root);auto dir=cli[launchTest+1];bool timed=false;
        QObject::connect(&manager,&McInstanceManager::launchFailed,&app,[&](const QString &,const QString &e){fprintf(stderr,"LAUNCH FAILED: %s\n",e.toUtf8().constData());app.exit(1);});
        QObject::connect(&manager,&McInstanceManager::gameStarted,&app,[&](const QString &){fprintf(stderr,"PROCESS STARTED\n");QTimer::singleShot(30000,&manager,[&]{timed=true;manager.killInstance(dir);});});
        QObject::connect(&manager,&McInstanceManager::gameEnded,&app,[&](const QString &,int code){fprintf(stderr,"PROCESS ENDED code=%d timed=%d\n",code,timed);app.exit(timed?0:1);});
        QTimer::singleShot(0,&manager,[&]{manager.launch(dir,"EbaliaTest","8e952cccdf9b33a495fe23c4bb5427a3",{},"legacy");});return app.exec();
    }
    ButtonTranslator buttons;app.installTranslator(&buttons);
    MainWindow window;window.show();
    UpdateChecker updater(&window);QTimer updateTimer;updateTimer.setInterval(6*60*60*1000);
    QObject::connect(&updateTimer,&QTimer::timeout,&updater,&UpdateChecker::check);updateTimer.start();
    auto updateButton=new QPushButton(&window);updateButton->setObjectName("launcherUpdateAvailable");updateButton->hide();
    window.statusBar()->addPermanentWidget(updateButton);
    QObject::connect(&updater,&UpdateChecker::updateAvailable,&window,[&](const QString &version){updateButton->setText("Nueva versión "+version+" · Descargar ↗");updateButton->show();});
    QObject::connect(updateButton,&QPushButton::clicked,&window,[&]{QDesktopServices::openUrl(QUrl(updater.releaseUrl()));});
    QTimer::singleShot(5000,&updater,&UpdateChecker::check);
    auto args=app.arguments();int test=args.indexOf("--selftest");
    if(test>=0){if(test+1<args.size())window.showPage(args[test+1].toInt());QTimer::singleShot(700,&window,[&]{auto path=qEnvironmentVariable("EBALIA_SCREENSHOT","/tmp/ebalia-main.png");window.grab().save(path);app.quit();});}
    int result=app.exec();QThreadPool::globalInstance()->waitForDone();return result;
}
