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

    void launch(const VersionInfo &version, const QString &installDir);

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
