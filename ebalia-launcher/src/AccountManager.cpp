#include "AccountManager.hpp"

#include <QFile>
#include <QSaveFile>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <QRegularExpression>
#include "Ui.hpp"

AccountManager::AccountManager(const QString &mcDir, QObject *parent)
    : QObject(parent)
{
    m_dir = mcDir;
    m_path = mcDir + QStringLiteral("/accounts.json");
    load();
}

void AccountManager::load()
{
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(f.readAll()).object();
    m_activeUuid = root[QStringLiteral("active")].toString();
    for (const QJsonValue &v : root[QStringLiteral("accounts")].toArray()) {
        const QJsonObject o = v.toObject();
        McAccount a;
        a.type         = o[QStringLiteral("type")].toString();
        a.name         = o[QStringLiteral("name")].toString();
        a.uuid         = o[QStringLiteral("uuid")].toString();
        a.mcToken      = o[QStringLiteral("mcToken")].toString();
        a.refreshToken = o[QStringLiteral("refreshToken")].toString();
        m_accounts.append(a);
    }
}

void AccountManager::save() const
{
    QJsonArray arr;
    for (const McAccount &a : m_accounts) {
        QJsonObject o;
        o[QStringLiteral("type")]         = a.type;
        o[QStringLiteral("name")]         = a.name;
        o[QStringLiteral("uuid")]         = a.uuid;
        o[QStringLiteral("mcToken")]      = a.mcToken;
        o[QStringLiteral("refreshToken")] = a.refreshToken;
        arr.append(o);
    }
    QJsonObject root;
    root[QStringLiteral("active")]   = m_activeUuid;
    root[QStringLiteral("accounts")] = arr;
    QSaveFile f(m_path);
    if (f.open(QIODevice::WriteOnly)) {
        f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        f.write(QJsonDocument(root).toJson());
        f.commit();
    }
}

McAccount AccountManager::active() const
{
    for (const McAccount &a : m_accounts)
        if (a.uuid == m_activeUuid)
            return a;
    return McAccount{};
}

bool AccountManager::hasActive() const
{
    return !m_activeUuid.isEmpty() &&
           std::any_of(m_accounts.begin(), m_accounts.end(),
                       [this](const McAccount &a) { return a.uuid == m_activeUuid; });
}

void AccountManager::addAccount(const McAccount &acc)
{
    for (int i = 0; i < m_accounts.size(); ++i)
        if (m_accounts[i].uuid == acc.uuid) {   // refresh in place
            m_accounts[i] = acc;
            m_activeUuid = acc.uuid;
            save();
            emit accountsChanged();
            return;
        }
    m_accounts.append(acc);
    m_activeUuid = acc.uuid;
    save();
    emit accountsChanged();
}

void AccountManager::removeAccount(const QString &uuid)
{
    for (int i = 0; i < m_accounts.size(); ++i)
        if (m_accounts[i].uuid == uuid) {
            m_accounts.removeAt(i);
            QFile::remove(picturePath(uuid));
            break;
        }
    if (m_activeUuid == uuid)
        m_activeUuid = m_accounts.isEmpty() ? QString() : m_accounts.first().uuid;
    save();
    emit accountsChanged();
}

void AccountManager::setActive(const QString &uuid)
{
    m_activeUuid = uuid;
    save();
    emit accountsChanged();
}

void AccountManager::updateTokens(const QString &uuid, const QString &mcToken,
                                  const QString &refreshToken)
{
    for (McAccount &a : m_accounts)
        if (a.uuid == uuid) {
            if (!mcToken.isEmpty())      a.mcToken = mcToken;
            if (!refreshToken.isEmpty()) a.refreshToken = refreshToken;
            break;
        }
    save();
}

QString AccountManager::picturePath(const QString &uuid) const
{
    static const QRegularExpression valid(QStringLiteral("^[0-9A-Fa-f-]{32,36}$"));
    if (!valid.match(uuid).hasMatch())
        return {};
    return m_dir + QStringLiteral("/avatars/") + uuid.toLower() + QStringLiteral(".png");
}

bool AccountManager::hasPicture(const QString &uuid) const
{
    const QString path = picturePath(uuid);
    return !path.isEmpty() && QFile::exists(path);
}

void AccountManager::setPicture(const QString &uuid, const QImage &picture)
{
    const QString path = picturePath(uuid);
    if (path.isEmpty() || picture.isNull())
        return;
    QDir().mkpath(m_dir + QStringLiteral("/avatars"));
    QSaveFile f(path);
    if (f.open(QIODevice::WriteOnly) && picture.save(&f, "PNG") && f.commit())
        emit accountsChanged();
}

bool AccountManager::hasCustomPicture(const QString &uuid) const
{
    const QString path = picturePath(uuid);
    return !path.isEmpty() && QFile::exists(path.chopped(4) + QStringLiteral("-custom.png"));
}

void AccountManager::setCustomPicture(const QString &uuid, const QImage &picture)
{
    const QString path = picturePath(uuid);
    if (path.isEmpty() || picture.isNull())
        return;
    QDir().mkpath(m_dir + QStringLiteral("/avatars"));
    QSaveFile f(path.chopped(4) + QStringLiteral("-custom.png"));
    if (f.open(QIODevice::WriteOnly) && picture.save(&f, "PNG") && f.commit())
        emit accountsChanged();
}

void AccountManager::clearCustomPicture(const QString &uuid)
{
    const QString path = picturePath(uuid);
    if (!path.isEmpty() && QFile::remove(path.chopped(4) + QStringLiteral("-custom.png")))
        emit accountsChanged();
}

QPixmap AccountManager::picture(const McAccount &account, int size) const
{
    if (hasCustomPicture(account.uuid)) {
        const QPixmap custom(picturePath(account.uuid).chopped(4) + QStringLiteral("-custom.png"));
        if (!custom.isNull())
            return Ui::cover(custom, QSize(size, size), int(size * 0.28));
    }
    if (hasPicture(account.uuid)) {
        const QPixmap source(picturePath(account.uuid));
        if (!source.isNull())
            return Ui::cover(source, QSize(size, size), int(size * 0.28));
    }
    return Ui::avatar(account.name, size);
}
