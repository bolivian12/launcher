#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>

struct VersionInfo {
    QString id;
    QString name;
    QString category;
    QString description;
    QString downloadUrl;
    QString workingDir;
    QString launchCommand;
    QString launcher;       // WindowsMPP, UltimMC, WindowsMPRP
    QString requiredTier;   // free (default), senior, elite
    bool installed = false;
    QString archivePrefix,archiveSha256;
    qint64 archiveSize=0;
};

class VersionManager : public QObject {
    Q_OBJECT
public:
    explicit VersionManager(QObject *parent = nullptr);

    QList<VersionInfo> getVersions() const { return m_versions; }
    bool isVersionInstalled(const VersionInfo &v) const;
    QString getInstallPath(const VersionInfo &v) const;
    QString getDataDir() const;
    QString getVersionsDir() const;

public slots:
    void loadVersions();

signals:
    void versionsLoaded();

private:
    QList<VersionInfo> m_versions;
    QString m_dataDir;
};
