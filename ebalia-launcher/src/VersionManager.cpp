#include "VersionManager.hpp"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QFileInfo>

VersionManager::VersionManager(QObject *parent)
    : QObject(parent)
{
    m_dataDir = qEnvironmentVariable("EBALIA_DATA_DIR");
    if (m_dataDir.isEmpty()) m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(m_dataDir);
    QDir().mkpath(m_dataDir + "/versions");
}

void VersionManager::loadVersions()
{
    QFile file(":/versions.json");
    if (!file.open(QIODevice::ReadOnly)) {
        emit versionsLoaded();
        return;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError) return;

    QJsonObject root = doc.object();
    QJsonArray arr = root["versions"].toArray();

    m_versions.clear();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        VersionInfo v;
        v.id          = obj["id"].toString();
        v.name        = obj["name"].toString();
        v.category    = obj["category"].toString();
        v.description = obj["description"].toString();
        v.downloadUrl = obj["download_url"].toString();
        v.archivePrefix=obj["archive_prefix"].toString();v.archiveSize=obj["archive_size"].toInteger();v.archiveSha256=obj["archive_sha256"].toString();
        v.workingDir  = obj["working_dir"].toString();
        v.launchCommand = obj["launch_command"].toString();
        v.launcher    = obj["launcher"].toString();
        v.requiredTier = obj["required_tier"].toString(QStringLiteral("free"));
        v.native      = obj["native"].toObject();
        v.installed   = isVersionInstalled(v);
        m_versions.append(v);
    }

    emit versionsLoaded();
}

bool VersionManager::isVersionInstalled(const VersionInfo &v) const
{
    QString path = getInstallPath(v);
    QDir dir(path);
    return QFile::exists(path + "/.installed.json");
}

QString VersionManager::getInstallPath(const VersionInfo &v) const
{
    return m_dataDir + "/versions/" + v.id;
}

QString VersionManager::getDataDir() const
{
    return m_dataDir;
}

QString VersionManager::getVersionsDir() const
{
    return m_dataDir + "/versions";
}
