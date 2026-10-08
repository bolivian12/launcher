#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
class AccountManager;
class McInstanceManager;
class QLabel;
class QListWidget;
class SkinsPage : public QWidget {
public:
    using Work=std::function<void(const QString &,std::function<QJsonObject()>,std::function<void(QJsonObject)>)>;
    SkinsPage(QString root,AccountManager *accounts,McInstanceManager *instances,Work work,QWidget *parent=nullptr);
private:
    void refresh();void showSkin();void showDefaultSkin(const QString &name,const QString &uuid);void importSkin();void apply(bool local);
    QString m_root;AccountManager *m_accounts;McInstanceManager *m_instances;Work m_work;
    QListWidget *m_list;QLabel *m_preview,*m_details;QJsonArray m_skins;bool m_back=false;
};
