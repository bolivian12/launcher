#include <QtTest>
#include <QtWidgets>
#include "MainWindow.hpp"
#include "Language.hpp"
#include "McInstanceManager.hpp"
#include "CreateInstanceDialog.hpp"
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
        for(auto language:Language::available()){Language::current=language;CreateInstanceDialog dialog(&manager,catalog,{}, {},false);dialog.show();auto tree=dialog.findChild<QTreeWidget*>("minecraftVersions");QVERIFY(tree);QCOMPARE(tree->topLevelItemCount(),1);auto sources=dialog.findChild<QListWidget*>("instanceSources");QCOMPARE(sources->count(),10);auto name=dialog.findChild<QLineEdit*>("instanceName");name->setText("Test");auto snapshot=dialog.findChild<QCheckBox*>("snapshotFilter");snapshot->setChecked(true);QCOMPARE(tree->topLevelItemCount(),2);dialog.findChild<QLineEdit*>("versionSearch")->setText("24w");QCOMPARE(tree->topLevelItemCount(),1);QCOMPARE(dialog.configuration()["mcVersion"].toString(),QString("24w01a"));QString dir=qEnvironmentVariable("EBALIA_TEST_ARTIFACTS");if(!dir.isEmpty()){dialog.findChild<QLineEdit*>("versionSearch")->clear();QVERIFY(dialog.grab().save(dir+"/"+language+"-create.png"));}dialog.close();}
    }
    void deleteInstanceThroughUi(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());qputenv("EBALIA_LANGUAGE","en");
        McInstanceManager setup(data.path());auto first=setup.createInstance("Keep","1.20.1","vanilla");auto second=setup.createInstance("Remove","1.20.1","vanilla");QFile world(second+"/world.txt");QVERIFY(world.open(QIODevice::WriteOnly));world.write("world data");world.close();
        MainWindow window;window.show();window.showPage(0);window.findChild<QTabWidget*>()->setCurrentIndex(1);
        auto list=window.findChild<QListWidget*>("instanceList");QVERIFY(list);for(int n=0;n<list->count();++n)if(list->item(n)->data(Qt::UserRole).toString()==second)list->setCurrentRow(n);
        auto remove=window.findChild<QPushButton*>("deleteInstance");QVERIFY(remove);QVERIFY(remove->isVisible());
        QTimer::singleShot(10,&window,[]{auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);QTest::mouseClick(box->button(QMessageBox::No),Qt::LeftButton);});QTest::mouseClick(remove,Qt::LeftButton);QCOMPARE(list->count(),2);
        QTimer::singleShot(10,&window,[]{auto box=qobject_cast<QMessageBox*>(QApplication::activeModalWidget());QVERIFY(box);QTest::mouseClick(box->button(QMessageBox::Yes),Qt::LeftButton);});QTest::mouseClick(remove,Qt::LeftButton);
        QCOMPARE(list->count(),1);QVERIFY(QFile::exists(first+"/instance.json"));QVERIFY(!QFile::exists(second));QDir trash(data.path()+"/mc/trash");auto entries=trash.entryList(QDir::Dirs|QDir::NoDotAndDotDot);QCOMPARE(entries.size(),1);QVERIFY(QFile::exists(trash.filePath(entries.first()+"/world.txt")));
    }
    void duplicateCreationThroughUi(){
        QTemporaryDir data;qputenv("EBALIA_DATA_DIR",data.path().toUtf8());qputenv("EBALIA_LANGUAGE","en");MainWindow window;window.show();window.showPage(0);
        auto manager=window.findChild<McInstanceManager*>();QVERIFY(manager);emit manager->manifestReady(QList<McVersion>{{"1.20.1","release","https://example.invalid","2023-06-12"}});
        for(int n=0;n<2;++n){
            bool handled=false;
            QTimer::singleShot(20,&window,[&]{auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());QVERIFY(dialog);auto edits=dialog->findChildren<QLineEdit*>();QVERIFY(!edits.isEmpty());edits.first()->setText("Same instance");auto boxes=dialog->findChildren<QDialogButtonBox*>();QVERIFY(!boxes.isEmpty());handled=true;QTest::mouseClick(boxes.first()->button(QDialogButtonBox::Ok),Qt::LeftButton);});
            QPushButton *create=nullptr;for(auto b:window.findChildren<QPushButton*>())if(b->isVisible()&&b->text().contains("New instance")){create=b;break;}QVERIFY(create);QTest::mouseClick(create,Qt::LeftButton);QVERIFY(handled);
        }
        QCOMPARE(manager->instances().size(),2);QVERIFY(manager->instances()[0].dir!=manager->instances()[1].dir);
    }
};
QTEST_MAIN(UiTests)
#include "UiTests.moc"
