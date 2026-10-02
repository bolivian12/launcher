#include "DownloadManager.hpp"
#include <QUrl>
#include <QNetworkRequest>

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
    , m_reply(nullptr)
{
}

DownloadManager::~DownloadManager()
{
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
    }
}

void DownloadManager::downloadFile(const QString &url, const QString &outputPath)
{
    // Abort any existing download
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_file.reset(new QFile(outputPath));
    if (!m_file->open(QIODevice::WriteOnly)) {
        emit finished(outputPath, false, "Cannot open file for writing: " + outputPath);
        return;
    }

    QUrl qurl(url);
    QNetworkRequest request(qurl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, "EBALIA-Launcher/1.0");

    m_reply = m_manager.get(request);

    connect(m_reply, &QNetworkReply::readyRead,
            this, &DownloadManager::onReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress,
            this, &DownloadManager::onProgress);
    connect(m_reply, &QNetworkReply::finished,
            this, &DownloadManager::onFinished);
}

void DownloadManager::onReadyRead()
{
    if (m_reply && m_file && m_file->isOpen()) {
        m_file->write(m_reply->readAll());
    }
}

void DownloadManager::onProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit progress(bytesReceived, bytesTotal);
}

void DownloadManager::onFinished()
{
    if (!m_reply) return;

    // Drain any remaining data
    if (m_file && m_file->isOpen()) {
        m_file->write(m_reply->readAll());
        m_file->close();
    }

    QString filePath = m_file ? m_file->fileName() : QString();
    bool ok = (m_reply->error() == QNetworkReply::NoError);
    QString err = ok ? QString() : m_reply->errorString();

    m_reply->deleteLater();
    m_reply = nullptr;
    m_file.reset();

    if (!ok && !filePath.isEmpty())
        QFile::remove(filePath);

    emit finished(filePath, ok, err);
}
