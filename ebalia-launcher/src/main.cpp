#include <QApplication>
#include <QTimer>
#include <QIcon>
#include <QDir>
#include <QStandardPaths>
#include <QLockFile>
#include <QMessageBox>
#include <QThreadPool>
#include "MainWindow.hpp"
#include "Language.hpp"
#include <QTranslator>
#include <QJsonObject>
#include "ModRepository.hpp"
#include "JavaRuntime.hpp"
#include "LostInstaller.hpp"
#include "VersionManager.hpp"
#include <cstdio>
class ButtonTranslator:public QTranslator { public: QString translate(const char *,const char *s,const char *,int) const override {return Language::standard(QString::fromUtf8(s));} bool isEmpty() const override {return false;} };
int main(int argc,char **argv){
    QApplication::setOrganizationName("EBALIA");QApplication::setApplicationName("EBALIA Launcher");QApplication::setApplicationVersion("4.0.0");
    QApplication app(argc,argv);app.setStyle("Fusion");app.setWindowIcon(QIcon(":/icon.png"));
    auto root=qEnvironmentVariable("EBALIA_DATA_DIR");if(root.isEmpty())root=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);QDir().mkpath(root);
    QLockFile lock(root+"/launcher.lock");if(!lock.tryLock()){QMessageBox::information(nullptr,"EBALIA","EBALIA is already running with this data folder.");return 1;}
    auto cli=app.arguments();if(cli.contains("--javatest")){for(auto j:JavaRuntime::discover())fprintf(stdout,"Java %s | %s | %s\n",j.version.toUtf8().constData(),j.architecture.toUtf8().constData(),j.path.toUtf8().constData());return 0;}int lostTest=cli.indexOf("--lost-installtest");if(lostTest>=0&&lostTest+1<cli.size()){VersionManager vm;vm.loadVersions();for(auto v:vm.getVersions())if(v.id==cli[lostTest+1]){try{LostInstaller::install(v,vm.getInstallPath(v),[](qint64 a,qint64 b){if(b>0)fprintf(stderr,"\rLost archive %lld / %lld MB",(long long)(a/1024/1024),(long long)(b/1024/1024));});fprintf(stderr,"\nInstalled %s\n",vm.getInstallPath(v).toUtf8().constData());return 0;}catch(const std::exception &e){fprintf(stderr,"%s\n",e.what());return 1;}}return 1;}
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
    auto args=app.arguments();int test=args.indexOf("--selftest");
    if(test>=0){if(test+1<args.size())window.showPage(args[test+1].toInt());QTimer::singleShot(700,&window,[&]{auto path=qEnvironmentVariable("EBALIA_SCREENSHOT","/tmp/ebalia-main.png");window.grab().save(path);app.quit();});}
    int result=app.exec();QThreadPool::globalInstance()->waitForDone();return result;
}
