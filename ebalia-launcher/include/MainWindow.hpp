#pragma once
#include <QMainWindow>
#include <QJsonArray>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QPixmap>
#include <functional>
#include "McInstanceManager.hpp"
class QListWidget;
class QLabel;
class QComboBox;
class QLineEdit;
class QStackedWidget;
class QProgressBar;
class VersionManager;
class AccountManager;
class JavaRunner;
class QCloseEvent;
class QPushButton;
class QHBoxLayout;
class QVBoxLayout;
class QNetworkAccessManager;
class PatreonAuth;
class InstanceGrid;
class InstanceDetail;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    // Sidebar order, like the Minecraft Launcher: sections on top, Settings at the bottom.
    enum Page{Home,Instances,Explore,Lost,Packs,Skins,News,Community,Guide,Bedrock,Settings};
    explicit MainWindow(QWidget *parent=nullptr);
    void showPage(int index);
    bool canUpdate() const;
    void startTour();
    // Opens the mods window of an instance.
    Q_INVOKABLE void openMods(const QString &dir){m_selectedDir=dir;manageMods();}
    // Launcher updates: the sidebar shows "Update to <version>" only while one is available.
    enum class LauncherUpdate{Unknown,Checking,Current,Available,Installing,Failed};
    void setLauncherUpdate(LauncherUpdate state,const QString &version={});
signals:
    void installLauncherUpdateRequested();
protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    void adaptSidebar();
    void build();
    LauncherUpdate m_launcherUpdate=LauncherUpdate::Unknown;QString m_launcherVersion;
    QTimer *m_pasteTimer=nullptr;QString m_pasteSnapshot;QSet<QString> m_pasteFailed;
    QWidget *buildSidebar();
    QWidget *buildHome();
    void openCreatorDialog(bool patreonOnly);
    QWidget *buildInstances();
    QWidget *buildExplore();
    QWidget *buildLost();
    QWidget *buildPacks();
    QWidget *buildSkins();
    QWidget *buildNews();
    QWidget *buildCommunity();
    QWidget *buildGuide();
    QWidget *buildSettings();
    void refreshInstances();
    void refreshPacks();
    void refreshAccounts();
    void refreshLost();
    void refreshNews(bool network=true);
    void loadImage(const QString &url,std::function<void(const QPixmap &)> done);
    void accountMenu();
    void instancePickerMenu();
    void selection();
    McInstance selected() const;
    void setInstanceIcon(const QString &dir,const QString &key,const QImage &custom);
    void changeBackground(const QString &dir);
    void showCrash(const QString &dir);
    void adoptPastedInstances();
    void showImportWarnings(const QString &dir);
    void useSoftwareRendering(const QString &dir);
    void installDependency(const QString &dir,const QString &mod,std::function<void(bool)> done);
    void createInstance(bool copy=false,int page=0);
    void instanceMenu();
    void groupMenu(const QString &group);
    void changeGroup();
    void copyInstance();
    void exportInstance();
    void showLog(const QString &dir,QWidget *parent);
    void play();
    void launchInstance(const McInstance &instance);
    void editInstance();
    void removeInstance();
    void manageMods();
    void savePack();
    void applyPack();
    void preview(QJsonArray projects,QString dir);
    void searchMods(int offset=0);
    void account(bool microsoft);
    void lostAction(bool launch);
    void lostSelection();
    static QString lostCategory(const QString &category);
    void work(const QString &label,std::function<QJsonObject()> job,std::function<void(QJsonObject)> done);
    void error(const QString &message);
    QString m_root;
    PatreonAuth *m_patreon;
    McInstanceManager *m_mc;
    VersionManager *m_versions;
    AccountManager *m_accounts;
    JavaRunner *m_java;
    QNetworkAccessManager *m_images=nullptr;
    QHash<QString,QPixmap> m_imageCache;
    QList<McVersion> m_manifest;
    QListWidget *m_packs=nullptr,*m_results=nullptr,*m_lost=nullptr,*m_news=nullptr,*m_nav=nullptr,*m_accountList=nullptr;
    QStackedWidget *m_pages=nullptr;
    QPushButton *m_navSettings=nullptr,*m_sidebarUpdate=nullptr,*m_accountButton=nullptr,*m_instancePicker=nullptr;
    QLabel *m_accountName=nullptr,*m_accountType=nullptr,*m_accountAvatar=nullptr;
    QLabel *m_playerAvatar=nullptr;
    QLabel *m_pickerIcon=nullptr,*m_pickerName=nullptr,*m_pickerSub=nullptr,*m_playerName=nullptr,*m_playerType=nullptr;
    QLabel *m_status=nullptr,*m_packDetails=nullptr,*m_catalog=nullptr;
    QProgressBar *m_progress=nullptr;
    QComboBox *m_target=nullptr,*m_modProvider=nullptr;
    QLineEdit *m_query=nullptr,*m_lostFilter=nullptr;
    QStackedWidget *m_library=nullptr;InstanceGrid *m_grid=nullptr;InstanceDetail *m_detail=nullptr;QString m_selectedDir;
    QPushButton *m_playButton=nullptr,*m_lostPlay=nullptr,*m_lostInstall=nullptr,*m_lostUninstall=nullptr;
    QLabel *m_lostTitle=nullptr,*m_lostInfo=nullptr;QWidget *m_lostImage=nullptr;
    QHBoxLayout *m_homeNews=nullptr;
    QJsonArray m_packData,m_hits;
    QSet<QString> m_installing,m_launchAfterInstall;
    int m_jobs=0,m_offset=0;
};
