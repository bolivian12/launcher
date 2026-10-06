#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

#include <QTcpServer>
#include <QTimer>
#include <QImage>
#include <functional>

// System-browser authorization code flow with PKCE and loopback callback.
class MsAuth : public QObject {
    Q_OBJECT
public:
    explicit MsAuth(QObject *parent = nullptr);

    void startLogin();
    void cancel();

    // Silent re-auth of a stored account. withPicture also downloads the profile picture.
    void refresh(const QString &refreshToken, bool withPicture = false);

signals:
    void statusUpdate(const QString &stage);
    void loginFailed(const QString &error);
    void loginDone(const QString &mcToken, const QString &refreshToken,
                   const QString &uuid, const QString &name);
    // Emitted before loginDone: the Xbox profile picture of the Microsoft
    // account, or the face of the Minecraft skin when there is none.
    void profilePicture(const QString &uuid, const QImage &picture);

private:
    void receiveCallback();
    void exchangeCode(const QString &code);
    void handleMsaToken(const QJsonObject &tokens, bool isRefresh);
    void xboxUserAuth(const QString &msaToken);
    void xstsAuthorize(const QString &xblToken);
    void mcLogin(const QString &uhs, const QString &xstsToken);
    void fetchProfile(const QString &mcToken, const QString &refreshToken);
    void fetchPicture(const QString &uuid, const QString &skinUrl, std::function<void()> done);
    void skinPicture(const QString &uuid, const QString &skinUrl, std::function<void()> done);

    QNetworkAccessManager m_nam;
    QTcpServer m_callback;
    QTimer m_loginTimeout;
    QString m_state, m_verifier, m_redirect, m_clientId;
    bool m_cancelled = false;
    QString m_refreshToken;
    QString m_xblToken;
    bool m_withPicture = true;
    int m_attempt = 0;
};
