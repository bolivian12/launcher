#pragma once
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrl>
#include <functional>

// All network operations run in the caller's worker thread; no GUI objects here.
class ModRepository {
public:
    using Transport = std::function<QByteArray(const QUrl &)>;
    explicit ModRepository(QString root, Transport transport = {});
    QJsonArray search(const QString &query, const QString &game, const QString &loader, int offset = 0, const QString &provider = "modrinth");
    QJsonObject plan(const QJsonArray &projects, const QString &game, const QString &loader, const QJsonArray &installed = {});
    void apply(const QString &instance, const QJsonObject &plan);
    QJsonObject capture(const QString &instance, const QString &name);
    QJsonArray packs() const;
    QString savePack(const QJsonObject &pack);
    static QJsonObject read(const QString &path);
    static void write(const QString &path, const QJsonObject &object);
    static bool safeName(const QString &name);
    static QByteArray fetch(const QUrl &url, const QMap<QByteArray,QByteArray> &headers = {});
    // EBALIA_CURSEFORGE_API_KEY, then Provider settings, then the key built into this copy (empty when none).
    static QString curseForgeKey();
private:
    QByteArray get(const QUrl &url);
    QJsonDocument api(const QString &path, const QList<QPair<QString, QString>> &query = {});
    QJsonDocument curse(const QString &path, const QList<QPair<QString,QString>> &query = {});
    QJsonObject curseVersion(const QJsonObject &file, const QString &game, const QString &loader);
    void resolve(const QString &project, const QString &version, const QString &game,
                 const QString &loader, QMap<QString, QJsonObject> &selected, QSet<QString> &visiting);
    QString m_root;
    Transport m_transport;
};
