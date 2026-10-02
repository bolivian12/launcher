#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QFile>
#include <QNetworkReply>
#include <memory>

class DownloadManager : public QObject {
    Q_OBJECT
public:
    explicit DownloadManager(QObject *parent = nullptr);
    ~DownloadManager() override;

    void downloadFile(const QString &url, const QString &outputPath);

signals:
    void progress(qint64 bytesReceived, qint64 bytesTotal);
    void finished(const QString &filePath, bool success, const QString &error);

private slots:
    void onReadyRead();
    void onProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onFinished();

private:
    QNetworkAccessManager m_manager;
    QNetworkReply *m_reply = nullptr;
    std::unique_ptr<QFile> m_file;
};
