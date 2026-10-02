#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QNetworkAccessManager>

struct NewsItem {
    QString title;
    QString date;
    QString excerpt;
    QString url;
};

// News feed. Bundled :/news.json loads instantly; refresh() pulls the live
// feed from the remote news.json (dj posts dev leaks / announcements there)
// and emits newsLoaded() when newer content arrives.
class NewsManager : public QObject {
    Q_OBJECT
public:
    explicit NewsManager(QObject *parent = nullptr);

    QList<NewsItem> items() const { return m_items; }
    bool isLive() const { return m_live; }

    void refresh();

signals:
    void newsLoaded();

private:
    void parse(const QByteArray &data, bool live);

    QList<NewsItem> m_items;
    QNetworkAccessManager m_nam;
    bool m_live = false;
};
