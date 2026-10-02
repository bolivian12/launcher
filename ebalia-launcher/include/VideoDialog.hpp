#pragma once

#include <QDialog>

class QMediaPlayer;
class QVideoWidget;
class QAudioOutput;
class QPushButton;
class QSlider;

// Minimal in-launcher trailer player (bundled mp4 from the legacy project).
class VideoDialog : public QDialog {
    Q_OBJECT
public:
    explicit VideoDialog(const QString &title, const QString &resourcePath,
                         QWidget *parent = nullptr);
    ~VideoDialog() override;

private:
    QMediaPlayer *m_player;
    QVideoWidget *m_video;
    QAudioOutput *m_audio;
    QPushButton *m_playPause;
    QSlider *m_seek;
};
