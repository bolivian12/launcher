#pragma once

#include <QObject>
#include <QString>

// License tiers: Free < Exclusive < Explicit < Senior
// Source of truth is the linked Patreon session (see PatreonAuth) —
// no keys, no .lic files. Tier + patron name persist in QSettings and
// are revalidated against Patreon on every launcher start.
class LicenseManager : public QObject {
    Q_OBJECT
public:
    enum class Tier { Free = 0, Exclusive = 1, Explicit = 2, Senior = 3 };

    explicit LicenseManager(QObject *parent = nullptr);

    Tier tier() const { return m_tier; }
    QString patronName() const { return m_patronName; }
    bool isLinked() const { return m_linked; }

    static QString tierName(Tier t);
    static Tier tierFromRank(int rank);
    static Tier tierFromString(const QString &s);
    static int tierRank(Tier t) { return static_cast<int>(t); }

public slots:
    void linkPatreon(const QString &patronName, int tierRank);
    void unlink();

signals:
    void licenseChanged();

private:
    void load();

    Tier m_tier = Tier::Free;
    QString m_patronName;
    bool m_linked = false;
};
