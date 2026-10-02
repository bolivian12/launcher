#include "DependencyManager.hpp"
#include "JavaRunner.hpp"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QSettings>
#include <QFileInfo>

// Live pack URLs (verified on the HeelXel/Launcher release)
static const char *kPackUrls[] = {
    "https://github.com/HeelXel/Launcher/releases/download/Launcher-Archivos1.0/Versiones3.zip",
    "https://github.com/HeelXel/Launcher/releases/download/Launcher-Archivos1.0/Versiones2.zip",
    "https://github.com/HeelXel/Launcher/releases/download/Launcher-Archivos1.0/Versiones.zip",
    "https://github.com/HeelXel/Launcher/releases/download/Launcher-Archivos1.0/betacraft.zip",
};
static const int kPackCount = 4;

// Adoptium Temurin 8 installers per OS (redirect to the real artifact)
#if defined(Q_OS_WIN)
static const char kJavaUrl[] =
    "https://api.adoptium.net/v3/installer/latest/8/ga/windows/x64/jdk/hotspot/normal/eclipse";
#elif defined(Q_OS_MAC)
static const char kJavaUrl[] =
    "https://api.adoptium.net/v3/installer/latest/8/ga/mac/x64/jdk/hotspot/normal/eclipse";
#else
static const char kJavaUrl[] =
    "https://api.adoptium.net/v3/binary/latest/8/ga/linux/x64/jre/hotspot/normal/eclipse";
#endif

DependencyManager::DependencyManager(QObject *parent)
    : QObject(parent)
{
    m_dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString DependencyManager::depName(Dep d)
{
    switch (d) {
    case Dep::Java:      return QStringLiteral("Java 8+");
    case Dep::Jarfix:    return QStringLiteral("Jarfix");
    case Dep::OpenAL:    return QStringLiteral("OpenAL");
    case Dep::Resources: return QStringLiteral("Game resources");
    }
    return QString();
}

QString DependencyManager::depDescription(Dep d)
{
    switch (d) {
    case Dep::Java:
        return QStringLiteral("Required to run every version. Installs OpenJDK (Temurin) 8.");
    case Dep::Jarfix:
        return QStringLiteral("Fixes the .jar file association so double-clicked jars run.");
    case Dep::OpenAL:
        return QStringLiteral("Audio library (oalinst) — without it the versions have no sound.");
    case Dep::Resources:
        return QStringLiteral("Downloaded once.");
    }
    return QString();
}

bool DependencyManager::depSupportedOnThisOS(Dep d)
{
#if defined(Q_OS_WIN)
    Q_UNUSED(d)
    return true;
#else
    // Jarfix and OpenAL installers are Windows-only
    return d == Dep::Java || d == Dep::Resources;
#endif
}

bool DependencyManager::isInstalled(Dep d) const
{
    switch (d) {
    case Dep::Java: {
        JavaRunner probe;
        return !probe.findJava().isEmpty();
    }
    case Dep::Jarfix: {
#if defined(Q_OS_WIN)
        QSettings assoc(QStringLiteral("HKEY_CLASSES_ROOT\\.jar"), QSettings::NativeFormat);
        return !assoc.value(QStringLiteral(".")).toString().isEmpty();
#else
        return true;
#endif
    }
    case Dep::OpenAL: {
#if defined(Q_OS_WIN)
        return QFile::exists(QStringLiteral("C:/Windows/System32/OpenAL32.dll"));
#else
        return true;
#endif
    }
    case Dep::Resources:
        return QFile::exists(m_dataDir + QStringLiteral("/resources_installed.txt"));
    }
    return false;
}

void DependencyManager::install(Dep d)
{
    switch (d) {
    case Dep::Java:      installJava();      break;
    case Dep::Jarfix:    installJarfix();    break;
    case Dep::OpenAL:    installOpenAL();    break;
    case Dep::Resources: installResources(); break;
    }
}

// ── Java ──
void DependencyManager::installJava()
{
#if defined(Q_OS_WIN)
    const QString dest = QDir::tempPath() + QStringLiteral("/ebalia_java8.msi");
#elif defined(Q_OS_MAC)
    const QString dest = QDir::tempPath() + QStringLiteral("/ebalia_java8.pkg");
#else
    const QString dest = QDir::tempPath() + QStringLiteral("/ebalia_java8.tar.gz");
#endif
    downloadTo(QString::fromLatin1(kJavaUrl), dest, Dep::Java,
               QStringLiteral("Downloading Java 8"));
}

// ── Jarfix / OpenAL: bundled in resources, written to temp and executed ──
void DependencyManager::installJarfix()
{
#if defined(Q_OS_WIN)
    const QString dest = QDir::tempPath() + QStringLiteral("/jarfix.exe");
    QFile::remove(dest);
    if (!QFile::copy(QStringLiteral(":/tools/jarfix.exe"), dest)) {
        emit failed(Dep::Jarfix, QStringLiteral("Cannot extract bundled jarfix.exe"));
        return;
    }
    emit progress(Dep::Jarfix, 50, QStringLiteral("Running jarfix"));
    QProcess::startDetached(dest, QStringList());
    emit progress(Dep::Jarfix, 100, QStringLiteral("Done"));
    emit installed(Dep::Jarfix);
#else
    emit failed(Dep::Jarfix, QStringLiteral("Windows only"));
#endif
}

void DependencyManager::installOpenAL()
{
#if defined(Q_OS_WIN)
    const QString dest = QDir::tempPath() + QStringLiteral("/oalinst.exe");
    QFile::remove(dest);
    if (!QFile::copy(QStringLiteral(":/tools/oalinst.exe"), dest)) {
        emit failed(Dep::OpenAL, QStringLiteral("Cannot extract bundled oalinst.exe"));
        return;
    }
    emit progress(Dep::OpenAL, 50, QStringLiteral("Running OpenAL installer"));
    QProcess::startDetached(dest, QStringList());
    emit progress(Dep::OpenAL, 100, QStringLiteral("Done"));
    emit installed(Dep::OpenAL);
#else
    emit failed(Dep::OpenAL, QStringLiteral("Windows only"));
#endif
}

// ── Game resources: download the 4 packs, extract, write marker ──
void DependencyManager::installResources()
{
    // Sequential downloads, then extraction happens in the UI layer loop
    downloadTo(QString::fromLatin1(kPackUrls[0]),
               m_dataDir + QStringLiteral("/Versiones3.zip"),
               Dep::Resources, QStringLiteral("Pack 1/4"));
}

void DependencyManager::downloadTo(const QString &url, const QString &dest, Dep d,
                                   const QString &stage)
{
    emit progress(d, 0, stage);

    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("EBALIA-Launcher/3.0"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam.get(req);
    auto *file = new QFile(dest);
    if (!file->open(QIODevice::WriteOnly)) {
        emit failed(d, QStringLiteral("Cannot write ") + dest);
        reply->abort();
        reply->deleteLater();
        file->deleteLater();
        return;
    }

    connect(reply, &QNetworkReply::readyRead, this, [reply, file] {
        file->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress,
            this, [this, d, stage](qint64 recv, qint64 total) {
        if (total > 0)
            emit progress(d, int(recv * 100 / total), stage);
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, file, dest, d, stage, url] {
        file->write(reply->readAll());
        file->close();
        file->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString();
            reply->deleteLater();
            QFile::remove(dest);
            emit failed(d, err);
            return;
        }
        reply->deleteLater();

        // Chain the remaining packs, then extract
        static const QStringList packs = {
            QStringLiteral("Versiones3.zip"), QStringLiteral("Versiones2.zip"),
            QStringLiteral("Versiones.zip"),  QStringLiteral("betacraft.zip"),
        };
        const int idx = packs.indexOf(QFileInfo(dest).fileName());

        if (d == Dep::Resources && idx >= 0 && idx + 1 < kPackCount) {
            downloadTo(QString::fromLatin1(kPackUrls[idx + 1]),
                       m_dataDir + QLatin1Char('/') + packs[idx + 1],
                       d, QStringLiteral("Pack %1/4").arg(idx + 2));
            return;
        }

        if (d == Dep::Resources && idx == kPackCount - 1) {
            emit progress(d, 95, QStringLiteral("Extracting packs"));
            bool ok = true;
            for (const QString &p : packs)
                ok = extractZip(m_dataDir + QLatin1Char('/') + p, m_dataDir) && ok;
            if (ok) {
                QFile marker(m_dataDir + QStringLiteral("/resources_installed.txt"));
                if (marker.open(QIODevice::WriteOnly)) {
                    marker.write("Download and resource extraction completed\n");
                    marker.close();
                }
                for (const QString &p : packs)
                    QFile::remove(m_dataDir + QLatin1Char('/') + p);
                emit progress(d, 100, QStringLiteral("Done"));
                emit installed(d);
            } else {
                emit failed(d, QStringLiteral("Extraction failed — install 7z/bsdtar/unzip"));
            }
            return;
        }

        if (d == Dep::Java) {
            // Windows/macOS: run the installer. Linux: unpack into dataDir/java.
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
            emit progress(d, 90, QStringLiteral("Running Java installer"));
            QProcess::startDetached(dest, QStringList());
            emit progress(d, 100, QStringLiteral("Done"));
            emit installed(d);
#else
            emit progress(d, 90, QStringLiteral("Unpacking Java"));
            QDir().mkpath(m_dataDir + QStringLiteral("/java"));
            QProcess tar;
            tar.start(QStringLiteral("tar"),
                      {QStringLiteral("-xzf"), dest,
                       QStringLiteral("-C"), m_dataDir + QStringLiteral("/java")});
            tar.waitForFinished(120000);
            if (tar.exitCode() == 0) {
                emit progress(d, 100, QStringLiteral("Done"));
                emit installed(d);
            } else {
                emit failed(d, QStringLiteral("tar extraction failed"));
            }
#endif
            return;
        }
    });
}

bool DependencyManager::extractZip(const QString &zip, const QString &destDir)
{
    // Try the extractors the legacy launcher relied on, plus unix natives
    const QStringList candidates = {
        QStringLiteral("7z"),     // 7-Zip
        QStringLiteral("bsdtar"), // libarchive
        QStringLiteral("unzip"),
    };
    for (const QString &tool : candidates) {
        QProcess p;
        if (tool == QStringLiteral("7z"))
            p.start(tool, {QStringLiteral("x"), zip,
                           QStringLiteral("-o") + destDir, QStringLiteral("-y")});
        else if (tool == QStringLiteral("bsdtar"))
            p.start(tool, {QStringLiteral("-xf"), zip,
                           QStringLiteral("-C"), destDir});
        else
            p.start(tool, {QStringLiteral("-o"), zip,
                           QStringLiteral("-d"), destDir});
        if (!p.waitForStarted(5000))
            continue;
        p.waitForFinished(-1);
        if (p.exitCode() == 0)
            return true;
    }
    return false;
}
