#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

// Checks the EBALIA freever GitHub repo for a newer launcher build.
// Port of the legacy Bexe16.4 updater (updaterfree.ps1) check logic.
//
// The upstream release has no semver tags, so "new version" means:
// the release asset's updated_at differs from the one we last saw/applied
// (stored in QSettings under update/seen_stamp).
class UpdateChecker : public QObject {
    Q_OBJECT
public:
    explicit UpdateChecker(QObject *parent = nullptr);

    void check();

    QString releaseUrl() const { return m_releaseUrl; }
    QString assetUrl() const { return m_assetUrl; }
    QString assetName() const { return m_assetName; }

    // Call after a successful update download so we stop nagging.
    void markUpdated();

signals:
    void updateAvailable(const QString &label);
    void upToDate();
    void checkFailed(const QString &error);

private:
    QNetworkAccessManager m_nam;
    QString m_releaseUrl;
    QString m_assetUrl;
    QString m_assetName;
    QString m_stamp;
};
