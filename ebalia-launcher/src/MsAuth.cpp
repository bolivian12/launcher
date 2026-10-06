#include "MsAuth.hpp"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QSettings>

#include "MicrosoftConfig.hpp"
#include "Language.hpp"
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QTcpSocket>
#include <QDesktopServices>
#include <QPainter>
#include <memory>

// Public application identifier, not a client secret. Credentials stay at Microsoft.
static QString clientId() {
    auto id=qEnvironmentVariable("EBALIA_MS_CLIENT_ID").trimmed();
    if(id.isEmpty()) id=QString::fromUtf8(EBALIA_MS_CLIENT_ID).trimmed();
    if(id.isEmpty()) id=QSettings().value("auth/microsoftClientId").toString().trimmed();
    return id;
}
static const char kScope[] = "XboxLive.SignIn XboxLive.offline_access";
static const char kTokenUrl[] = "https://login.microsoftonline.com/consumers/oauth2/v2.0/token";
static QString randomToken() {
    QByteArray bytes(32, Qt::Uninitialized);
    for(int i=0;i<bytes.size();++i) bytes[i]=char(QRandomGenerator::system()->generate() & 255);
    return QString::fromLatin1(bytes.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
}
MsAuth::MsAuth(QObject *parent):QObject(parent) {
    m_nam.setTransferTimeout(60000);
    m_loginTimeout.setSingleShot(true);
    connect(&m_loginTimeout,&QTimer::timeout,this,[this]{cancel();emit loginFailed(Language::text("El inicio de sesión venció. Volvé a intentarlo.","Sign-in timed out. Please try again.","O login expirou. Tente novamente."));});
    connect(&m_callback,&QTcpServer::newConnection,this,&MsAuth::receiveCallback);
}
void MsAuth::cancel() {
    m_cancelled=true;m_loginTimeout.stop();m_callback.close();m_state.clear();m_verifier.clear();
    for(auto reply:m_nam.findChildren<QNetworkReply*>()) reply->abort();
}
void MsAuth::startLogin() {
    cancel();m_cancelled=false;m_withPicture=true;m_refreshToken.clear();m_clientId=clientId();
    if(m_clientId.isEmpty()) {
        emit loginFailed(Language::text("El acceso Microsoft todavía no está habilitado en esta compilación de EBALIA. No necesitás configurar claves personales.","Microsoft sign-in is not enabled in this EBALIA build yet. You do not need to configure personal keys.","O acesso Microsoft ainda não está habilitado nesta versão do EBALIA. Você não precisa configurar chaves pessoais."));return;
    }
    if(!m_callback.listen(QHostAddress::LocalHost,0)) {
        emit loginFailed(Language::text("No se pudo preparar el inicio de sesión local.","Could not prepare the local sign-in callback.","Não foi possível preparar o retorno local do login."));return;
    }
    m_redirect=QString("http://localhost:%1/").arg(m_callback.serverPort());
    m_state=randomToken();m_verifier=randomToken();
    const auto challenge=QCryptographicHash::hash(m_verifier.toLatin1(),QCryptographicHash::Sha256).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    QUrl url("https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize");QUrlQuery query;
    query.addQueryItem("client_id",m_clientId);query.addQueryItem("response_type","code");query.addQueryItem("redirect_uri",m_redirect);
    query.addQueryItem("scope",kScope);query.addQueryItem("state",m_state);query.addQueryItem("code_challenge",QString::fromLatin1(challenge));
    query.addQueryItem("code_challenge_method","S256");query.addQueryItem("response_mode","query");query.addQueryItem("prompt","select_account");url.setQuery(query);
    m_loginTimeout.start(10*60*1000);
    emit statusUpdate(Language::text("Continuá en Microsoft. Volverás automáticamente al launcher.","Continue at Microsoft. You will return to the launcher automatically.","Continue na Microsoft. Você voltará automaticamente ao launcher."));
    if(!QDesktopServices::openUrl(url)) {cancel();emit loginFailed(Language::text("No se pudo abrir el navegador.","Could not open the browser.","Não foi possível abrir o navegador."));}
}
void MsAuth::receiveCallback() {
    while(m_callback.hasPendingConnections()) {
        auto socket=m_callback.nextPendingConnection();socket->setParent(this);
        socket->setReadBufferSize(16385);
        connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        QTimer::singleShot(10000,socket,[socket]{socket->disconnectFromHost();});
        connect(socket,&QTcpSocket::readyRead,this,[this,socket]{
            auto bytes=socket->property("httpBuffer").toByteArray()+socket->readAll();
            if(bytes.size()>16384){socket->disconnectFromHost();return;}
            if(!bytes.contains("\r\n\r\n")){socket->setProperty("httpBuffer",bytes);return;}
            if(socket->property("handled").toBool())return;socket->setProperty("handled",true);
            const auto line=bytes.left(bytes.indexOf("\r\n")).split(' ');
            const QUrl target=line.size()==3?QUrl::fromEncoded(line[1]):QUrl();const QUrlQuery q(target);
            const bool valid=!m_cancelled&&!m_state.isEmpty()&&line.value(0)=="GET"&&target.isRelative()&&target.path()=="/"&&q.queryItemValue("state")==m_state;
            const auto code=q.queryItemValue("code");const auto error=q.queryItemValue("error");
            const bool complete=valid&&(!code.isEmpty()||!error.isEmpty());
            const QByteArray body=(complete?Language::text("Volvé a EBALIA para completar el acceso. Ya podés cerrar esta pestaña.","Return to EBALIA to finish signing in. You can close this tab.","Volte ao EBALIA para concluir o login. Você pode fechar esta aba."):QString("Invalid sign-in callback.")).toUtf8();
            socket->write(QByteArray(complete?"HTTP/1.1 200 OK\r\n":"HTTP/1.1 400 Bad Request\r\n")+"Content-Type: text/plain; charset=utf-8\r\nCache-Control: no-store\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);socket->disconnectFromHost();
            if(!complete)return;
            m_callback.close();m_state.clear();
            if(!error.isEmpty()){cancel();emit loginFailed(Language::text("No se autorizó el acceso con Microsoft.","Microsoft sign-in was not authorized.","O acesso Microsoft não foi autorizado."));return;}
            exchangeCode(code);
        });
    }
}
void MsAuth::exchangeCode(const QString &code) {
    QUrlQuery form;form.addQueryItem("client_id",m_clientId);form.addQueryItem("grant_type","authorization_code");
    form.addQueryItem("code",code);form.addQueryItem("redirect_uri",m_redirect);form.addQueryItem("code_verifier",m_verifier);form.addQueryItem("scope",kScope);
    QNetworkRequest request{QUrl(kTokenUrl)};request.setHeader(QNetworkRequest::ContentTypeHeader,"application/x-www-form-urlencoded");
    auto reply=m_nam.post(request,form.toString(QUrl::FullyEncoded).toUtf8());m_verifier.clear();
    connect(reply,&QNetworkReply::finished,this,[this,reply]{reply->deleteLater();if(m_cancelled)return;
        if(reply->error()!=QNetworkReply::NoError){m_loginTimeout.stop();emit loginFailed(Language::text("Microsoft no pudo completar el acceso. Volvé a intentarlo.","Microsoft could not complete sign-in. Please try again.","A Microsoft não conseguiu concluir o login. Tente novamente."));return;}
        handleMsaToken(QJsonDocument::fromJson(reply->readAll()).object(),false);
    });
}

void MsAuth::refresh(const QString &refreshToken, bool withPicture)
{
    m_cancelled = false;
    m_withPicture = withPicture;
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
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("x-xbl-contract-version", "1");

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
        m_xblToken = xbl;
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
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("x-xbl-contract-version", "1");

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
    req.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = m_nam.post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (m_cancelled) return;
        const auto response = reply->readAll();
        const QJsonObject o = QJsonDocument::fromJson(response).object();
        const QString mcToken = o[QStringLiteral("access_token")].toString();
        if (mcToken.isEmpty()) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QString detail = o[QStringLiteral("errorMessage")].toString();
            if (detail.isEmpty()) detail = o[QStringLiteral("error")].toString();
            if (detail.isEmpty()) detail = o[QStringLiteral("developerMessage")].toString();
            if (status == 403) {
                detail = Language::text("Minecraft Services rechazó el Id. de aplicación (HTTP 403). El cliente debe ser aprobado en aka.ms/AppRegInfo; el registro de Entra por sí solo no habilita el acceso.","Minecraft Services rejected the application ID (HTTP 403). The client must be approved at aka.ms/AppRegInfo; an Entra registration alone does not enable access.","O Minecraft Services recusou o ID do aplicativo (HTTP 403). O cliente precisa ser aprovado em aka.ms/AppRegInfo; o registro no Entra sozinho não libera o acesso.");
            } else if (status == 401) {
                detail = detail.isEmpty() ? Language::text("La cuenta no tiene acceso a Minecraft Java o la licencia no está disponible.","This account has no access to Minecraft: Java Edition or the license is unavailable.","A conta não tem acesso ao Minecraft Java ou a licença não está disponível.") : detail;
            }
            if (detail.isEmpty()) detail = Language::text("Respuesta HTTP %1","HTTP response %1","Resposta HTTP %1").arg(status);
            emit loginFailed(Language::text("Minecraft rechazó el acceso: %1","Minecraft refused sign-in: %1","O Minecraft recusou o acesso: %1").arg(detail));
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
        QString skinUrl;
        for (const QJsonValue &skin : o[QStringLiteral("skins")].toArray())
            if (skin[QStringLiteral("state")].toString() == QStringLiteral("ACTIVE"))
                skinUrl = skin[QStringLiteral("url")].toString();
        auto finish = [this, mcToken, refreshToken, uuid, name, attempt = ++m_attempt] {
            if (m_cancelled || attempt != m_attempt) return;
            ++m_attempt;
            m_loginTimeout.stop();
            emit loginDone(mcToken, refreshToken, uuid, name);
        };
        if (!m_withPicture) { finish(); return; }
        emit statusUpdate(QStringLiteral("Fetching profile picture"));
        // The picture is optional: never hold the sign-in back for long.
        QTimer::singleShot(10000, this, finish);
        fetchPicture(uuid, skinUrl, finish);
    });
}

namespace {
QNetworkRequest jsonRequest(const QUrl &url)
{
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    req.setRawHeader("Accept", "application/json");
    return req;
}
// Front of the head plus the hat layer, enlarged without smoothing.
QImage skinFace(const QByteArray &png)
{
    QImage skin;
    if (!skin.loadFromData(png) || skin.width() < 64 || skin.height() < 32 || skin.width() % 64) return {};
    const int u = skin.width() / 64;
    QImage face(8, 8, QImage::Format_ARGB32_Premultiplied);
    face.fill(Qt::transparent);
    QPainter p(&face);
    p.drawImage(QRect(0, 0, 8, 8), skin, QRect(8 * u, 8 * u, 8 * u, 8 * u));
    p.drawImage(QRect(0, 0, 8, 8), skin, QRect(40 * u, 8 * u, 8 * u, 8 * u));
    p.end();
    return face.scaled(128, 128, Qt::IgnoreAspectRatio, Qt::FastTransformation);
}
}

void MsAuth::fetchPicture(const QString &uuid, const QString &skinUrl, std::function<void()> done)
{
    if (m_xblToken.isEmpty()) { skinPicture(uuid, skinUrl, done); return; }
    // A second XSTS token, for Xbox Live itself, reads the account's gamerpic.
    const QJsonObject body {
        { QStringLiteral("Properties"), QJsonObject{
            { QStringLiteral("SandboxId"), QStringLiteral("RETAIL") },
            { QStringLiteral("UserTokens"), QJsonArray{ m_xblToken } },
        }},
        { QStringLiteral("RelyingParty"), QStringLiteral("http://xboxlive.com") },
        { QStringLiteral("TokenType"), QStringLiteral("JWT") },
    };
    auto req = jsonRequest(QUrl(QStringLiteral("https://xsts.auth.xboxlive.com/xsts/authorize")));
    req.setRawHeader("x-xbl-contract-version", "1");
    QNetworkReply *reply = m_nam.post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, uuid, skinUrl, done] {
        reply->deleteLater();
        if (m_cancelled) return;
        const QJsonObject o = QJsonDocument::fromJson(reply->readAll()).object();
        const QString token = o[QStringLiteral("Token")].toString();
        const QJsonArray claims = o[QStringLiteral("DisplayClaims")].toObject()[QStringLiteral("xui")].toArray();
        const QString uhs = claims.isEmpty() ? QString() : claims.first()[QStringLiteral("uhs")].toString();
        if (token.isEmpty() || uhs.isEmpty()) { skinPicture(uuid, skinUrl, done); return; }
        auto profile = jsonRequest(QUrl(QStringLiteral("https://profile.xboxlive.com/users/me/profile/settings?settings=GameDisplayPicRaw")));
        profile.setRawHeader("x-xbl-contract-version", "2");
        profile.setRawHeader("Authorization", QStringLiteral("XBL3.0 x=%1;%2").arg(uhs, token).toUtf8());
        QNetworkReply *settings = m_nam.get(profile);
        connect(settings, &QNetworkReply::finished, this, [this, settings, uuid, skinUrl, done] {
            settings->deleteLater();
            if (m_cancelled) return;
            const QJsonArray users = QJsonDocument::fromJson(settings->readAll()).object()[QStringLiteral("profileUsers")].toArray();
            QString picture;
            for (const QJsonValue &setting : users.isEmpty() ? QJsonArray() : users.first()[QStringLiteral("settings")].toArray())
                if (setting[QStringLiteral("id")].toString() == QStringLiteral("GameDisplayPicRaw"))
                    picture = setting[QStringLiteral("value")].toString();
            QUrl url(picture);
            if (url.scheme() != QStringLiteral("https") || !url.host().endsWith(QStringLiteral(".xboxlive.com"))) {
                skinPicture(uuid, skinUrl, done);
                return;
            }
            QUrlQuery query(url);
            query.removeAllQueryItems(QStringLiteral("format"));
            query.addQueryItem(QStringLiteral("format"), QStringLiteral("png"));
            query.addQueryItem(QStringLiteral("w"), QStringLiteral("128"));
            query.addQueryItem(QStringLiteral("h"), QStringLiteral("128"));
            url.setQuery(query);
            QNetworkReply *image = m_nam.get(QNetworkRequest(url));
            connect(image, &QNetworkReply::finished, this, [this, image, uuid, skinUrl, done] {
                image->deleteLater();
                if (m_cancelled) return;
                QImage picture;
                if (image->error() != QNetworkReply::NoError || image->size() > 4 * 1024 * 1024 || !picture.loadFromData(image->readAll())) {
                    skinPicture(uuid, skinUrl, done);
                    return;
                }
                emit profilePicture(uuid, picture.scaled(128, 128, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
                done();
            });
        });
    });
}

void MsAuth::skinPicture(const QString &uuid, const QString &skinUrl, std::function<void()> done)
{
    QUrl url(skinUrl);
    if (url.scheme() == QStringLiteral("http")) url.setScheme(QStringLiteral("https"));
    if (url.scheme() != QStringLiteral("https") || url.host() != QStringLiteral("textures.minecraft.net")) { done(); return; }
    QNetworkReply *reply = m_nam.get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, uuid, done] {
        reply->deleteLater();
        if (m_cancelled) return;
        const QImage face = reply->error() == QNetworkReply::NoError && reply->size() < 1024 * 1024 ? skinFace(reply->readAll()) : QImage();
        if (!face.isNull()) emit profilePicture(uuid, face);
        done();
    });
}
