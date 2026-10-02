#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonArray>
class QTimer;
class PatreonAuth : public QObject {
    Q_OBJECT
public:
    explicit PatreonAuth(QObject *parent=nullptr);
    bool isConfigured() const;
    bool hasTokens() const;
    bool verified() const {return m_verified;}
    QString patronName() const {return m_name;}
    QString tierTitle() const {return m_title;}
    QString status() const {return m_status;}
    QString authUrl() const {return m_authUrl;}
    QJsonArray posts() const {return m_posts;}
    static QString serviceUrl();
    void startLogin();
    void fetchIdentity(){refreshNews();}
    void refreshNews();
    void logout();
signals:
    void loginStarted();
    void loginFailed(const QString &error);
    void changed();
private:
    QString sessionKey() const;
    void clearView();
    QNetworkAccessManager m_nam;
    QTimer *m_timer;
    QString m_authUrl,m_name,m_title,m_status;
    QJsonArray m_posts;
    bool m_verified=false,m_busy=false;
    int m_generation=0;
    qint64 m_pendingUntil=0;
};
