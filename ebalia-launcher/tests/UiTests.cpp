#include <QtTest>
#include <QtWidgets>
#include "MainWindow.hpp"
#include "VersionManager.hpp"
#include "Language.hpp"
#include "McInstanceManager.hpp"
#include "CreateInstanceDialog.hpp"
#include "ModRepository.hpp"
#include "SetupDialog.hpp"
#include "AccountManager.hpp"
#include "HomeBanner.hpp"
#include "CreatorDialog.hpp"
#include "PatreonAuth.hpp"
#include "UpdateChecker.hpp"
#include "PatreonNewsPage.hpp"
#include "Ui.hpp"
#include "BedrockPage.hpp"
#include "LauncherUpdater.hpp"
#include "MsAuth.hpp"
#include "InstanceIcons.hpp"
#include "InstanceViews.hpp"
#include "ServerList.hpp"
#include <QTcpSocket>
#include <QUrlQuery>
class UiTests:public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE void captureUrl(const QUrl &url){lastUrl=url;}
private:
    QUrl lastUrl;
private slots:
    void microsoftBrowserCallback() {
        const auto previous=qgetenv("EBALIA_MS_CLIENT_ID");
        const bool wasSet=qEnvironmentVariableIsSet("EBALIA_MS_CLIENT_ID");
        auto restore=qScopeGuard([&]{QDesktopServices::unsetUrlHandler("https");if(wasSet)qputenv("EBALIA_MS_CLIENT_ID",previous);else qunsetenv("EBALIA_MS_CLIENT_ID");});
        qputenv("EBALIA_MS_CLIENT_ID","00000000-0000-0000-0000-000000000001");
        QDesktopServices::setUrlHandler("https",this,"captureUrl");lastUrl=QUrl();
        MsAuth auth;QSignalSpy failed(&auth,&MsAuth::loginFailed);QSignalSpy done(&auth,&MsAuth::loginDone);
        auth.startLogin();
        QCOMPARE(lastUrl.host(),QString("login.microsoftonline.com"));
        QUrlQuery query(lastUrl);QCOMPARE(query.queryItemValue("code_challenge_method"),QString("S256"));
        QCOMPARE(query.queryItemValue("response_type"),QString("code"));
        QVERIFY(query.queryItemValue("state").size()>=32);QVERIFY(query.queryItemValue("code_challenge").size()>=43);
        const QUrl redirect(query.queryItemValue("redirect_uri"));QCOMPARE(redirect.host(),QString("localhost"));
        auto callback=[&](const QString &state){
            QTcpSocket socket;socket.connectToHost(QHostAddress::LocalHost,redirect.port());
            if(!socket.waitForConnected(2000))return QByteArray();
            socket.write("GET /?error=access_denied&state="+state.toUtf8()+" HTTP/1.1\r\nHost: localhost\r\n\r\n");socket.flush();
            QElapsedTimer timer;timer.start();QByteArray response;
            while(timer.elapsed()<2000&&!response.contains("\r\n\r\n")){QCoreApplication::processEvents();response+=socket.readAll();QTest::qWait(5);}
            return response;
        };
        QVERIFY(callback("wrong-state").startsWith("HTTP/1.1 400"));QCOMPARE(failed.count(),0);QCOMPARE(done.count(),0);
        QVERIFY(callback(query.queryItemValue("state")).startsWith("HTTP/1.1 200"));QCOMPARE(failed.count(),1);QCOMPARE(done.count(),0);
        auth.cancel();
    }
    void initTestCase(){QSettings().setValue("ui/tutorialSeen",true);QApplication::setStyle("Fusion");QCoreApplication::setOrganizationName("EBALIA-test");QCoreApplication::setApplicationName("UI-test");QSettings().setValue("ui/tutorialSeen",true);qputenv("EBALIA_NO_NETWORK","1");}

    void bedrockCommandValidation() {
        QTemporaryDir dir;
        QVERIFY(!Bedrock::localCommand("relative.exe",Bedrock::Platform::Windows).valid());
        QVERIFY(!Bedrock::localCommand(dir.path()+"/missing.exe",Bedrock::Platform::Windows).valid());
        const auto exe=dir.path()+"/manager name; literal.exe";
        QFile file(exe);QVERIFY(file.open(QIODevice::WriteOnly));file.write("test");file.close();
        auto windows=Bedrock::localCommand(exe,Bedrock::Platform::Windows);
        QCOMPARE(windows.program,exe);QVERIFY(windows.arguments.isEmpty());
        QVERIFY(!Bedrock::localCommand(exe,Bedrock::Platform::Unsupported).valid());
#ifndef Q_OS_WIN
        QVERIFY(!Bedrock::localCommand(exe,Bedrock::Platform::Linux).valid());
        QVERIFY(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        QCOMPARE(Bedrock::localCommand(exe,Bedrock::Platform::Linux).program,exe);
#endif
        const auto bundle=dir.path()+"/Bedrock Test.app";QVERIFY(QDir().mkpath(bundle));
        auto mac=Bedrock::localCommand(bundle,Bedrock::Platform::MacOS);
        QCOMPARE(mac.program,QString("/usr/bin/open"));QCOMPARE(mac.arguments,QStringList({"-a",bundle}));
        QVERIFY(!Bedrock::localCommand(bundle,Bedrock::Platform::Linux).valid());
    }
    void bedrockMissingProviderNeverEnablesLaunch() {
        QTemporaryDir dir;
        QSettings settings(dir.path()+"/bedrock-lab.ini",QSettings::IniFormat);
        settings.setValue("provider/path",dir.path()+"/nonexistent");settings.sync();
        BedrockPage page(dir.path());page.resize(600,650);page.show();
        auto launch=page.findChild<QPushButton*>("bedrockOpenManager");QVERIFY(launch);
        QTest::qWait(50);QVERIFY(!launch->isEnabled());
        QVERIFY(!page.findChild<QLabel*>("bedrockProviderStatus")->text().isEmpty());
    }
    void bedrockLaunchPreservesLiteralPath() {
#ifndef Q_OS_UNIX
        QSKIP("Uses a POSIX test helper; Windows argv is covered by command validation.");
#else
        QTemporaryDir dir;
        const auto exe=dir.path()+"/bedrock helper; literal";
        QFile helper(exe);QVERIFY(helper.open(QIODevice::WriteOnly));
        helper.write(R"SCRIPT(#!/bin/sh
printf started > launched-marker
)SCRIPT");helper.close();
        QVERIFY(helper.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
        QSettings settings(dir.path()+"/bedrock-lab.ini",QSettings::IniFormat);
        settings.setValue("provider/path",exe);settings.sync();
        BedrockPage page(dir.path());page.show();
        auto launch=page.findChild<QPushButton*>("bedrockOpenManager");
        QTRY_VERIFY(launch->isEnabled());launch->click();
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(dir.path()+"/launched-marker"),3000);
        auto path=page.findChild<QLineEdit*>("bedrockProviderPath");
        path->setFocus();QTest::keyClicks(path,"invalid");QVERIFY(!launch->isEnabled());
#endif
    }
    void lostUninstallCancelPreservesFiles() {
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());
        VersionManager versions;versions.loadVersions();QVERIFY(!versions.getVersions().isEmpty());
        for(const auto &v:versions.getVersions()) {
            auto dir=versions.getInstallPath(v);QDir().mkpath(dir);
            QFile marker(dir+"/.installed.json");QVERIFY(marker.open(QIODevice::WriteOnly));marker.write("{}");marker.close();
        }
        MainWindow window;window.show();window.showPage(MainWindow::Lost);
        auto uninstall=window.findChild<QPushButton*>("lostUninstall");QVERIFY(uninstall);QVERIFY(uninstall->isVisible());QVERIFY(uninstall->isEnabled());
        QTimer::singleShot(0,[] {if(auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->done(QMessageBox::No);});
        uninstall->click();
        for(const auto &v:versions.getVersions())QVERIFY(versions.isVersionInstalled(v));
        QVERIFY(!window.findChild<QWidget*>("backgroundTaskProgress"));
    }
    void asynchronousArtwork(){
        const QString path=":/art/backgrounds/village.png";const QSize bounds(137,137);QObject receiver;auto removed=new QObject;
        int delivered=0;QPixmap first,second;bool stale=false;
        Ui::loadArt(path,bounds,removed,[&](const QPixmap &){stale=true;});delete removed;
        Ui::loadArt(path,bounds,&receiver,[&](const QPixmap &p){first=p;++delivered;});Ui::loadArt(path,bounds,&receiver,[&](const QPixmap &p){second=p;++delivered;});
        QCOMPARE(delivered,0);QTRY_COMPARE_WITH_TIMEOUT(delivered,2,5000);QVERIFY(!stale);QVERIFY(!first.isNull());QCOMPARE(first.cacheKey(),second.cacheKey());QVERIFY(first.width()<=137&&first.height()<=137);
        Ui::loadArt(path,bounds,&receiver,[&](const QPixmap &p){QCOMPARE(p.cacheKey(),first.cacheKey());++delivered;});QCOMPARE(delivered,3);
    }
    void creatorPopupsAndPatreonSections(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());qputenv("EBALIA_LANGUAGE","es");MainWindow window;window.resize(900,600);window.show();window.showPage(MainWindow::Home);
        auto mods=window.findChild<QPushButton*>("myModsButton");QVERIFY(mods);QTest::mouseClick(mods,Qt::LeftButton);auto dialog=window.findChild<QDialog*>("myModsDialog");QVERIFY(dialog&&dialog->isVisible());
        auto links=dialog->findChildren<QPushButton*>("creatorModLink");QCOMPARE(links.size(),3);QDesktopServices::setUrlHandler("https",this,"captureUrl");
        const QStringList slugs{"ooo-jar","in-your-world","secret-01-proyect"};for(int i=0;i<3;++i){QTest::mouseClick(links[i],Qt::LeftButton);QCOMPARE(lastUrl,QUrl("https://www.curseforge.com/minecraft/mc-mods/"+slugs[i]));QCOMPARE(links[i]->property("soon").toBool(),i==2);}
        dialog->close();QTest::mouseClick(window.findChild<QPushButton*>("patreonInviteButton"),Qt::LeftButton);auto invite=window.findChild<QDialog*>("patreonInvite");QVERIFY(invite&&invite->isVisible());QTest::mouseClick(invite->findChild<QPushButton*>("joinPatreon"),Qt::LeftButton);QCOMPARE(lastUrl,QUrl("https://www.patreon.com/EBALIA"));
        auto hide=invite->findChild<QCheckBox*>("hidePatreonInvite");hide->setChecked(true);QVERIFY(QSettings().value("ui/hidePatreonInvite").toBool());QSettings().remove("ui/hidePatreonInvite");invite->close();QDesktopServices::unsetUrlHandler("https");
        window.showPage(MainWindow::News);auto news=window.findChild<QTabWidget*>("newsTabs");QVERIFY(news);QCOMPARE(news->count(),2);news->setCurrentIndex(1);auto sections=window.findChild<QTabWidget*>("patreonNewsTabs");QVERIFY(sections);QCOMPARE(sections->count(),2);
        auto paid=window.findChild<QScrollArea*>("patreonPaidPosts");QVERIFY(paid);QVERIFY(paid->findChildren<QPushButton*>("patreonReadPost").isEmpty());QVERIFY(!window.findChild<PatreonAuth*>()->paidMember());
    }
    void simulatedPatreonMembershipAndCards(){
        PatreonAuth auth;auth.m_verified=true;auth.m_paid=true;auth.m_name="SIMULACIÓN · Miembro de prueba";auth.m_title="Gold";
        auth.m_status="SIMULACIÓN LOCAL · No es una suscripción real";
        QJsonObject post{{"title","Actualización exclusiva · Nuevo mundo"},{"date","2026-10-03"},{"is_public",false},{"url","https://www.patreon.com/EBALIA"},{"content","<p>Una aventura para los miembros de EBALIA.</p><p><b>Novedades:</b> nuevos biomas, criaturas y mejoras.</p><p><a href='https://www.curseforge.com/minecraft/mc-mods/in-your-world'>Ver el mod en CurseForge ↗</a></p><img src='https://c10.patreonusercontent.com/test-preview.png'><p>Gracias por acompañar el desarrollo.</p>"}};
        auth.m_posts=QJsonArray{post};PatreonNewsPage page(&auth);page.setStyleSheet(Ui::styleSheet());page.resize(800,700);page.show();page.findChild<QTabWidget*>("patreonNewsTabs")->setCurrentIndex(1);
        auto paid=page.findChild<QScrollArea*>("patreonPaidPosts");QVERIFY(auth.paidMember());QCOMPARE(paid->findChildren<QFrame*>("patreonPostCard").size(),1);
        auto body=paid->findChild<QTextBrowser*>("patreonPostBody");QVERIFY(body);QImage art(800,360,QImage::Format_RGB32);art.fill(QColor("#427659"));QPainter painter(&art);painter.setPen(Qt::white);painter.setFont(QFont("Sans",28,QFont::Bold));painter.drawText(art.rect(),Qt::AlignCenter,"EBALIA · VISTA PREVIA");painter.end();body->document()->addResource(QTextDocument::ImageResource,QUrl("https://c10.patreonusercontent.com/test-preview.png"),art);
        for(int width:{640,1280}){page.resize(width,720);QTest::qWait(60);QCOMPARE(paid->horizontalScrollBar()->maximum(),0);QCOMPARE(body->horizontalScrollBar()->maximum(),0);auto dir=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");if(!dir.isEmpty()){QDir().mkpath(dir);QVERIFY(page.grab().save(dir+"/patreon-subscriber-"+QString::number(width)+".png"));}}
        QDesktopServices::setUrlHandler("https",this,"captureUrl");QTest::mouseClick(paid->findChild<QPushButton*>("patreonReadPost"),Qt::LeftButton);QCOMPARE(lastUrl,QUrl("https://www.patreon.com/EBALIA"));QDesktopServices::unsetUrlHandler("https");
        auth.m_paid=false;emit auth.changed();QVERIFY(!auth.paidMember());QVERIFY(paid->findChildren<QFrame*>("patreonPostCard").isEmpty());
        auth.m_paid=true;auth.m_verified=false;emit auth.changed();QVERIFY(!auth.paidMember());QVERIFY(paid->findChildren<QFrame*>("patreonPostCard").isEmpty());
    }
    void patreonTiersRefreshWithoutRestart(){
        PatreonAuth auth;PatreonNewsPage page(&auth);page.show();
        auto label=page.findChild<QLabel*>("patreonAvailableTiers");QVERIFY(label);
        for(const QString &name:{QString("Exclusive Chad"),QString("Explicit Chad"),QString("SENIOR CHAD"),QString("Nuevo nivel creado después")}){
            auth.m_availableTiers.append(QJsonObject{{"id",QString::number(auth.m_availableTiers.size()+1)},{"title",name},{"amount_cents",250}});
            auth.m_verified=true;auth.m_paid=true;auth.m_title=name;auth.m_tierIds={QString::number(auth.m_availableTiers.size())};emit auth.changed();
            QVERIFY(label->text().contains(name));QVERIFY(page.findChild<QLabel*>("patreonMembership")->text().contains(name));QCOMPARE(auth.tierIds().size(),1);
        }
        auth.m_paid=false;emit auth.changed();QVERIFY(auth.tierIds().isEmpty());
    }
    void firstRunTourAndReplay(){
        QSettings().setValue("ui/tutorialSeen",false);QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());
        MainWindow window;window.resize(640,480);window.show();
        QTRY_VERIFY(window.findChild<QWidget*>("interactiveTour"));
        auto tour=window.findChild<QWidget*>("interactiveTour");auto next=tour->findChild<QPushButton*>("tourNext");
        QVERIFY(next);QVERIFY(!tour->findChild<QPushButton*>("tourBack")->isEnabled());
        for(int i=0;i<13;++i){next->click();QTest::qWait(10);QVERIFY(tour->rect().contains(tour->findChild<QFrame*>("tourCard")->geometry()));}
        next->click();QTRY_VERIFY(!window.findChild<QWidget*>("interactiveTour"));QVERIFY(QSettings().value("ui/tutorialSeen").toBool());
        window.showPage(MainWindow::Guide);window.findChild<QPushButton*>("startInteractiveTour")->click();
        QTRY_VERIFY(window.findChild<QWidget*>("interactiveTour"));window.findChild<QPushButton*>("tourSkip")->click();
        QTRY_VERIFY(!window.findChild<QWidget*>("interactiveTour"));window.close();
        MainWindow second;second.show();QTest::qWait(450);QVERIFY(!second.findChild<QWidget*>("interactiveTour"));
    }
    void updatePackageValidation(){
        QVERIFY(LauncherUpdater::trustedAsset("https://github.com/ebalia-real/launcher/releases/download/v1.2.0/ebalia-windows-x64.zip","v1.2.0"));
        for(const QString &url:{QString("https://evil.test/ebalia-real/launcher/releases/download/v1.2.0/x.zip"),QString("https://github.com/other/launcher/releases/download/v1.2.0/x.zip"),QString("https://github.com/ebalia-real/launcher/releases/download/v1.1.0/x.zip"),QString("https://github.com/ebalia-real/launcher/releases/download/v1.2.0/../x.zip")})QVERIFY(!LauncherUpdater::trustedAsset(url,"v1.2.0"));
        const auto hash=QByteArray(64,'a');
        QCOMPARE(LauncherUpdater::checksum(hash+"  file.zip\n","file.zip"),hash);
        QVERIFY_EXCEPTION_THROWN(LauncherUpdater::checksum(hash+"  wrong.zip\n","file.zip"),std::runtime_error);
        QVERIFY_EXCEPTION_THROWN(LauncherUpdater::checksum(hash+"  file.zip\n"+hash+"  file.zip\n","file.zip"),std::runtime_error);
        QCOMPARE(LauncherUpdater::assetName("linux","x86_64"),QString("ebalia-linux-x64-update.zip"));
        QVERIFY(LauncherUpdater::assetName("windows","arm64").isEmpty());
        QCOMPARE(LauncherUpdater::assetName("appimage","x86_64"),QString("ebalia-linux-x86_64.AppImage"));
        QCOMPARE(LauncherUpdater::assetName("macos","x86_64"),QString("ebalia-macos-x64.dmg"));
    }
    void updateReplacementAndCleanup(){
#ifdef Q_OS_MACOS
        QSKIP("The macOS updater replaces complete app bundles; this fixture covers file-manifest packages.");
#else
        QTemporaryDir temp;const auto root=temp.path()+"/data",destination=temp.path()+"/installed";
        const auto stage=root+"/updates/1.2.0-11111111-1111-1111-1111-111111111111";
#ifdef Q_OS_WIN
        const QString relative="ebalia-launcher.exe";
#else
        const QString relative="bin/ebalia-launcher";
#endif
        auto write=[](const QString &path,const QByteArray &bytes){QDir().mkpath(QFileInfo(path).absolutePath());QFile file(path);if(!file.open(QIODevice::WriteOnly))return false;return file.write(bytes)==bytes.size();};
        auto manifest=[](const QStringList &files){return QJsonDocument(QJsonObject{{"format",1},{"files",QJsonArray::fromStringList(files)}}).toJson();};
        QVERIFY(write(destination+"/"+relative,"old binary"));
        QVERIFY(write(destination+"/old-library.dll","old library"));
        QVERIFY(write(destination+"/worlds/keep.txt","my world"));
        QVERIFY(write(destination+"/update-files.json",manifest({relative,"old-library.dll","update-files.json"})));
        const auto source=stage+"/app";QDir().mkpath(QFileInfo(source+"/"+relative).absolutePath());
        QVERIFY(QFile::copy(QString::fromUtf8(EBALIA_UPDATE_HELPER),source+"/"+relative));
        QVERIFY(write(source+"/update-files.json",manifest({relative,"update-files.json"})));
        QJsonObject prepared{{"version","1.2.0"},{"stage",stage},{"destination",destination},{"executable",source+"/"+relative},{"nix",false}};
        QVERIFY(LauncherUpdater::applyPrepared(prepared,root));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(root+"/update-child-started"),5000);
        QVERIFY(!QFileInfo::exists(destination+"/old-library.dll"));
        QFile world(destination+"/worlds/keep.txt");QVERIFY(world.open(QIODevice::ReadOnly));QCOMPARE(world.readAll(),QByteArray("my world"));
        QVERIFY(QFileInfo::exists(stage+"/previous/old-library.dll"));
        QVERIFY(write(root+"/updates/cleanup.json",QJsonDocument(QJsonObject{{"version",QCoreApplication::applicationVersion()},{"folders",QJsonArray{QFileInfo(stage).fileName()}}}).toJson()));
        LauncherUpdater::cleanupInstalled(root);
        QVERIFY(!QFileInfo::exists(stage));QVERIFY(QFileInfo::exists(destination+"/"+relative));QVERIFY(QFileInfo::exists(destination+"/worlds/keep.txt"));
#endif
    }
    void updateFailureRestoresPreviousFiles(){
#ifdef Q_OS_MACOS
        QSKIP("Manifest replacement fixture applies to Linux and Windows.");
#else
        QTemporaryDir temp;const auto root=temp.path()+"/data",destination=temp.path()+"/installed";
        const auto stage=root+"/updates/1.2.0-22222222-2222-2222-2222-222222222222";
#ifdef Q_OS_WIN
        const QString relative="ebalia-launcher.exe";
#else
        const QString relative="bin/ebalia-launcher";
#endif
        auto write=[](const QString &path,const QByteArray &bytes){QDir().mkpath(QFileInfo(path).absolutePath());QFile file(path);if(!file.open(QIODevice::WriteOnly))return false;return file.write(bytes)==bytes.size();};
        auto manifest=[](const QStringList &files){return QJsonDocument(QJsonObject{{"format",1},{"files",QJsonArray::fromStringList(files)}}).toJson();};
        QVERIFY(write(destination+"/"+relative,"old binary"));
        QVERIFY(write(destination+"/update-files.json",manifest({relative,"update-files.json"})));
        QVERIFY(write(stage+"/app/update-files.json",manifest({relative,"update-files.json"})));
        QJsonObject prepared{{"version","1.2.0"},{"stage",stage},{"destination",destination},{"executable",stage+"/app/"+relative},{"nix",false}};
        QVERIFY_EXCEPTION_THROWN(LauncherUpdater::applyPrepared(prepared,root),std::runtime_error);
        QFile old(destination+"/"+relative);QVERIFY(old.open(QIODevice::ReadOnly));QCOMPARE(old.readAll(),QByteArray("old binary"));
        QVERIFY(QFileInfo::exists(destination+"/update-files.json"));old.close();
        QDir().mkpath(QFileInfo(stage+"/app/"+relative).absolutePath());
        QVERIFY(QFile::copy(QString::fromUtf8(EBALIA_UPDATE_HELPER),stage+"/app/"+relative));
        prepared["version"]="1.3.0"; // helper reports 1.2.0: startup verification must roll back.
        QVERIFY_EXCEPTION_THROWN(LauncherUpdater::applyPrepared(prepared,root),std::runtime_error);
        QVERIFY(old.open(QIODevice::ReadOnly));QCOMPARE(old.readAll(),QByteArray("old binary"));
#endif
    }
    void updatePromptCanBeDeferred(){
        QWidget parent;
        auto prompt=UpdateChecker::showUpdatePrompt(&parent,"1.2.0");
        QVERIFY(prompt->isVisible());QVERIFY(!prompt->isModal());QVERIFY(prompt->text().contains("1.2.0"));
        QVERIFY(prompt->escapeButton());QTest::mouseClick(prompt->escapeButton(),Qt::LeftButton);
        QVERIFY(!prompt->isVisible());
    }
    void launcherUpdateButton(){
        // The sidebar offers the update only while one is available.
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());auto restore=qScopeGuard([]{qunsetenv("EBALIA_DATA_DIR");});
        MainWindow window;window.resize(1280,820);window.show();
        auto sidebar=window.findChild<QPushButton*>("launcherUpdateAvailable");QVERIFY(sidebar);QSignalSpy install(&window,&MainWindow::installLauncherUpdateRequested);
        QVERIFY(!sidebar->isVisible());
        for(auto state:{MainWindow::LauncherUpdate::Checking,MainWindow::LauncherUpdate::Current,MainWindow::LauncherUpdate::Failed}){window.setLauncherUpdate(state);QVERIFY(!sidebar->isVisible());}
        window.setLauncherUpdate(MainWindow::LauncherUpdate::Available,"9.9.9");QVERIFY(sidebar->isVisible());QVERIFY(sidebar->text().contains("9.9.9"));
        if(auto out=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");!out.isEmpty()){QDir().mkpath(out);window.grab().save(out+"/update-available.png");}
        QTest::mouseClick(sidebar,Qt::LeftButton);QCOMPARE(install.size(),1);
        window.setLauncherUpdate(MainWindow::LauncherUpdate::Installing);QVERIFY(sidebar->isVisible());QVERIFY(!sidebar->isEnabled());
        window.setLauncherUpdate(MainWindow::LauncherUpdate::Available);QVERIFY(sidebar->isEnabled()); // a failed update can be retried
        window.resize(900,700);QTest::qWait(30);QVERIFY(sidebar->isVisible());QVERIFY(sidebar->text().isEmpty()); // compact sidebar: icon only
    }
    void instanceBackgrounds(){
        // A launcher picture, a picture from the computer, or the automatic one; anything else falls back to automatic.
        QTemporaryDir t;McInstanceManager manager(t.path());auto dir=manager.createInstance("Art","1.21.1","vanilla");
        const auto automatic=Ui::artFor(dir);QCOMPARE(InstanceIcons::background(dir),automatic);
        QVERIFY(InstanceIcons::backgrounds().size()>=20);for(const auto &art:InstanceIcons::backgrounds())QVERIFY2(!QImage(art).isNull(),qPrintable(art));
        auto set=[&](const QString &key){auto info=ModRepository::read(dir+"/instance.json");info["background"]=key;ModRepository::write(dir+"/instance.json",info);};
        set(":/art/backgrounds/nether.png");QCOMPARE(InstanceIcons::background(dir),QString(":/art/backgrounds/nether.png"));
        set(":/art/../icon.png");QCOMPARE(InstanceIcons::background(dir),automatic);
        set("custom");QCOMPARE(InstanceIcons::background(dir),automatic); // no picture saved yet
        QImage picture(320,180,QImage::Format_RGB32);picture.fill(Qt::red);QVERIFY(InstanceIcons::saveBackground(dir,picture));
        QCOMPARE(InstanceIcons::background(dir),dir+"/instance-background.jpg");QCOMPARE(QImage(dir+"/instance-background.jpg").size(),QSize(320,180));
        // The picker lists every picture and returns the one chosen.
        QString key;QImage custom;
        QTimer::singleShot(1500,[&]{QDialog *d=nullptr;for(auto w:QApplication::topLevelWidgets())if(w->objectName()=="instanceBackgroundWindow"&&w->isVisible())d=qobject_cast<QDialog*>(w);if(!d){QFAIL("picker not shown");return;}auto list=d->findChild<QListWidget*>("backgroundList");QCOMPARE(list->count(),InstanceIcons::backgrounds().size()+2);
            if(auto out=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");!out.isEmpty()){QDir().mkpath(out);d->grab().save(out+"/background-picker.png");}
            for(int n=0;n<list->count();++n)if(list->item(n)->data(Qt::UserRole).toString()==":/art/backgrounds/warden.png")list->setCurrentRow(n);d->accept();});
        QVERIFY(InstanceIcons::chooseBackground(nullptr,dir,"custom",key,custom));QCOMPARE(key,QString(":/art/backgrounds/warden.png"));QVERIFY(custom.isNull());
    }
    void crashSummaryWindow(){
        // A friend's real Forge 26.3 crash: a missing dependency and an AMD driver crash, explained with a button each.
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());auto restore=qScopeGuard([]{qunsetenv("EBALIA_DATA_DIR");});
        MainWindow window;window.resize(1280,820);window.show();auto manager=window.findChild<McInstanceManager*>();
        auto dir=manager->createInstance("Biomes","26.3","forge","66.0.9");
        {QFile f(dir+"/launcher.log");QVERIFY(f.open(QIODevice::WriteOnly));f.write("Missing or unsupported mandatory dependencies:\n\tMod ID: 'terrablender', Requested by: 'biomesoplenty', Expected range: '[26.3.0.0.6,)', Actual version: '[MISSING]'\n#\n# Problematic frame:\n# C  [atio6axx.dll+0x192b60]\n");}
        bool shown=false;
        QTimer::singleShot(800,&window,[&]{QDialog *d=nullptr;for(auto w:QApplication::topLevelWidgets())if(w->objectName()=="crashSummaryWindow"&&w->isVisible())d=qobject_cast<QDialog*>(w);
            if(!d)return;shown=true;QCOMPARE(d->findChildren<QFrame*>("crashFinding").size(),2);QVERIFY(d->findChild<QPushButton*>("installMissingMod"));
            QStringList texts;for(auto l:d->findChildren<QLabel*>())texts<<l->text();const auto all=texts.join("\n");QVERIFY(all.contains("terrablender"));QVERIFY(all.contains("biomesoplenty"));QVERIFY(all.contains("AMD"));
            if(auto out=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");!out.isEmpty()){QDir().mkpath(out);d->grab().save(out+"/crash-summary.png");}
            d->reject();});
        emit manager->gameEnded(dir,1);QTRY_VERIFY_WITH_TIMEOUT(shown,5000);
    }
    void pastedInstanceAppearsWhileOpen(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());auto restore=qScopeGuard([]{qunsetenv("EBALIA_DATA_DIR");});
        MainWindow window;window.resize(1280,820);window.show();auto manager=window.findChild<McInstanceManager*>();QTest::qWait(200);
        const auto prism=manager->instancesRoot()+"/Copied From Prism";QDir().mkpath(prism+"/minecraft/mods");
        {QFile f(prism+"/mmc-pack.json");QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(QJsonObject{{"formatVersion",1},{"components",QJsonArray{QJsonObject{{"uid","net.minecraft"},{"version","1.21.1"}},QJsonObject{{"uid","net.fabricmc.fabric-loader"},{"version","0.16.7"}}}}}).toJson());}
        {QFile f(prism+"/instance.cfg");QVERIFY(f.open(QIODevice::WriteOnly));f.write("[General]\nname=Copied From Prism\n");}
        {QFile f(prism+"/minecraft/mods/a.jar");QVERIFY(f.open(QIODevice::WriteOnly));f.write("mod");}
        QTRY_VERIFY_WITH_TIMEOUT([&]{for(const auto &i:manager->instances())if(i.name=="Copied From Prism"&&i.loader=="fabric"&&i.mcVersion=="1.21.1")return true;return false;}(),20000);
        QVERIFY(QFile::exists(manager->instancesRoot()+"/Copied From Prism/mods/a.jar"));
    }
    void instanceIconsFromGameTextures(){
        // Opt-in: EBALIA_TEST_JARS=<client jars separated by ;> (Minecraft is not part of the repository).
        const auto jars=qEnvironmentVariable("EBALIA_TEST_JARS").split(';',Qt::SkipEmptyParts);if(jars.isEmpty())QSKIP("Set EBALIA_TEST_JARS to Minecraft client jars");
        QImage sheet(16*13*4,int(jars.size())*16*4+8,QImage::Format_ARGB32);sheet.fill(QColor(24,24,28));QPainter p(&sheet);
        for(int row=0;row<jars.size();++row){QTemporaryDir t;QVERIFY2(InstanceIcons::extractGameTextures(jars[row],t.path()),qPrintable(jars[row]));int col=0;
            for(const auto &key:InstanceIcons::keys())if(key!="ebalia"){QImage icon(t.path()+"/"+key+".png");QVERIFY2(!icon.isNull()&&icon.width()==icon.height(),qPrintable(key));p.drawImage(QRect(col++*72+4,row*68+4,64,64),icon);}}
        p.end();if(auto out=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");!out.isEmpty()){QDir().mkpath(out);sheet.save(out+"/instance-icons.png");}
    }
    void patreonPostsLookTidy(){
        // A public post as Patreon's editor writes it: small caps, font sizes, empty paragraphs and long text.
        PatreonAuth auth;auth.m_status="Updated · 12:16:28";
        QString body="<p><span style='font-size:18px'>English:</span></p><p>If you are a paying member, tell me and you will have an exclusive role.</p><p><br></p><p><br></p>"
            "<p><span style='font-variant:small-caps;font-family:Georgia'>Unete a mi server de Discord para descargar el mod y se parte de la comunidad!</span></p>"
            "<p><font size='5' color='red'>HERE</font> / <a href='https://discord.gg/x'>AQUI</a> (PERMANENT LINK)</p>";
        for(int n=0;n<6;++n)body+="<p>Línea "+QString::number(n+1)+" de una publicación larga para comprobar que se recorta con Ver más.</p>";
        auth.m_posts=QJsonArray{QJsonObject{{"title","NEW EBALIA LAUNCHER - COMING SOON"},{"date","2024-08-10"},{"is_public",true},{"url","https://www.patreon.com/EBALIA"},{"content",body}},
                                QJsonObject{{"title","Devlog #3"},{"date","2024-07-01"},{"is_public",true},{"url","https://www.patreon.com/EBALIA"},{"content","<p>Short update.</p>"}}};
        PatreonNewsPage page(&auth);page.setStyleSheet(Ui::styleSheet());page.resize(900,760);page.show();QTest::qWait(100);
        auto cards=page.findChild<QScrollArea*>("patreonPublicPosts")->findChildren<QFrame*>("patreonPostCard");QCOMPARE(cards.size(),2);
        auto text=cards[0]->findChild<QTextBrowser*>("patreonPostBody");QVERIFY(!text->toHtml().contains("small-caps"));QVERIFY(!text->toHtml().contains("Georgia"));
        auto more=cards[0]->findChild<QPushButton*>("patreonShowMore");QVERIFY(more->isVisible());const int closed=text->height();QTest::mouseClick(more,Qt::LeftButton);QVERIFY(text->height()>closed);
        QVERIFY(!cards[1]->findChild<QPushButton*>("patreonShowMore")->isVisible());
        QTest::mouseClick(more,Qt::LeftButton);
        if(auto out=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");!out.isEmpty()){QDir().mkpath(out);page.grab().save(out+"/patreon-page.png");}
    }
    void guideQuestionsAndAnswers(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());auto restore=qScopeGuard([]{qunsetenv("EBALIA_DATA_DIR");});
        MainWindow window;window.resize(1280,900);window.show();window.showPage(MainWindow::Guide);QTest::qWait(100);
        auto questions=window.findChildren<QPushButton*>("faqQuestion");QVERIFY(questions.size()>=10);auto answers=window.findChildren<QLabel*>("faqAnswer");
        for(auto a:answers)QVERIFY(!a->isVisible());
        QTest::mouseClick(questions[0],Qt::LeftButton);QVERIFY(answers[0]->isVisible());QVERIFY(answers[0]->text().contains(".mrpack"));
        QTest::mouseClick(questions[1],Qt::LeftButton);QTest::qWait(50);
        if(auto out=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");!out.isEmpty()){QDir().mkpath(out);auto page=questions[0]->window()->findChild<QWidget*>("guide")->parentWidget();page->grab().save(out+"/guide-faq.png");
            for(auto card:window.findChildren<QFrame*>("modCard")){}window.showPage(MainWindow::Home);QTest::qWait(400);window.grab().save(out+"/home-mods.png");}
        QTest::mouseClick(questions[0],Qt::LeftButton);QVERIFY(!answers[0]->isVisible());
    }
    void colourThemes(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());auto restore=qScopeGuard([]{qunsetenv("EBALIA_DATA_DIR");Ui::setTheme("ebalia");qApp->setStyleSheet(Ui::styleSheet());});
        QCOMPARE(Ui::themed(QColor("#3c8527")),QColor("#3c8527"));QCOMPARE(Ui::styleSheet().contains("#3c8527"),true);
        for(const auto &t:Ui::themes()){
            Ui::setTheme(t.id);QCOMPARE(Ui::theme(),t.id);const auto sheet=Ui::styleSheet();
            if(t.id!="ebalia"){QVERIFY2(!sheet.contains("#3c8527")&&!sheet.contains("#4a9e31"),qPrintable(t.id));QCOMPARE(Ui::themed(QColor("#ed6957")),QColor("#ed6957"));QCOMPARE(Ui::themed(Qt::white),QColor(Qt::white));}
            if(auto out=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");!out.isEmpty()){qApp->setStyleSheet(sheet);MainWindow window;window.resize(1280,820);window.show();window.showPage(MainWindow::Settings);QTest::qWait(150);window.grab().save(out+"/theme-"+t.id+".png");}
        }
        Ui::setTheme("unknown");QCOMPARE(Ui::theme(),QString("ebalia"));
    }
    void serverListEditing(){
        QTemporaryDir t;const auto file=t.path()+"/servers.dat";
        ServerList::add(file,"§6Hypixel §cNetwork","mc.hypixel.net");ServerList::add(file,"Local","localhost");
        ServerList::edit(file,0,"Hypixel","hypixel.net");auto list=ServerList::read(file);QCOMPARE(list.size(),2);QCOMPARE(list[0].name,QString("Hypixel"));QCOMPARE(list[0].address,QString("hypixel.net"));QCOMPARE(list[1].address,QString("localhost"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error,ServerList::edit(file,5,"x","y"));
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());auto restore=qScopeGuard([]{qunsetenv("EBALIA_DATA_DIR");});
        McInstanceManager manager(data.path());auto dir=manager.createInstance("Servers","1.21.1","vanilla");ServerList::add(dir+"/servers.dat","§aGreen §lBold","play.example.org");
        InstanceDetail detail;detail.resize(1280,900);detail.show();InstanceInfo info;info.base=McInstance{"Servers",dir,"1.21.1","vanilla",true,4096,0,0};detail.showInstance(info);
        auto servers=detail.findChild<QListWidget*>("instanceServers");QCOMPARE(servers->count(),1);QVERIFY(servers->item(0)->text().startsWith("Green Bold\n"));
        QVERIFY(detail.findChild<QPushButton*>("editServer"));
    }
    void releaseDetection(){
        QVERIFY(!UpdateChecker::releasePage(QJsonObject{{"html_url","https://github.com/ebalia-real/launcher/releases/tag/v1.1.0"}},false).isEmpty());
        QVERIFY(UpdateChecker::releasePage(QJsonObject{{"html_url","https://github.com/other/launcher/releases/tag/v1.1.0"}},false).isEmpty());
        QVERIFY(UpdateChecker::newerStable("v4.0.1","4.0.0"));QVERIFY(!UpdateChecker::newerStable("v4.0.1","v4.1.0"));QVERIFY(UpdateChecker::newerStable("4.10.0","4.9.0"));
        for(const QString &tag:{QString("v4.0.0"),QString("3.9.0"),QString("v5.0.0-beta"),QString("latest")})QVERIFY(!UpdateChecker::newerStable(tag,"4.0.0"));
        QVERIFY(!UpdateChecker::releasePage(QJsonObject{{"html_url","https://github.com/bolivian12/launcher/releases/tag/v4.1.0"}},false).isEmpty());
        QVERIFY(UpdateChecker::releasePage(QJsonObject{{"html_url","https://evil.test/bolivian12/launcher/releases/tag/v5.0.0"}},false).isEmpty());
    }
    void backgroundCarousel(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());qputenv("EBALIA_LANGUAGE","es");QSettings().setValue("ui/rotateBackgrounds",true);
        MainWindow window;window.resize(1280,800);window.show();window.showPage(MainWindow::Home);
        auto banner=window.findChild<HomeBanner*>();QVERIFY(banner);auto timer=banner->findChild<QTimer*>("heroRotation");QVERIFY(timer);
        auto pause=banner->findChild<QToolButton*>("heroPause");auto next=banner->findChild<QToolButton*>("heroNext");auto previous=banner->findChild<QToolButton*>("heroPrevious");QVERIFY(pause&&next&&previous);
        QVERIFY(timer->isActive());QCOMPARE(banner->slideCount(),4);
        QTest::mouseClick(pause,Qt::LeftButton);QVERIFY(!timer->isActive());QVERIFY(!QSettings().value("ui/rotateBackgrounds").toBool());
        for(int i=1;i<=banner->slideCount();++i){QTest::mouseClick(next,Qt::LeftButton);QCOMPARE(banner->currentSlide(),i%banner->slideCount());QVERIFY(!timer->isActive());}
        QTest::mouseClick(previous,Qt::LeftButton);QCOMPARE(banner->currentSlide(),banner->slideCount()-1);
        auto dots=banner->findChildren<QToolButton*>("heroDot");QCOMPARE(dots.size(),banner->slideCount());QTest::mouseClick(dots[1],Qt::LeftButton);QCOMPARE(banner->currentSlide(),1);QVERIFY(dots[1]->isChecked());
        {HomeBanner another;another.show();QVERIFY(another.findChild<QToolButton*>("heroPause")->isChecked());QVERIFY(!another.findChild<QTimer*>("heroRotation")->isActive());}
        timer->setInterval(80);QTest::mouseClick(pause,Qt::LeftButton);QTRY_VERIFY_WITH_TIMEOUT(banner->currentSlide()!=1,500);
        window.showPage(MainWindow::Instances);QVERIFY(!timer->isActive());const int hidden=banner->currentSlide();QTest::qWait(180);QCOMPARE(banner->currentSlide(),hidden);
        window.showPage(MainWindow::Home);QVERIFY(timer->isActive());window.close();QSettings().remove("ui/rotateBackgrounds");
    }
    void responsiveHomeAndArchive(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());qputenv("EBALIA_LANGUAGE","es");
        MainWindow window;window.show();auto pages=window.findChild<QStackedWidget*>();QVERIFY(pages);
        for(const QSize size:{QSize(640,480),QSize(800,600),QSize(1280,800),QSize(1920,1080)}){
            window.resize(size);window.showPage(MainWindow::Home);QTest::qWait(80);QCOMPARE(window.size(),size);
            auto home=qobject_cast<QScrollArea*>(pages->currentWidget());QVERIFY(home);QCOMPARE(home->horizontalScrollBar()->maximum(),0);
            auto play=window.findChild<QPushButton*>("homePlay");auto picker=window.findChild<QPushButton*>("instancePicker");auto bar=window.findChild<QFrame*>("playBar");
            QRect playRect(play->mapTo(bar,QPoint()),play->size()),pickerRect(picker->mapTo(bar,QPoint()),picker->size());
            QVERIFY(bar->rect().contains(playRect));QVERIFY(bar->rect().contains(pickerRect));QVERIFY(!playRect.intersects(pickerRect));
            window.showPage(MainWindow::Lost);QTest::qWait(80);auto archive=qobject_cast<QScrollArea*>(pages->currentWidget());QVERIFY(archive);QVERIFY2(archive->horizontalScrollBar()->maximum()==0,qPrintable(QString("Lost archive overflows by %1 px at %2 px").arg(archive->horizontalScrollBar()->maximum()).arg(size.width())));
            auto detail=window.findChild<QFrame*>("lostDetail");auto install=window.findChild<QPushButton*>("lostInstall");auto launch=window.findChild<QPushButton*>("lostPlay");
            QRect installRect(install->mapTo(detail,QPoint()),install->size()),launchRect(launch->mapTo(detail,QPoint()),launch->size());
            QVERIFY(detail->rect().contains(installRect));QVERIFY(detail->rect().contains(launchRect));QVERIFY(!installRect.intersects(launchRect));
            QVERIFY(installRect.left()-launchRect.right()>=10);
            for(int page=0;page<=MainWindow::Settings;++page){
                window.showPage(page);QTest::qWait(30);auto area=qobject_cast<QScrollArea*>(pages->currentWidget());
                QVERIFY2(area->horizontalScrollBar()->maximum()==0,qPrintable(QString("Page %1 overflows at %2 px").arg(page).arg(size.width())));
            }
        }
        auto list=window.findChild<QListWidget*>("lostList");QVERIFY(list);QSet<QByteArray> images;
        for(int i=0;i<list->count();++i){auto path=list->item(i)->data(Qt::UserRole+1).toJsonObject()["art"].toString();QFile image(path);QVERIFY2(image.open(QIODevice::ReadOnly),qPrintable(path));images.insert(QCryptographicHash::hash(image.readAll(),QCryptographicHash::Sha256));}
        QCOMPARE(images.size(),list->count());window.close();
    }
    void compactInstanceDetail(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());qputenv("EBALIA_LANGUAGE","es");
        {McInstanceManager manager(data.path());manager.createInstance("Mi mundo de aventuras con amigos","1.20.1","vanilla");}
        MainWindow window;window.resize(640,480);window.show();window.showPage(MainWindow::Instances);QTest::qWait(60);
        auto card=window.findChild<QFrame*>("instanceCard");QVERIFY(card);QTest::mouseClick(card,Qt::LeftButton,{},QPoint(30,30));QTest::qWait(60);
        auto content=window.findChild<QWidget*>("instanceDetailContent");auto play=window.findChild<QPushButton*>("detailPlay");auto name=content->findChild<QLabel*>("detailTitle");
        QVERIFY(content&&play&&name);QRect playRect(play->mapTo(content,QPoint()),play->size()),nameRect(name->mapTo(content,QPoint()),name->size());
        QVERIFY(content->rect().contains(playRect));QVERIFY(content->rect().contains(nameRect));QVERIFY(!playRect.intersects(nameRect));
        QCOMPARE(content->width(),content->parentWidget()->width());
        for(auto stat:content->findChildren<QFrame*>("statCard"))QVERIFY(content->rect().contains(QRect(stat->mapTo(content,QPoint()),stat->size())));
    }
    void instanceGalleryServersAndIcon(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());qputenv("EBALIA_LANGUAGE","es");
        QString dir;{McInstanceManager manager(data.path());dir=manager.createInstance("Galería","1.20.1","fabric");}
        QDir().mkpath(dir+"/screenshots");
        for(int n=0;n<3;++n){QImage shot(320,180,QImage::Format_RGB32);shot.fill(QColor::fromHsv(n*100,180,200));QVERIFY(shot.save(dir+QString("/screenshots/2026-10-0%1_12.00.00.png").arg(n+1)));}
        ServerList::add(dir+"/servers.dat","Hypixel","mc.hypixel.net");ServerList::add(dir+"/servers.dat","","play.example.org");
        MainWindow window;window.resize(1280,900);window.show();window.showPage(MainWindow::Instances);QTest::qWait(60);
        auto card=window.findChild<QFrame*>("instanceCard");QVERIFY(card);QTest::mouseClick(card,Qt::LeftButton,{},QPoint(30,30));QTest::qWait(60);
        auto gallery=window.findChild<QListWidget*>("instanceGallery");auto servers=window.findChild<QListWidget*>("instanceServers");QVERIFY(gallery&&servers);
        QTRY_COMPARE(gallery->count(),3);QCOMPARE(servers->count(),2);QVERIFY(servers->item(1)->text().contains("play.example.org"));
        QTRY_VERIFY(!gallery->item(0)->icon().isNull());
        // Built-in icon from the instance page menu.
        auto icon=window.findChild<QToolButton*>("detailIcon");QVERIFY(icon&&icon->menu());
        QAction *tnt=nullptr;for(auto a:icon->menu()->actions())if(a->text()==InstanceIcons::name("tnt"))tnt=a;
        QVERIFY(tnt);QVERIFY(icon->menu()->findChild<QAction*>("customInstanceIcon"));tnt->trigger();
        QCOMPARE(ModRepository::read(dir+"/instance.json")["icon"].toString(),QString("tnt"));
        // A picture from the computer, after the instance exists.
        auto detail=static_cast<InstanceDetail*>(window.findChild<QWidget*>("instanceDetail"));QVERIFY(detail&&detail->changeIcon);
        QImage picture(400,300,QImage::Format_ARGB32);picture.fill(QColor(200,40,180));detail->changeIcon("custom",picture.scaled(128,128));
        QCOMPARE(ModRepository::read(dir+"/instance.json")["icon"].toString(),QString("custom"));QVERIFY(QFile::exists(dir+"/instance-icon.png"));
        QCOMPARE(QImage(dir+"/instance-icon.png").size(),QSize(128,128));
        const auto out=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");if(!out.isEmpty()){QDir().mkpath(out);QTest::qWait(300);QVERIFY(window.grab().save(out+"/instance-detail-extras.png"));}
    }
    void dialogOpenedFromAPageStaysUsable(){
        // Regression: the skin import dialog (a child of a page) opened with Save and Cancel disabled.
        QMainWindow window;auto central=new QWidget;auto layout=new QVBoxLayout(central);auto page=new QWidget;layout->addWidget(page);window.setCentralWidget(central);window.show();
        QDialog dialog(page);auto form=new QFormLayout(&dialog);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);form->addRow(buttons);
        connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
        QTimer::singleShot(200,&dialog,[&]{
            QVERIFY(dialog.isEnabled());auto save=buttons->button(QDialogButtonBox::Save);QVERIFY(save->isEnabled());QVERIFY(!central->isEnabled());
            QTest::mouseClick(save,Qt::LeftButton);
        });
        QCOMPARE(Ui::openWindow(dialog),int(QDialog::Accepted));QVERIFY(central->isEnabled());
    }
    void compactInstanceWizard(){
        QTemporaryDir data;McInstanceManager manager(data.path());Language::current="es";
        CreateInstanceDialog dialog(&manager,{{"1.20.1","release","","2023-06-12"}},{},{},false);dialog.resize(640,480);dialog.show();
        for(int page=0;page<=CreateInstanceDialog::Technic;++page){
            dialog.showPage(page);QTest::qWait(30);QCOMPARE(dialog.size(),QSize(640,480));
            auto area=dialog.findChild<QScrollArea*>("instanceFormScroll");QVERIFY(area);QVERIFY2(area->horizontalScrollBar()->maximum()==0,qPrintable(QString("Instance page %1 overflows by %2 px").arg(page).arg(area->horizontalScrollBar()->maximum())));
            auto create=dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok);
            QVERIFY(dialog.rect().contains(QRect(create->mapTo(&dialog,QPoint()),create->size())));
        }
        auto sources=dialog.findChild<QListWidget*>("instanceSources");QVERIFY(sources->width()<100);for(int i=0;i<sources->count();++i){QVERIFY(!sources->item(i)->toolTip().isEmpty());QVERIFY(!sources->item(i)->icon().isNull());}
    }
    void everyLanguage(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());QSettings().setValue("ui/tutorialSeen",true);
        for(const auto &lang:Language::available()){
            qputenv("EBALIA_LANGUAGE",lang.toUtf8());MainWindow window;window.show();window.showPage(0);QTest::qWait(25);
            QCOMPARE(Language::current,lang);
            auto nav=window.findChild<QListWidget*>("navigation");QVERIFY(nav);QCOMPARE(nav->count(),int(MainWindow::Settings));
            auto pages=window.findChild<QStackedWidget*>();QVERIFY(pages);QCOMPARE(pages->count(),int(MainWindow::Settings)+1);
            for(int page=0;page<=MainWindow::Settings;++page){
                window.showPage(page);QTest::qWait(5);QVERIFY(window.isVisible());QCOMPARE(pages->currentIndex(),page);
                QCOMPARE(nav->currentRow(),page==MainWindow::Settings?-1:page);QCOMPARE(window.findChild<QPushButton*>("navSettings")->isChecked(),page==MainWindow::Settings);
                for(auto b:window.findChildren<QPushButton*>())if(b->isVisible())QVERIFY2(!b->text().trimmed().isEmpty()||!b->icon().isNull()||!b->accessibleName().isEmpty(),"Unnamed visible button");
            }
            nav->setCurrentRow(MainWindow::Lost);QCOMPARE(pages->currentIndex(),int(MainWindow::Lost));
            window.showPage(MainWindow::Community);auto tabs=window.findChild<QTabWidget*>("communityTabs");QVERIFY(tabs);QCOMPARE(tabs->count(),5);for(int tab=0;tab<5;++tab){tabs->setCurrentIndex(tab);QTest::qWait(5);}tabs->setCurrentIndex(0);
            QString dir=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");if(!dir.isEmpty()){QDir().mkpath(dir);window.showPage(MainWindow::Home);QVERIFY(window.grab().save(dir+"/"+lang+"-home.png"));window.showPage(MainWindow::Guide);QVERIFY(window.grab().save(dir+"/"+lang+"-guide.png"));}
            window.showPage(MainWindow::Community);QCOMPARE(window.findChildren<QToolButton*>("fanartCard").size(),20);
            if(!dir.isEmpty()){QVERIFY(window.grab().save(dir+"/"+lang+"-fanarts.png"));tabs->setCurrentIndex(4);QVERIFY(window.grab().save(dir+"/"+lang+"-patreon.png"));tabs->setCurrentIndex(0);}
            window.close();
        }
    }
    void languageSwitchRebuilds(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());qputenv("EBALIA_LANGUAGE","en");MainWindow window;window.show();window.showPage(MainWindow::Settings);
        auto combo=window.findChild<QComboBox*>("languageChoice");QVERIFY(combo);combo->setCurrentIndex(combo->findData("es"));emit combo->activated(combo->currentIndex());
        auto first=[&]{auto nav=window.centralWidget()->findChild<QListWidget*>("navigation");return nav?nav->item(0)->data(Qt::AccessibleTextRole).toString():QString();};
        QTRY_COMPARE(first(),QString("Inicio"));QCOMPARE(window.centralWidget()->findChild<QStackedWidget*>()->currentIndex(),int(MainWindow::Settings));
        QVERIFY(window.findChild<QPushButton*>("navSettings")->isChecked());QSettings().remove("ui/language");
    }
    void instanceWizardFilters(){
        QTemporaryDir data;McInstanceManager manager(data.path());QList<McVersion> catalog{{"1.20.1","release","","2023-06-12"},{"24w01a","snapshot","","2024-01-01"},{"b1.7.3","old_beta","","2011-07-08"}};
        for(auto language:Language::available()){Language::current=language;CreateInstanceDialog dialog(&manager,catalog,{}, {},false);dialog.show();auto tree=dialog.findChild<QTreeWidget*>("minecraftVersions");QVERIFY(tree);QCOMPARE(tree->topLevelItemCount(),1);auto sources=dialog.findChild<QListWidget*>("instanceSources");QCOMPARE(sources->count(),9);auto name=dialog.findChild<QLineEdit*>("instanceName");name->setText("Test");auto snapshot=dialog.findChild<QCheckBox*>("snapshotFilter");snapshot->setChecked(true);QCOMPARE(tree->topLevelItemCount(),2);dialog.findChild<QLineEdit*>("versionSearch")->setText("24w");QCOMPARE(tree->topLevelItemCount(),1);QCOMPARE(dialog.configuration()["mcVersion"].toString(),QString("24w01a"));QString dir=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");if(!dir.isEmpty()){dialog.findChild<QLineEdit*>("versionSearch")->clear();QVERIFY(dialog.grab().save(dir+"/"+language+"-create.png"));}dialog.close();}
    }
    void deleteInstanceThroughUi(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());qputenv("EBALIA_LANGUAGE","en");
        McInstanceManager setup(data.path());auto first=setup.createInstance("Keep","1.20.1","vanilla");auto second=setup.createInstance("Remove","1.20.1","vanilla");QFile world(second+"/world.txt");QVERIFY(world.open(QIODevice::WriteOnly));world.write("world data");world.close();
        MainWindow window;window.resize(1200,800);window.show();window.showPage(MainWindow::Instances);QTest::qWait(20);
        auto cards=[&]{return window.findChildren<QFrame*>("instanceCard");};QCOMPARE(cards().size(),2);
        QFrame *card=nullptr;for(auto c:cards())if(c->property("dir").toString()==second)card=c;QVERIFY(card);QTest::mouseClick(card,Qt::LeftButton,{},QPoint(30,30));QTest::qWait(20);
        auto remove=window.findChild<QPushButton*>("deleteInstance");QVERIFY(remove);QVERIFY(remove->isVisible());
        QTimer::singleShot(10,&window,[]{auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);QTest::mouseClick(box->button(QMessageBox::No),Qt::LeftButton);});QTest::mouseClick(remove,Qt::LeftButton);QTest::qWait(20);QCOMPARE(cards().size(),2);QVERIFY(QFile::exists(second));
        QTimer::singleShot(10,&window,[]{auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);QTest::mouseClick(box->button(QMessageBox::Yes),Qt::LeftButton);});QTest::mouseClick(remove,Qt::LeftButton);QTest::qWait(20);
        QCOMPARE(cards().size(),1);QVERIFY(window.findChild<QWidget*>("instanceGrid")->isVisible());QVERIFY(QFile::exists(first+"/instance.json"));QVERIFY(!QFile::exists(second));
        QDir trash(data.path()+"/mc/trash");auto entries=trash.entryList(QDir::Dirs|QDir::NoDotAndDotDot);QCOMPARE(entries.size(),1);QVERIFY(QFile::exists(trash.filePath(entries.first()+"/world.txt")));
    }
    void playAsksOnlyForAName(){
        // First game: no account and nothing installed. One press asks for a player name and starts the installation.
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());qputenv("EBALIA_LANGUAGE","en");
        {McInstanceManager setup(data.path());setup.createInstance("First","0.0-test","vanilla");}
        MainWindow window;window.resize(1200,800);window.show();window.showPage(MainWindow::Instances);QTest::qWait(20);
        auto play=window.findChild<QPushButton*>("cardPlay");QVERIFY(play);QVERIFY(play->text().contains("Play"));
        bool asked=false;QTimer::singleShot(20,&window,[&]{auto dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);QVERIFY(!dialog->textValue().isEmpty());dialog->setTextValue("Tester");asked=true;dialog->accept();});
        QTest::mouseClick(play,Qt::LeftButton);QVERIFY(asked);
        auto accounts=window.findChild<AccountManager*>();QVERIFY(accounts);QCOMPARE(accounts->active().name,QString("Tester"));
        auto manager=window.findChild<McInstanceManager*>();QVERIFY(manager->isInstalling(manager->instances().first().dir));
        // The unknown version fails; close the error so the test does not wait on it.
        auto closer=new QTimer(&window);connect(closer,&QTimer::timeout,&window,[]{if(auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))box->accept();});closer->start(50);
        QTRY_VERIFY_WITH_TIMEOUT(!manager->isInstalling(manager->instances().first().dir),60000);
    }
    void copyAndGroupThroughCore(){
        QTemporaryDir data;McInstanceManager manager(data.path());auto dir=manager.createInstance("Base","1.20.1","fabric");QDir().mkpath(dir+"/saves/World");QFile f(dir+"/saves/World/level.dat");QVERIFY(f.open(QIODevice::WriteOnly));f.write("level");f.close();QDir().mkpath(dir+"/natives");
        auto copy=manager.copyInstance(dir,"Base copy");QVERIFY(copy!=dir);QCOMPARE(ModRepository::read(copy+"/instance.json")["name"].toString(),QString("Base copy"));QVERIFY(QFile::exists(copy+"/saves/World/level.dat"));QVERIFY(!QDir(copy+"/natives").exists());
        auto zip=data.path()+"/export.zip";manager.exportInstance(copy,zip);QVERIFY(QFile::exists(zip));QCOMPARE(manager.instances().size(),2);
    }
    void duplicateCreationThroughUi(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());qputenv("EBALIA_LANGUAGE","en");MainWindow window;window.show();window.showPage(0);
        auto manager=window.findChild<McInstanceManager*>();QVERIFY(manager);emit manager->manifestReady(QList<McVersion>{{"1.20.1","release","https://example.invalid","2023-06-12"}});
        for(int n=0;n<2;++n){
            // The first time from the Play button of Home (no instances yet), then from the library.
            bool handled=false;window.showPage(n?MainWindow::Instances:MainWindow::Home);QTest::qWait(10);
            if(n){auto back=window.findChild<QPushButton*>("backButton");QVERIFY(back&&back->isVisible());QTest::mouseClick(back,Qt::LeftButton);QTest::qWait(10);} // creating opens the new instance; go back to the library
            QTimer::singleShot(30,&window,[&]{
                QDialog *dialog=nullptr;for(auto w:QApplication::topLevelWidgets())if(auto d=qobject_cast<QDialog*>(w);d&&d->isVisible()&&d->objectName()=="createInstanceDialog")dialog=d;QVERIFY(dialog);
                // A separate window: not modal (GNOME would glue it to the launcher) and the launcher behind it is disabled.
                QCOMPARE(dialog->windowModality(),Qt::NonModal);QVERIFY(!window.centralWidget()->isEnabled());QVERIFY(dialog->isSizeGripEnabled());
                auto edits=dialog->findChildren<QLineEdit*>();QVERIFY(!edits.isEmpty());edits.first()->setText("Same instance");auto boxes=dialog->findChildren<QDialogButtonBox*>();QVERIFY(!boxes.isEmpty());handled=true;QTest::mouseClick(boxes.first()->button(QDialogButtonBox::Ok),Qt::LeftButton);});
            auto create=window.findChild<QPushButton*>(n?"newInstance":"homePlay");QVERIFY(create);QVERIFY(create->isVisible());QTest::mouseClick(create,Qt::LeftButton);QVERIFY(handled);QVERIFY(window.centralWidget()->isEnabled());
        }
        QCOMPARE(manager->instances().size(),2);QVERIFY(manager->instances()[0].dir!=manager->instances()[1].dir);
    }
    void liveCurseForgeBrowser(){
        if(!qEnvironmentVariableIsSet("EBALIA_LIVE_TESTS"))QSKIP("Opt-in network test");
        QVERIFY2(!ModRepository::curseForgeKey().isEmpty(),"Configure a CurseForge API key before running this test");
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());qputenv("EBALIA_LANGUAGE","es");
        MainWindow window;window.show();window.showPage(MainWindow::Home);
        auto manager=window.findChild<McInstanceManager*>();QVERIFY(manager);
        CreateInstanceDialog dialog(manager,{{"1.20.1","release","","2023-06-12"}},{},{},false,&window);dialog.show();
        auto sources=dialog.findChild<QListWidget*>("instanceSources");QVERIFY(sources);
        QTest::mouseClick(sources->viewport(),Qt::LeftButton,{},sources->visualItemRect(sources->item(CreateInstanceDialog::CurseForge)).center());
        auto page=dialog.findChild<QWidget*>("curseforgePage");QVERIFY(page&&page->isVisible());
        auto key=page->findChild<QLineEdit*>("curseForgeKey");QVERIFY(key&&!key->isVisible());
        auto search=page->findChild<QLineEdit*>("packSearch");auto results=page->findChild<QListWidget*>("packResults");
        auto versions=page->findChild<QComboBox*>("packVersions");QVERIFY(search&&results&&versions);
        QTest::keyClicks(search,"SkyFactory 4");QTest::keyClick(search,Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(results->count()>0,45000);
        QTest::mouseClick(results->viewport(),Qt::LeftButton,{},results->visualItemRect(results->item(0)).center());
        QTRY_VERIFY_WITH_TIMEOUT(versions->count()>0,45000);
        const auto config=dialog.configuration();QCOMPARE(config["providerPack"].toObject()["provider"].toString(),QString("curseforge"));
        QVERIFY(!config["providerVersion"].toObject()["id"].toString().isEmpty());
        QVERIFY(dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled());
        const auto out=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");if(!out.isEmpty()){QDir().mkpath(out);QTest::qWait(500);QVERIFY(dialog.grab().save(out+"/curseforge-live.png"));}
        qInfo().noquote()<<"CurseForge UI:"<<results->count()<<"results,"<<versions->count()<<"versions; instance creation enabled";
        dialog.close();window.close();
    }
    void screenshots(){
        // Visual check of the main views: EBALIA_TEST_ARTIFACTS=<folder> [EBALIA_SHOT_LANGUAGE=es] [EBALIA_LIVE_TESTS=1 for provider pages].
        const QString out=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");if(out.isEmpty())QSKIP("Set EBALIA_TEST_ARTIFACTS to save screenshots");QDir().mkpath(out);
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());const auto language=qEnvironmentVariable("EBALIA_SHOT_LANGUAGE","es");qputenv("EBALIA_LANGUAGE",language.toUtf8());
        McInstanceManager setup(data.path());const auto now=QDateTime::currentSecsSinceEpoch();
        struct Sample{const char *name,*version,*loader,*group,*icon;bool ready;qint64 played;int mods,worlds;};
        const QList<Sample> samples{{"Survival","1.20.1","forge","Mis aventuras","creeper",true,now-7200,42,3},{"Skyblock","1.21.1","fabric","Mis aventuras","gem",true,now-864000,18,1},{"Adventure","1.21.1","fabric","Mis aventuras","book",false,0,0,0},
            {"PvP Practice","1.8.9","vanilla","Construir y jugar","pickaxe",true,now-259200,0,2},{"Creative","1.21.4","vanilla","Construir y jugar","crafting",true,now-400,0,5},{"Vanilla 1.21.4","1.21.4","vanilla","","grass",true,0,0,0}};
        QString first;
        for(const auto &sample:samples){
            auto dir=setup.createInstance(sample.name,sample.version,sample.loader);if(first.isEmpty())first=dir;auto info=ModRepository::read(dir+"/instance.json");
            info["group"]=sample.group;info["icon"]=sample.icon;info["ready"]=sample.ready;info["lastPlayed"]=sample.played;info["loaderVersion"]=QString(sample.loader)=="forge"?"47.3.0":QString(sample.loader)=="fabric"?"0.16.9":"";ModRepository::write(dir+"/instance.json",info);
            for(int n=0;n<sample.mods;++n){QFile f(dir+"/mods/mod"+QString::number(n)+".jar");QVERIFY(f.open(QIODevice::WriteOnly));}
            for(int n=0;n<sample.worlds;++n)QDir().mkpath(dir+"/saves/World "+QString::number(n));
        }
        MainWindow window;window.resize(1280,820);window.show();window.showPage(0);auto manager=window.findChild<McInstanceManager*>();
        emit manager->manifestReady(QList<McVersion>{{"1.21.4","release","","2024-12-03"},{"1.21.1","release","","2024-08-08"},{"1.20.1","release","","2023-06-12"},{"24w14a","snapshot","","2024-04-03"},{"1.8.9","release","","2015-12-09"}});
        QTest::qWait(50);QVERIFY(window.grab().save(out+"/"+language+"-home.png"));
        window.showPage(MainWindow::Instances);QTest::qWait(80);QVERIFY(window.grab().save(out+"/"+language+"-library.png"));
        QCOMPARE(window.findChildren<QFrame*>("instanceCard").size(),samples.size());
        QFrame *card=nullptr;for(auto c:window.findChildren<QFrame*>("instanceCard"))if(c->property("dir").toString()==first)card=c;QVERIFY(card);
        QTest::mouseClick(card,Qt::LeftButton,{},QPoint(20,20));QTest::qWait(80);QVERIFY(window.findChild<QWidget*>("instanceDetail")->isVisible());QVERIFY(window.grab().save(out+"/"+language+"-detail.png"));
        for(auto page:{MainWindow::Lost,MainWindow::Explore,MainWindow::Packs,MainWindow::Skins,MainWindow::News,MainWindow::Settings}){window.showPage(page);QTest::qWait(50);QVERIFY(window.grab().save(out+"/"+language+"-page"+QString::number(page)+".png"));}
        window.resize(900,600);QTest::qWait(50);window.showPage(MainWindow::Home);QTest::qWait(50);QVERIFY(window.grab().save(out+"/"+language+"-home-small.png"));window.showPage(MainWindow::Instances);QTest::qWait(50);QVERIFY(window.grab().save(out+"/"+language+"-detail-small.png"));window.resize(1280,820);
        {SetupDialog setup(&window);setup.show();QTest::qWait(1500);QVERIFY(setup.grab().save(out+"/"+language+"-setup.png"));}
        window.showPage(0);
        CreateInstanceDialog dialog(manager,{{"1.21.4","release","","2024-12-03"},{"1.21.1","release","","2024-08-08"},{"1.20.1","release","","2023-06-12"}},{}, {},false,&window);dialog.show();QTest::qWait(50);
        QVERIFY(dialog.grab().save(out+"/"+language+"-create-custom.png"));dialog.showPage(CreateInstanceDialog::Import);QTest::qWait(50);QVERIFY(dialog.grab().save(out+"/"+language+"-create-import.png"));dialog.showPage(CreateInstanceDialog::CurseForge);QTest::qWait(50);QVERIFY(dialog.grab().save(out+"/"+language+"-create-curseforge.png"));
        if(qEnvironmentVariableIsSet("EBALIA_LIVE_TESTS")){qunsetenv("EBALIA_NO_NETWORK");for(auto page:{CreateInstanceDialog::Modrinth,CreateInstanceDialog::FTB}){dialog.showPage(page);QTest::qWait(9000);auto list=dialog.findChildren<QListWidget*>("packResults");for(auto l:list)if(l->isVisible()&&l->count())l->setCurrentRow(0);QTest::qWait(4000);QVERIFY(dialog.grab().save(out+"/"+language+"-create-"+QString::number(page)+".png"));}qputenv("EBALIA_NO_NETWORK","1");}
        dialog.close();
    }
};
QTEST_MAIN(UiTests)
#include "UiTests.moc"
