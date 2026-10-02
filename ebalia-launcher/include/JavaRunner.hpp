#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

struct VersionInfo;

class JavaRunner : public QObject {
    Q_OBJECT
public:
    explicit JavaRunner(QObject *parent = nullptr);
    ~JavaRunner() override;

    void locateJava();
    QStringList findJava();

    // java: Java 8 for the package (a Windows java.exe when the package runs in Wine).
    void launch(const VersionInfo &version, const QString &installDir, const QString &java = {});
    bool isRunning() const { return m_process && m_process->state() != QProcess::NotRunning; }
    // Under Wine the game is a child of wineserver; the separate prefix is closed as a whole.
    void stop();

signals:
    void javaFound(const QString &path);
    void javaNotFound();
    void processStarted();
    void processFinished(int exitCode);
    void processError(const QString &error);

private slots:
    void onErrorOccurred(QProcess::ProcessError error);
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess *m_process = nullptr;
    QStringList m_javaPaths;
};
