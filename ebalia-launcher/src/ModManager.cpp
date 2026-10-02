#include "ModManager.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

ModManager::ModManager(QObject *parent)
    : QObject(parent)
{
    loadMods();
}

void ModManager::loadMods()
{
    QFile file(QStringLiteral(":/mods.json"));
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray arr = doc.object()[QStringLiteral("mods")].toArray();

    m_mods.clear();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        ModInfo m;
        m.id          = obj[QStringLiteral("id")].toString();
        m.name        = obj[QStringLiteral("name")].toString();
        m.author      = obj[QStringLiteral("author")].toString();
        m.description = obj[QStringLiteral("description")].toString();
        m.icon        = obj[QStringLiteral("icon")].toString();

        const QJsonArray links = obj[QStringLiteral("links")].toArray();
        for (const QJsonValue &lv : links) {
            QJsonObject lo = lv.toObject();
            ModLink link;
            link.label = lo[QStringLiteral("label")].toString();
            link.url   = lo[QStringLiteral("url")].toString();
            if (!link.url.isEmpty())
                m.links.append(link);
        }
        m_mods.append(m);
    }
}
