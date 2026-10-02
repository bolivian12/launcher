#pragma once
#include <QWidget>
#include <QJsonObject>
#include <functional>
class QLineEdit;class QListWidget;class QComboBox;class QLabel;class QTextBrowser;class QPushButton;class QNetworkAccessManager;
// Modpack search page embedded in the new-instance dialog, like Prism's provider pages.
class PackBrowser:public QWidget {
public:
    PackBrowser(const QString &root,const QString &provider,QWidget *parent=nullptr);
    void activate(); // first search happens when the page is opened, not when the dialog is built
    QString provider() const {return m_provider;}
    QJsonObject selectedPack,selectedVersion;
    std::function<void()> changed;
private:
    void lookup(bool append);
    void describe(const QJsonObject &pack);
    void loadIcon(const QString &url,const QString &id);
    QString m_root,m_provider,m_query;
    QLineEdit *m_search;QListWidget *m_results;QComboBox *m_versions;QLabel *m_status;QTextBrowser *m_description;QPushButton *m_find,*m_more;
    QNetworkAccessManager *m_icons=nullptr;QWidget *m_keyRow=nullptr;
    int m_searchEpoch=0,m_versionEpoch=0,m_page=0;bool m_started=false;
};
