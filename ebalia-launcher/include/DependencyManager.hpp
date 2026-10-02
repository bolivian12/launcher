#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>

class QProcess;

// Runtime dependencies the lost versions need, mirroring what the legacy
// launcher shipped in CD_Root/AutoPlay:
//   - Java 8+          (JavaSetup8u431.exe / OpenJDK msi back then)
//   - Jarfix           (fixes .jar file association — Windows)
//   - OpenAL           (oalinst.exe, game audio — Windows)
//   - Game resources   (Versiones*.zip + betacraft.zip packs)
class DependencyManager : public QObject {
    Q_OBJECT
public:
    enum class Dep { Java, Jarfix, OpenAL, Resources };

    explicit DependencyManager(QObject *parent = nullptr);

    bool isInstalled(Dep d) const;
    static QString depName(Dep d);
    static QString depDescription(Dep d);
    static bool depSupportedOnThisOS(Dep d);

    void install(Dep d);   // downloads / extracts / runs as needed

signals:
    void progress(Dep d, int percent, const QString &stage);
    void installed(Dep d);
    void failed(Dep d, const QString &error);

private:
    void installJava();
    void installJarfix();
    void installOpenAL();
    void installResources();
    void downloadTo(const QString &url, const QString &dest, Dep d,
                    const QString &stage);
    bool extractZip(const QString &zip, const QString &destDir);

    QNetworkAccessManager m_nam;
    QString m_dataDir;
};
