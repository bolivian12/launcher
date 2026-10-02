#pragma once
#include <QDialog>
#include <QJsonArray>
#include "McInstanceManager.hpp"
class QLineEdit;class QComboBox;class QTreeWidget;class QListWidget;class QCheckBox;class QLabel;class QSpinBox;class QDialogButtonBox;
class CreateInstanceDialog:public QDialog {
public:
    CreateInstanceDialog(McInstanceManager *manager,const QList<McVersion> &catalog,const QJsonArray &packs,const McInstance &source,bool copy,QWidget *parent=nullptr);
    QJsonObject configuration() const;
private:
    void populateVersions();
    void fetchLoaders();
    void validate();
    McInstanceManager *m_manager;
    QList<McVersion> m_catalog;
    QLineEdit *m_name,*m_search,*m_loaderSearch;
    QComboBox *m_group,*m_pack,*m_icon;
    QTreeWidget *m_versions;
    QListWidget *m_loaders;
    QLabel *m_status,*m_loaderStatus;
    QSpinBox *m_memory;
    QDialogButtonBox *m_buttons=nullptr;
    QHash<QString,QCheckBox*> m_types;
    QString m_loader="vanilla";
    QJsonObject m_import,m_providerPack,m_providerVersion;
    int m_request=0;
    bool m_loading=false;
};
