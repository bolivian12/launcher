#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

// Microsoft → Xbox Live → XSTS → Minecraft auth chain, modeled on
// PrismLauncher's MSADeviceCodeStep → XboxUserStep → XboxAuthorizationStep
// → LauncherLoginStep → MinecraftProfileStep.
//
// Flow: user gets a short code, opens microsoft.com/link, enters it.
// We poll until they finish, then walk the token chain.
class MsAuth : public QObject {
    Q_OBJECT
public:
    explicit MsAuth(QObject *parent = nullptr);

    void startLogin();
    void cancel();

    // Silent re-auth of a stored account
    void refresh(const QString &refreshToken);

signals:
    void showCode(const QString &userCode, const QString &verifyUrl);
    void statusUpdate(const QString &stage);
    void loginFailed(const QString &error);
    void loginDone(const QString &mcToken, const QString &refreshToken,
                   const QString &uuid, const QString &name);

private:
    void pollToken(const QString &deviceCode, int intervalSecs, int expiresInSecs);
    void handleMsaToken(const QJsonObject &tokens, bool isRefresh);
    void xboxUserAuth(const QString &msaToken);
    void xstsAuthorize(const QString &xblToken);
    void mcLogin(const QString &uhs, const QString &xstsToken);
    void fetchProfile(const QString &mcToken, const QString &refreshToken);

    QNetworkAccessManager m_nam;
    bool m_cancelled = false;
    QString m_refreshToken;
};
