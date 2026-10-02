#pragma once

#include <QObject>
#include <QString>
#include <QList>

struct ModLink {
    QString label;
    QString url;
};

struct ModInfo {
    QString id;
    QString name;
    QString author;
    QString description;
    QString icon;   // resource path, e.g. "mods/geckolib.png"
    QList<ModLink> links;
};

// Loads the curated mod list from the embedded resources (:/mods.json).
// Mirrors the mods section of the legacy EBALIA launcher.
class ModManager : public QObject {
    Q_OBJECT
public:
    explicit ModManager(QObject *parent = nullptr);

    QList<ModInfo> getMods() const { return m_mods; }

private:
    void loadMods();

    QList<ModInfo> m_mods;
};
