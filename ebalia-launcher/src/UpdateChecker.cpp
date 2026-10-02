#include "UpdateChecker.hpp"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>

static const char kRepoApi[] =
    "https://api.github.com/repos/ralphdepriestdepriest2/freever/releases/latest";

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
{
}

void UpdateChecker::check()
{
    QNetworkRequest req(QUrl(QString::fromLatin1(kRepoApi)));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("EBALIA-Launcher/3.0"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit checkFailed(reply->errorString());
            return;
        }

        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = obj[QStringLiteral("tag_name")].toString();
        m_releaseUrl = obj[QStringLiteral("html_url")].toString();
        m_stamp      = obj[QStringLiteral("published_at")].toString();

        const QJsonArray assets = obj[QStringLiteral("assets")].toArray();
        for (const QJsonValue &av : assets) {
            const QJsonObject a = av.toObject();
            const QString name = a[QStringLiteral("name")].toString();
            if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
                m_assetName = name;
                m_assetUrl  = a[QStringLiteral("browser_download_url")].toString();
                // Asset re-uploads bump updated_at — that's our freshness signal
                const QString assetStamp =
                    a[QStringLiteral("updated_at")].toString();
                if (!assetStamp.isEmpty())
                    m_stamp = assetStamp;
                break;
            }
        }

        if (m_stamp.isEmpty()) {
            emit checkFailed(QStringLiteral("No release info on GitHub"));
            return;
        }

        QSettings s(QStringLiteral("EBALIA"), QStringLiteral("EBALIA Launcher"));
        const QString seen = s.value(QStringLiteral("update/seen_stamp")).toString();

        if (seen.isEmpty()) {
            // First run ever: record baseline silently, don't nag
            s.setValue(QStringLiteral("update/seen_stamp"), m_stamp);
            emit upToDate();
        } else if (seen != m_stamp) {
            emit updateAvailable(tag.isEmpty() ? QStringLiteral("new build") : tag);
        } else {
            emit upToDate();
        }
    });
}

void UpdateChecker::markUpdated()
{
    if (m_stamp.isEmpty()) return;
    QSettings s(QStringLiteral("EBALIA"), QStringLiteral("EBALIA Launcher"));
    s.setValue(QStringLiteral("update/seen_stamp"), m_stamp);
}
