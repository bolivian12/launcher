#pragma once
#include <QJsonObject>
#include <QString>
#include <functional>
namespace LauncherUpdater {
using Progress = std::function<void(int,const QString &)>;
QJsonObject prepare(const QJsonObject &release, const QString &root, Progress progress = {});
bool launch(const QJsonObject &prepared, const QString &root, const QString &pending = {});
QString writePending(const QJsonObject &prepared, const QString &root);
QJsonObject readPending(const QString &name, const QString &root);
bool waitForParent(const QJsonObject &prepared);
bool applyPrepared(const QJsonObject &prepared, const QString &root);
void cleanupInstalled(const QString &root);
bool activate(const QJsonObject &prepared, const QString &root);
bool forwardToInstalled(const QString &root);
bool trustedAsset(const QString &url, const QString &tag);
QString assetName(const QString &platform, const QString &architecture);
QByteArray checksum(const QByteArray &manifest, const QString &name);
}
