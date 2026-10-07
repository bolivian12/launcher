#pragma once
#include <QObject>
#include <QJsonObject>
#include <QList>
#include <QHash>
#include <QSet>
#include <QProcess>

struct McVersion { QString id, type, url, releaseTime; };
struct McInstance {
    QString name, dir, mcVersion, loader;
    bool ready = false;
    int xmx = 4096;
    qint64 lastPlayed = 0, totalSecs = 0;
};
class McInstanceManager : public QObject {
    Q_OBJECT
public:
    explicit McInstanceManager(const QString &dataDir, QObject *parent = nullptr);
    void fetchManifest();
    QList<McInstance> instances() const;
    QString instancesRoot() const { return m_root + "/instances"; }
    QString mcDir() const { return m_root; }
    QString createInstance(const QString &name, const QString &version, const QString &loader, const QString &loaderVersion = {});
    void deleteInstance(const QString &dir);
    // Copies worlds, mods and settings into a new independent instance.
    QString copyInstance(const QString &dir, const QString &name);
    void exportInstance(const QString &dir, const QString &zip);
    void installInstance(const QString &dir);
    void launch(const QString &dir, const QString &name, const QString &uuid, const QString &token, const QString &userType);
    bool isRunning(const QString &dir) const { return m_running.contains(dir); }
    bool isInstalling(const QString &dir) const { return m_installing.contains(dir); }
    void killInstance(const QString &dir);
    static bool allowedByRules(const QJsonObject &object);
    // inheritsFrom-style merge: loader libraries first, replacing the game's copy of the same group:artifact:classifier.
    // A free folder in parent named after the instance: characters Windows rejects become "_", " (2)" when taken.
    // current is the instance's own folder, which may keep its name.
    static QString folderFor(const QString &parent, const QString &name, const QString &current = {});
    // Renames the folder to the instance name when it is not in use; returns the folder the instance is in.
    QString renameFolder(const QString &dir);
    // Instances from launchers before 1.1.1 live in random-number folders: give them their names.
    void nameFolders();
    static QJsonArray mergeLibraries(const QJsonArray &game, const QJsonArray &loader);
    static QStringList arguments(const QJsonArray &list, const QMap<QString,QString> &values);
signals:
    void manifestReady(const QList<McVersion> &versions);
    void manifestFailed(const QString &error);
    void installProgress(const QString &dir, int percent, const QString &stage);
    void installDone(const QString &dir, bool ok, const QString &error);
    void gameStarted(const QString &dir);
    void gameEnded(const QString &dir, int code);
    void launchFailed(const QString &dir, const QString &error);
private:
    void install(const QString &dir);
    QString javaFor(const QString &dir,const QJsonObject &info,const QJsonObject &profile);
    QString m_root;
    QHash<QString,QProcess*> m_running;
    QSet<QString> m_installing;
    bool m_fetching = false;
};
