#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
class QWidget;
class QMessageBox;
class UpdateChecker:public QObject {
 Q_OBJECT
public:
 explicit UpdateChecker(QObject *parent=nullptr);
 void check();
 QString releaseUrl() const{return m_releaseUrl;}
 static QMessageBox *showUpdatePrompt(QWidget *parent,const QString &version);
 static bool newerStable(QString tag,const QString &current);
 static QString releasePage(const QJsonObject &release,bool gitgud);
signals:
 void updateAvailable(const QString &version);
 void upToDate();
 void checkFailed(const QString &error);
private:
 void fetch(bool gitgud);
 QNetworkAccessManager m_nam;
 QString m_releaseUrl;
 bool m_busy=false;
};
