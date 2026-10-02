#pragma once
#include <QMainWindow>
#include <QJsonArray>
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
class QTabWidget;
class QHBoxLayout;
class PatreonAuth;
class InstanceGrid;
class InstanceDetail;
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent=nullptr);
    void showPage(int index);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void build();
    void refreshInstances();
    void refreshPacks();
    void refreshAccounts();
    void refreshLost();
    void refreshNews(bool network=true);
    void selection();
    McInstance selected() const;
    void createInstance(bool copy=false,int page=0);
    void instanceMenu();
    void groupMenu(const QString &group);
    void changeGroup();
    void copyInstance();
    void exportInstance();
    void showLog(const QString &dir,QWidget *parent);
    void play();
    void editInstance();
    void removeInstance();
    void manageMods();
    void savePack();
    void applyPack();
    void preview(QJsonArray projects,QString dir);
    void searchMods(int offset=0);
    void account(bool microsoft);
    void lostAction(bool launch);
    void work(const QString &label,std::function<QJsonObject()> job,std::function<void(QJsonObject)> done);
    void error(const QString &message);
    QString m_root;
    PatreonAuth *m_patreon;
    McInstanceManager *m_mc;
    VersionManager *m_versions;
    AccountManager *m_accounts;
    JavaRunner *m_java;
    QList<McVersion> m_manifest;
    QListWidget *m_packs=nullptr,*m_results=nullptr,*m_lost=nullptr,*m_news=nullptr,*m_nav=nullptr;
    QStackedWidget *m_pages=nullptr;
    QLabel *m_playerName=nullptr,*m_status=nullptr,*m_packDetails=nullptr,*m_catalog=nullptr;
    QProgressBar *m_progress=nullptr;
    QComboBox *m_target=nullptr,*m_account=nullptr;
    QLineEdit *m_query=nullptr,*m_lostFilter=nullptr;
    QStackedWidget *m_library=nullptr;InstanceGrid *m_grid=nullptr;InstanceDetail *m_detail=nullptr;QString m_selectedDir;
    QComboBox *m_playInstance=nullptr,*m_modProvider=nullptr;
    QPushButton *m_playButton=nullptr;
    QTabWidget *m_clientTabs=nullptr;
    QHBoxLayout *m_homeNews=nullptr;
    QJsonArray m_packData,m_hits;
    QSet<QString> m_installing;
    int m_jobs=0,m_offset=0;
};
