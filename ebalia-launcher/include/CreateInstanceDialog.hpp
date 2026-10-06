#pragma once
#include <QImage>
#include <QDialog>
#include <QJsonArray>
#include "McInstanceManager.hpp"
class QLineEdit;class QComboBox;class QTreeWidget;class QListWidget;class QCheckBox;class QLabel;class QSpinBox;class QDialogButtonBox;class QStackedWidget;class QToolButton;class PackBrowser;
// Laid out like Prism Launcher's "New instance" window: name and group on top, sources on the left, the chosen source on the right.
class CreateInstanceDialog:public QDialog {
public:
    // The picture chosen from the computer, when configuration()["icon"] is "custom".
    QImage customIcon() const {return m_customIcon;}
    enum Page{Custom,Import,ATLauncher,CurseForge,FTB,FTBLegacy,FTBApp,Modrinth,Technic};
    CreateInstanceDialog(McInstanceManager *manager,const QList<McVersion> &catalog,const QJsonArray &packs,const McInstance &source,bool copy,QWidget *parent=nullptr);
    QJsonObject configuration() const;
    void showPage(int page);
    void setImportPath(const QString &path);
protected:
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
private:
    QWidget *customPage(const QJsonArray &packs,bool copy);
    QWidget *importPage();
    QWidget *ftbAppPage();
    void populateVersions();
    void fetchLoaders();
    void listLocalInstances();
    void validate();
    QString suggestedName() const;
    McInstanceManager *m_manager;
    QList<McVersion> m_catalog;
    QLineEdit *m_name,*m_search,*m_loaderSearch,*m_importPath,*m_ftbFolder;
    QComboBox *m_group,*m_pack;
    QToolButton *m_icon;
    QListWidget *m_sources,*m_loaders,*m_found,*m_ftbList;
    QStackedWidget *m_pages;
    QTreeWidget *m_versions;
    QLabel *m_status,*m_loaderStatus,*m_importStatus,*m_ftbStatus;
    QSpinBox *m_memory;
    QDialogButtonBox *m_buttons=nullptr;
    QHash<QString,QCheckBox*> m_types;
    QHash<int,PackBrowser*> m_browsers;
    QString m_loader="vanilla",m_iconKey="grass";
    QImage m_customIcon;
    int m_request=0;
    bool m_loading=false,m_localListed=false;
};
