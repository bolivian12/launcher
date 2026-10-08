#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QPixmap>
#include <QImage>

struct McAccount {
    QString type;          // "offline" | "msa"
    QString name;
    QString uuid;
    QString mcToken;       // msa only
    QString refreshToken;  // msa only
};

// Persisted account list (dataDir/mc/accounts.json), Prism-style management.
class AccountManager : public QObject {
    Q_OBJECT
public:
    explicit AccountManager(const QString &mcDir, QObject *parent = nullptr);

    QList<McAccount> accounts() const { return m_accounts; }
    McAccount active() const;
    bool hasActive() const;

    void addAccount(const McAccount &acc);
    void removeAccount(const QString &uuid);
    void setActive(const QString &uuid);
    void updateTokens(const QString &uuid, const QString &mcToken,
                      const QString &refreshToken);

    // Profile pictures (dataDir/mc/avatars/<uuid>.png); accounts without one
    // use the initial-letter avatar.
    QString picturePath(const QString &uuid) const;
    bool hasPicture(const QString &uuid) const;
    void setPicture(const QString &uuid, const QImage &picture);
    QPixmap picture(const McAccount &account, int size) const;
    // A picture the player chose (avatars/<uuid>-custom.png): shown instead of the Microsoft or initial-letter one
    // and kept when Microsoft refreshes its picture.
    bool hasCustomPicture(const QString &uuid) const;
    void setCustomPicture(const QString &uuid, const QImage &picture);
    void clearCustomPicture(const QString &uuid);

signals:
    void accountsChanged();

private:
    void load();
    void save() const;

    QString m_path;
    QString m_dir;
    QList<McAccount> m_accounts;
    QString m_activeUuid;
};
