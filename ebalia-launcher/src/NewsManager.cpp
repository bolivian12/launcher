#include "NewsManager.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QNetworkReply>

// Post news by editing this file in the GitHub repo — the launcher picks it
// up live on every start (and on each membership change).
static const char kNewsUrl[] =
    "https://raw.githubusercontent.com/HeelXel/Launcher/main/news.json";

NewsManager::NewsManager(QObject *parent)
    : QObject(parent)
{
    QFile file(QStringLiteral(":/news.json"));
    if (file.open(QIODevice::ReadOnly))
        parse(file.readAll(), false);
}

void NewsManager::refresh()
{
    QNetworkRequest req(QUrl(QString::fromLatin1(kNewsUrl)));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("EBALIA-Launcher/3.0"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return; // keep bundled/last feed silently
        parse(reply->readAll(), true);
    });
}

void NewsManager::parse(const QByteArray &data, bool live)
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    const QJsonArray arr = doc.object()[QStringLiteral("news")].toArray();
    if (arr.isEmpty())
        return;

    QList<NewsItem> items;
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        NewsItem n;
        n.title   = obj[QStringLiteral("title")].toString();
        n.date    = obj[QStringLiteral("date")].toString();
        n.excerpt = obj[QStringLiteral("excerpt")].toString();
        n.url     = obj[QStringLiteral("url")].toString();
        items.append(n);
    }

    m_items = items;
    m_live  = live;
    emit newsLoaded();
}
