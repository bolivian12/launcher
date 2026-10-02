#pragma once
#include <QStringList>
#include <QList>
struct JavaInstallation {QString path,version,architecture,vendor;int major=0;};
class JavaRuntime {
public:
    static QStringList candidates(const QStringList &extraRoots={});
    static JavaInstallation inspect(const QString &path);
    static JavaInstallation parse(const QString &path,const QString &output);
    static QList<JavaInstallation> discover(const QStringList &extraRoots={});
    // Exact major first; newer Java only for versions that already need Java 16+ (Java 8 games stay on Java 8).
    static bool compatible(int required,int major);
    static QString select(int major,const QString &overridePath={});
    static QString dataDir();
};
