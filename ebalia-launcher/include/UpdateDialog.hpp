#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QProgressBar;
class UpdateChecker;
class DownloadManager;

// Qt port of the legacy updaterfree.ps1 assistant:
// pick a folder, download the new launcher build, progress bar included.
class UpdateDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateDialog(UpdateChecker *checker, QWidget *parent = nullptr);

private slots:
    void onBrowse();
    void onDownload();

private:
    UpdateChecker *m_checker;
    DownloadManager *m_downloader;
    QLineEdit *m_pathEdit;
    QPushButton *m_downloadBtn;
    QProgressBar *m_progress;
    QLabel *m_status;
};
