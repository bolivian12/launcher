#include "MsAuth.hpp"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QSettings>

// Register EBALIA as a public client; never impersonate another launcher's OAuth application.
static QString clientId() {
    auto id=qEnvironmentVariable("EBALIA_MS_CLIENT_ID");
    return id.isEmpty()?QSettings().value("auth/microsoftClientId").toString():id;
}
static const char kScope[]    = "XboxLive.SignIn XboxLive.offline_access";
static const char kDeviceCodeUrl[] =
    "https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode";
static const char kTokenUrl[] =
    "https://login.microsoftonline.com/consumers/oauth2/v2.0/token";

MsAuth::MsAuth(QObject *parent)
    : QObject(parent)
{
}

void MsAuth::cancel() { m_cancelled = true; }

void MsAuth::startLogin()
{
    m_cancelled = false;
    if(clientId().isEmpty()){emit loginFailed("Microsoft sign-in requires the EBALIA OAuth client ID (EBALIA_MS_CLIENT_ID). See README.md.");return;}
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), clientId());
    form.addQueryItem(QStringLiteral("scope"), QString::fromLatin1(kScope));

    QNetworkRequest req{QUrl(QString::fromLatin1(kDeviceCodeUrl))};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));

    QNetworkReply *reply = m_nam.post(req, form.toString(QUrl::FullyEncoded).toUtf8());
    emit statusUpdate(QStringLiteral("Requesting device code"));

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_cancelled) return;
        if (reply->error() != QNetworkReply::NoError) {
            emit loginFailed(QStringLiteral("Device code request failed: ") + reply->errorString());
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const QString deviceCode = o[QStringLiteral("device_code")].toString();
        const QString userCode   = o[QStringLiteral("user_code")].toString();
        const QString verifyUrl  = o[QStringLiteral("verification_uri")].toString();
        const int interval = o[QStringLiteral("interval")].toInt(5);
        const int expires  = o[QStringLiteral("expires_in")].toInt(900);
        if (deviceCode.isEmpty()) {
            emit loginFailed(QStringLiteral("No device code returned"));
            return;
        }
        emit showCode(userCode, verifyUrl);
        emit statusUpdate(QStringLiteral("Waiting for you in the browser"));
        pollToken(deviceCode, interval, expires);
    });
}

void MsAuth::pollToken(const QString &deviceCode, int intervalSecs, int expiresInSecs)
{
    if (m_cancelled || expiresInSecs <= 0) {
        if (!m_cancelled) emit loginFailed(QStringLiteral("Code expired"));
        return;
    }

    QTimer::singleShot(intervalSecs * 1000, this, [this, deviceCode, intervalSecs, expiresInSecs] {
        if (m_cancelled) return;

        QUrlQuery form;
        form.addQueryItem(QStringLiteral("client_id"), clientId());
        form.addQueryItem(QStringLiteral("grant_type"),
                          QStringLiteral("urn:ietf:params:oauth:grant-type:device_code"));
        form.addQueryItem(QStringLiteral("device_code"), deviceCode);

        QNetworkRequest req{QUrl(QString::fromLatin1(kTokenUrl))};
        req.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));

        QNetworkReply *reply = m_nam.post(req, form.toString(QUrl::FullyEncoded).toUtf8());
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, deviceCode, intervalSecs, expiresInSecs] {
            reply->deleteLater();
            if (m_cancelled) return;

            const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
            const QString err = o[QStringLiteral("error")].toString();

            if (err == QStringLiteral("authorization_pending") ||
                err == QStringLiteral("slow_down")) {
                pollToken(deviceCode, intervalSecs + (err.endsWith("slow_down") ? 2 : 0),
                          expiresInSecs - intervalSecs);
                return;
            }
            if (!err.isEmpty()) {
                emit loginFailed(QStringLiteral("Auth error: ") + err);
                return;
            }
            handleMsaToken(o, false);
        });
    });
}

void MsAuth::refresh(const QString &refreshToken)
{
    m_cancelled = false;
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), clientId());
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));
    form.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
    form.addQueryItem(QStringLiteral("scope"), QString::fromLatin1(kScope));

    QNetworkRequest req{QUrl(QString::fromLatin1(kTokenUrl))};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));

    QNetworkReply *reply = m_nam.post(req, form.toString(QUrl::FullyEncoded).toUtf8());
    emit statusUpdate(QStringLiteral("Refreshing Microsoft session"));

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_cancelled) return;
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        if (!o[QStringLiteral("error")].toString().isEmpty()) {
            emit loginFailed(QStringLiteral("Session refresh failed — log in again"));
            return;
        }
        handleMsaToken(o, true);
    });
}

void MsAuth::handleMsaToken(const QJsonObject &tokens, bool isRefresh)
{
    const QString msaToken = tokens[QStringLiteral("access_token")].toString();
    const QString refresh  = tokens[QStringLiteral("refresh_token")].toString();
    if (!refresh.isEmpty())
        m_refreshToken = refresh;
    if (msaToken.isEmpty()) {
        emit loginFailed(QStringLiteral("No Microsoft token"));
        return;
    }
    Q_UNUSED(isRefresh)
    emit statusUpdate(QStringLiteral("Xbox Live sign-in"));
    xboxUserAuth(msaToken);
}

void MsAuth::xboxUserAuth(const QString &msaToken)
{
    const QJsonObject body {
        { QStringLiteral("Properties"), QJsonObject{
            { QStringLiteral("AuthMethod"), QStringLiteral("RPS") },
            { QStringLiteral("SiteName"), QStringLiteral("user.auth.xboxlive.com") },
            { QStringLiteral("RpsTicket"), QStringLiteral("d=") + msaToken },
        }},
        { QStringLiteral("RelyingParty"), QStringLiteral("http://auth.xboxlive.com") },
        { QStringLiteral("TokenType"), QStringLiteral("JWT") },
    };

    QNetworkRequest req{QUrl(QStringLiteral("https://user.auth.xboxlive.com/user/authenticate"))};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply *reply = m_nam.post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_cancelled) return;
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const QString xbl = o[QStringLiteral("Token")].toString();
        if (xbl.isEmpty()) {
            emit loginFailed(QStringLiteral("Xbox Live auth failed"));
            return;
        }
        emit statusUpdate(QStringLiteral("XSTS authorization"));
        xstsAuthorize(xbl);
    });
}

void MsAuth::xstsAuthorize(const QString &xblToken)
{
    const QJsonObject body {
        { QStringLiteral("Properties"), QJsonObject{
            { QStringLiteral("SandboxId"), QStringLiteral("RETAIL") },
            { QStringLiteral("UserTokens"), QJsonArray{ xblToken } },
        }},
        { QStringLiteral("RelyingParty"), QStringLiteral("rp://api.minecraftservices.com/") },
        { QStringLiteral("TokenType"), QStringLiteral("JWT") },
    };

    QNetworkRequest req{QUrl(QStringLiteral("https://xsts.auth.xboxlive.com/xsts/authorize"))};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply *reply = m_nam.post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_cancelled) return;
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();

        // Xbox error codes (from Prism's XboxAuthorizationStep)
        const qint64 xerr = o[QStringLiteral("XErr")].toVariant().toLongLong();
        if (xerr == 2148916233LL) {
            emit loginFailed(QStringLiteral("This Microsoft account has no Xbox profile — create one at xbox.com first"));
            return;
        }
        if (xerr == 2148916238LL) {
            emit loginFailed(QStringLiteral("Child account — needs an adult family exception"));
            return;
        }
        if (xerr != 0) {
            emit loginFailed(QStringLiteral("XSTS error %1").arg(xerr));
            return;
        }

        const QString xsts = o[QStringLiteral("Token")].toString();
        const QJsonArray claims = o[QStringLiteral("DisplayClaims")].toObject()
                                      [QStringLiteral("xui")].toArray();
        const QString uhs = claims.isEmpty() ? QString()
            : claims.first().toObject()[QStringLiteral("uhs")].toString();
        if (xsts.isEmpty() || uhs.isEmpty()) {
            emit loginFailed(QStringLiteral("XSTS token missing"));
            return;
        }
        emit statusUpdate(QStringLiteral("Minecraft login"));
        mcLogin(uhs, xsts);
    });
}

void MsAuth::mcLogin(const QString &uhs, const QString &xstsToken)
{
    const QJsonObject body {
        { QStringLiteral("xtoken"),
          QStringLiteral("XBL3.0 x=%1;%2").arg(uhs, xstsToken) },
        { QStringLiteral("platform"), QStringLiteral("PC_LAUNCHER") },
    };

    QNetworkRequest req{QUrl(QStringLiteral("https://api.minecraftservices.com/launcher/login"))};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply *reply = m_nam.post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_cancelled) return;
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const QString mcToken = o[QStringLiteral("access_token")].toString();
        if (mcToken.isEmpty()) {
            emit loginFailed(QStringLiteral("Minecraft login failed"));
            return;
        }
        emit statusUpdate(QStringLiteral("Fetching profile"));
        fetchProfile(mcToken, m_refreshToken);
    });
}

void MsAuth::fetchProfile(const QString &mcToken, const QString &refreshToken)
{
    QNetworkRequest req{QUrl(QStringLiteral("https://api.minecraftservices.com/minecraft/profile"))};
    req.setRawHeader("Authorization", "Bearer " + mcToken.toUtf8());

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, mcToken, refreshToken] {
        reply->deleteLater();
        if (m_cancelled) return;

        if (reply->error() == QNetworkReply::ContentNotFoundError) {
            emit loginFailed(QStringLiteral("This account does not own Minecraft: Java Edition"));
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const QString uuid = o[QStringLiteral("id")].toString();
        const QString name = o[QStringLiteral("name")].toString();
        if (uuid.isEmpty() || name.isEmpty()) {
            emit loginFailed(QStringLiteral("Profile unreadable"));
            return;
        }
        emit loginDone(mcToken, refreshToken, uuid, name);
    });
}
