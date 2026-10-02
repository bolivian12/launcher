#include "UpdateDialog.hpp"
#include "UpdateChecker.hpp"
#include "DownloadManager.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QFileDialog>
#include <QPixmap>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QCoreApplication>

UpdateDialog::UpdateDialog(UpdateChecker *checker, QWidget *parent)
    : QDialog(parent)
    , m_checker(checker)
{
    setWindowTitle(QStringLiteral("EBALIA Download Assistant"));
    setModal(true);
    setMinimumWidth(460);

    m_downloader = new DownloadManager(this);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(12);

    auto *logo = new QLabel(this);
    logo->setPixmap(QPixmap(QStringLiteral(":/ebalia.png"))
                        .scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    root->addWidget(logo);

    auto *title = new QLabel(QStringLiteral("UPDATE AVAILABLE"), this);
    title->setObjectName(QStringLiteral("dialogTitle"));
    title->setAlignment(Qt::AlignCenter);
    root->addWidget(title);

    auto *info = new QLabel(
        QStringLiteral("Select the folder where the new build will be downloaded.\n"
                       "To update in place, pick the folder where the launcher lives."), this);
    info->setObjectName(QStringLiteral("dialogInfo"));
    info->setAlignment(Qt::AlignCenter);
    root->addWidget(info);

    auto *pathRow = new QHBoxLayout;
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setObjectName(QStringLiteral("licenseInput"));
    m_pathEdit->setText(QCoreApplication::applicationDirPath());
    pathRow->addWidget(m_pathEdit, 1);
    auto *browse = new QPushButton(QStringLiteral("BROWSE"), this);
    browse->setObjectName(QStringLiteral("dialogBtnSecondary"));
    browse->setCursor(Qt::PointingHandCursor);
    pathRow->addWidget(browse);
    root->addLayout(pathRow);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    root->addWidget(m_progress);

    m_status = new QLabel(QStringLiteral("Progress: 0%"), this);
    m_status->setObjectName(QStringLiteral("dialogInfo"));
    m_status->setAlignment(Qt::AlignCenter);
    root->addWidget(m_status);

    auto *row = new QHBoxLayout;
    row->addStretch(1);
    m_downloadBtn = new QPushButton(QStringLiteral("DOWNLOAD"), this);
    m_downloadBtn->setObjectName(QStringLiteral("dialogBtn"));
    m_downloadBtn->setCursor(Qt::PointingHandCursor);
    row->addWidget(m_downloadBtn);
    root->addLayout(row);

    connect(browse, &QPushButton::clicked, this, &UpdateDialog::onBrowse);
    connect(m_downloadBtn, &QPushButton::clicked, this, &UpdateDialog::onDownload);

    connect(m_downloader, &DownloadManager::progress,
            this, [this](qint64 recv, qint64 total) {
        if (total > 0) {
            const int pct = static_cast<int>(recv * 100 / total);
            m_progress->setValue(pct);
            m_status->setText(QStringLiteral("Progress: %1%").arg(pct));
        }
    });

    connect(m_downloader, &DownloadManager::finished,
            this, [this](const QString &filePath, bool ok, const QString &err) {
        m_downloadBtn->setEnabled(true);
        if (ok) {
            m_checker->markUpdated();
            m_progress->setValue(100);
            m_status->setText(QStringLiteral("Update completed! Saved to:\n") + filePath);
        } else {
            m_status->setText(QStringLiteral("Download failed: ") + err);
        }
    });

    // No direct asset (shouldn't happen) → fall back to opening the release page
    if (m_checker->assetUrl().isEmpty()) {
        m_downloadBtn->setText(QStringLiteral("OPEN RELEASE PAGE"));
    }
}

void UpdateDialog::onBrowse()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select download folder"), m_pathEdit->text());
    if (!dir.isEmpty())
        m_pathEdit->setText(dir);
}

void UpdateDialog::onDownload()
{
    if (m_checker->assetUrl().isEmpty()) {
        QDesktopServices::openUrl(QUrl(m_checker->releaseUrl()));
        return;
    }

    const QString dest = QDir(m_pathEdit->text()).filePath(m_checker->assetName());
    m_downloadBtn->setEnabled(false);
    m_progress->setValue(0);
    m_status->setText(QStringLiteral("Progress: 0%"));
    m_downloader->downloadFile(m_checker->assetUrl(), dest);
}
