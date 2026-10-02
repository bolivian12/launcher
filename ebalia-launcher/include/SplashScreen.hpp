#pragma once

#include <QWidget>

class QLabel;
class QProgressBar;
class QGraphicsOpacityEffect;

// Frameless startup splash, Minecraft style: dirt-textured panel, the EBALIA
// logo scales in, the letters E-B-A-L-I-A drop one by one with a bounce,
// progress bar fills, window fades out, finished() fires.
class SplashScreen : public QWidget {
    Q_OBJECT
public:
    explicit SplashScreen(QWidget *parent = nullptr);

signals:
    void finished();

private:
    QProgressBar *m_progress;
};
