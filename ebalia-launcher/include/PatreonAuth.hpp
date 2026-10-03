#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonArray>
#include <QStringList>
#include <QPointer>
class QTimer;
class PatreonAuth : public QObject {
    Q_OBJECT
public:
    explicit PatreonAuth(QObject *parent=nullptr);
    bool isConfigured() const;
    bool hasTokens() const;
    bool verified() const {return m_verified;}
    bool paidMember() const {return m_verified&&m_paid;}
    QString patronName() const {return m_name;}
    QString tierTitle() const {return m_title;}
    QStringList tierIds() const {return paidMember()?m_tierIds:QStringList{};}
    QString status() const {return m_status;}
    QString authUrl() const {return m_authUrl;}
    QJsonArray posts() const {return m_posts;}
    QJsonArray availableTiers() const {return m_availableTiers;}
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
    friend class UiTests; // Test fixtures can exercise membership transitions without a production bypass.
    QString sessionKey() const;
    void clearView();
    void watchUpdates();
    QNetworkAccessManager m_nam;
    QTimer *m_timer;
    QString m_authUrl,m_name,m_title,m_status;
    QStringList m_tierIds;
    QJsonArray m_posts,m_availableTiers;
    bool m_verified=false,m_paid=false,m_busy=false,m_refreshQueued=false;
    QPointer<QNetworkReply> m_events;
    QString m_revision;
    int m_generation=0;
    qint64 m_pendingUntil=0;
};
