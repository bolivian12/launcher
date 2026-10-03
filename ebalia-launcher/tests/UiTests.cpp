#include <QtTest>
#include <QtWidgets>
#include "MainWindow.hpp"
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
class UiTests:public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE void captureUrl(const QUrl &url){lastUrl=url;}
private:
    QUrl lastUrl;
private slots:
    void initTestCase(){QApplication::setStyle("Fusion");QCoreApplication::setOrganizationName("EBALIA-test");QCoreApplication::setApplicationName("UI-test");qputenv("EBALIA_NO_NETWORK","1");}
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
    void releaseDetection(){
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
