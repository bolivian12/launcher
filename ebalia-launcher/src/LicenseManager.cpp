#include "LicenseManager.hpp"

#include <QSettings>

LicenseManager::LicenseManager(QObject *parent)
    : QObject(parent)
{
    load();
}

QString LicenseManager::tierName(Tier t)
{
    switch (t) {
    case Tier::Exclusive: return QStringLiteral("EXCLUSIVE");
    case Tier::Explicit:  return QStringLiteral("EXPLICIT");
    case Tier::Senior:    return QStringLiteral("SENIOR");
    default:              return QStringLiteral("FREE");
    }
}

LicenseManager::Tier LicenseManager::tierFromRank(int rank)
{
    switch (rank) {
    case 1:  return Tier::Exclusive;
    case 2:  return Tier::Explicit;
    case 3:  return Tier::Senior;
    default: return Tier::Free;
    }
}

LicenseManager::Tier LicenseManager::tierFromString(const QString &s)
{
    const QString v = s.trimmed().toLower();
    if (v == QStringLiteral("exclusive")) return Tier::Exclusive;
    if (v == QStringLiteral("explicit"))  return Tier::Explicit;
    if (v == QStringLiteral("senior"))    return Tier::Senior;
    return Tier::Free;
}

void LicenseManager::load()
{
    QSettings s(QStringLiteral("EBALIA"), QStringLiteral("EBALIA Launcher"));
    // A stored session is only trusted until PatreonAuth revalidates it
    const bool hasSession =
        !s.value(QStringLiteral("patreon/access")).toString().isEmpty();
    if (hasSession) {
        m_linked     = true;
        m_patronName = s.value(QStringLiteral("patreon/name")).toString();
        m_tier = tierFromRank(s.value(QStringLiteral("patreon/tier"), 0).toInt());
    }
}

void LicenseManager::linkPatreon(const QString &patronName, int tierRank)
{
    m_linked     = true;
    m_patronName = patronName;
    m_tier       = tierFromRank(tierRank);

    QSettings s(QStringLiteral("EBALIA"), QStringLiteral("EBALIA Launcher"));
    s.setValue(QStringLiteral("patreon/name"), patronName);
    s.setValue(QStringLiteral("patreon/tier"), tierRank);
    emit licenseChanged();
}

void LicenseManager::unlink()
{
    m_linked = false;
    m_patronName.clear();
    m_tier = Tier::Free;

    QSettings s(QStringLiteral("EBALIA"), QStringLiteral("EBALIA Launcher"));
    s.remove(QStringLiteral("patreon/name"));
    s.remove(QStringLiteral("patreon/tier"));
    emit licenseChanged();
}
