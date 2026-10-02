#include <QtTest>
#include <QtWidgets>
#include "MainWindow.hpp"
#include "Language.hpp"
#include "McInstanceManager.hpp"
#include "CreateInstanceDialog.hpp"
#include "ModRepository.hpp"
#include "SetupDialog.hpp"
class UiTests:public QObject {
    Q_OBJECT
private slots:
    void initTestCase(){QApplication::setStyle("Fusion");QCoreApplication::setOrganizationName("EBALIA-test");QCoreApplication::setApplicationName("UI-test");qputenv("EBALIA_NO_NETWORK","1");}
    void everyLanguage(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());QSettings().setValue("ui/tutorialSeen",true);
        for(const auto &lang:Language::available()){
            qputenv("EBALIA_LANGUAGE",lang.toUtf8());MainWindow window;window.show();window.showPage(0);QTest::qWait(25);
            QCOMPARE(Language::current,lang);
            auto nav=window.findChild<QListWidget*>("navigation");QVERIFY(nav);QCOMPARE(nav->count(),12);
            for(int page=0;page<12;++page){window.showPage(page);QTest::qWait(5);QVERIFY(window.isVisible());}
            window.showPage(0);auto tabs=window.findChild<QTabWidget*>();QVERIFY(tabs);QCOMPARE(tabs->count(),3);for(int tab=0;tab<3;++tab){tabs->setCurrentIndex(tab);QTest::qWait(5);}tabs->setCurrentIndex(0);
            for(auto b:window.findChildren<QPushButton*>())if(b->isVisible())QVERIFY2(!b->text().isEmpty()||!b->icon().isNull()||!b->accessibleName().isEmpty(),"Unnamed visible button");
            QString dir=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");if(!dir.isEmpty()){QDir().mkpath(dir);QVERIFY(window.grab().save(dir+"/"+lang+"-home.png"));window.showPage(5);QVERIFY(window.grab().save(dir+"/"+lang+"-guide.png"));}
            window.showPage(7);QCOMPARE(window.findChildren<QToolButton*>("fanartCard").size(),20);
            if(!dir.isEmpty()){QVERIFY(window.grab().save(dir+"/"+lang+"-fanarts.png"));window.showPage(11);QVERIFY(window.grab().save(dir+"/"+lang+"-patreon.png"));}
            window.close();
        }
    }
    void instanceWizardFilters(){
        QTemporaryDir data;McInstanceManager manager(data.path());QList<McVersion> catalog{{"1.20.1","release","","2023-06-12"},{"24w01a","snapshot","","2024-01-01"},{"b1.7.3","old_beta","","2011-07-08"}};
        for(auto language:Language::available()){Language::current=language;CreateInstanceDialog dialog(&manager,catalog,{}, {},false);dialog.show();auto tree=dialog.findChild<QTreeWidget*>("minecraftVersions");QVERIFY(tree);QCOMPARE(tree->topLevelItemCount(),1);auto sources=dialog.findChild<QListWidget*>("instanceSources");QCOMPARE(sources->count(),9);auto name=dialog.findChild<QLineEdit*>("instanceName");name->setText("Test");auto snapshot=dialog.findChild<QCheckBox*>("snapshotFilter");snapshot->setChecked(true);QCOMPARE(tree->topLevelItemCount(),2);dialog.findChild<QLineEdit*>("versionSearch")->setText("24w");QCOMPARE(tree->topLevelItemCount(),1);QCOMPARE(dialog.configuration()["mcVersion"].toString(),QString("24w01a"));QString dir=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");if(!dir.isEmpty()){dialog.findChild<QLineEdit*>("versionSearch")->clear();QVERIFY(dialog.grab().save(dir+"/"+language+"-create.png"));}dialog.close();}
    }
    void deleteInstanceThroughUi(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());qputenv("EBALIA_LANGUAGE","en");
        McInstanceManager setup(data.path());auto first=setup.createInstance("Keep","1.20.1","vanilla");auto second=setup.createInstance("Remove","1.20.1","vanilla");QFile world(second+"/world.txt");QVERIFY(world.open(QIODevice::WriteOnly));world.write("world data");world.close();
        MainWindow window;window.resize(1200,800);window.show();window.showPage(0);window.findChild<QTabWidget*>()->setCurrentIndex(1);QTest::qWait(20);
        auto cards=[&]{return window.findChildren<QFrame*>("instanceCard");};QCOMPARE(cards().size(),2);
        QFrame *card=nullptr;for(auto c:cards())if(c->property("dir").toString()==second)card=c;QVERIFY(card);QTest::mouseClick(card,Qt::LeftButton,{},QPoint(30,30));QTest::qWait(20);
        auto remove=window.findChild<QPushButton*>("deleteInstance");QVERIFY(remove);QVERIFY(remove->isVisible());
        QTimer::singleShot(10,&window,[]{auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);QTest::mouseClick(box->button(QMessageBox::No),Qt::LeftButton);});QTest::mouseClick(remove,Qt::LeftButton);QTest::qWait(20);QCOMPARE(cards().size(),2);QVERIFY(QFile::exists(second));
        QTimer::singleShot(10,&window,[]{auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);QTest::mouseClick(box->button(QMessageBox::Yes),Qt::LeftButton);});QTest::mouseClick(remove,Qt::LeftButton);QTest::qWait(20);
        QCOMPARE(cards().size(),1);QVERIFY(window.findChild<QWidget*>("instanceGrid")->isVisible());QVERIFY(QFile::exists(first+"/instance.json"));QVERIFY(!QFile::exists(second));
        QDir trash(data.path()+"/mc/trash");auto entries=trash.entryList(QDir::Dirs|QDir::NoDotAndDotDot);QCOMPARE(entries.size(),1);QVERIFY(QFile::exists(trash.filePath(entries.first()+"/world.txt")));
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
            bool handled=false;window.findChild<QTabWidget*>()->setCurrentIndex(0);
            QTimer::singleShot(20,&window,[&]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);auto edits=dialog->findChildren<QLineEdit*>();QVERIFY(!edits.isEmpty());edits.first()->setText("Same instance");auto boxes=dialog->findChildren<QDialogButtonBox*>();QVERIFY(!boxes.isEmpty());handled=true;QTest::mouseClick(boxes.first()->button(QDialogButtonBox::Ok),Qt::LeftButton);});
            QPushButton *create=nullptr;for(auto b:window.findChildren<QPushButton*>())if(b->isVisible()&&b->text().contains("New instance")){create=b;break;}QVERIFY(create);QTest::mouseClick(create,Qt::LeftButton);QVERIFY(handled);
        }
        QCOMPARE(manager->instances().size(),2);QVERIFY(manager->instances()[0].dir!=manager->instances()[1].dir);
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
        window.findChild<QTabWidget*>()->setCurrentIndex(1);QTest::qWait(80);QVERIFY(window.grab().save(out+"/"+language+"-library.png"));
        QCOMPARE(window.findChildren<QFrame*>("instanceCard").size(),samples.size());
        QFrame *card=nullptr;for(auto c:window.findChildren<QFrame*>("instanceCard"))if(c->property("dir").toString()==first)card=c;QVERIFY(card);
        QTest::mouseClick(card,Qt::LeftButton,{},QPoint(20,20));QTest::qWait(80);QVERIFY(window.findChild<QWidget*>("instanceDetail")->isVisible());QVERIFY(window.grab().save(out+"/"+language+"-detail.png"));
        window.showPage(1);QTest::qWait(50);QVERIFY(window.grab().save(out+"/"+language+"-lost.png"));
        {SetupDialog setup(&window);setup.show();QTest::qWait(1500);QVERIFY(setup.grab().save(out+"/"+language+"-setup.png"));}
        window.showPage(0);
        CreateInstanceDialog dialog(manager,{{"1.21.4","release","","2024-12-03"},{"1.21.1","release","","2024-08-08"},{"1.20.1","release","","2023-06-12"}},{}, {},false,&window);dialog.show();QTest::qWait(50);
        QVERIFY(dialog.grab().save(out+"/"+language+"-create-custom.png"));dialog.showPage(CreateInstanceDialog::Import);QTest::qWait(50);QVERIFY(dialog.grab().save(out+"/"+language+"-create-import.png"));
        if(qEnvironmentVariableIsSet("EBALIA_LIVE_TESTS")){qunsetenv("EBALIA_NO_NETWORK");for(auto page:{CreateInstanceDialog::Modrinth,CreateInstanceDialog::FTB}){dialog.showPage(page);QTest::qWait(9000);auto list=dialog.findChildren<QListWidget*>("packResults");for(auto l:list)if(l->isVisible()&&l->count())l->setCurrentRow(0);QTest::qWait(4000);QVERIFY(dialog.grab().save(out+"/"+language+"-create-"+QString::number(page)+".png"));}qputenv("EBALIA_NO_NETWORK","1");}
        dialog.close();
    }
};
QTEST_MAIN(UiTests)
#include "UiTests.moc"
