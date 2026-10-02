#pragma once

#include <QObject>
#include <QString>
#include <QList>

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

signals:
    void accountsChanged();

private:
    void load();
    void save() const;

    QString m_path;
    QList<McAccount> m_accounts;
    QString m_activeUuid;
};
