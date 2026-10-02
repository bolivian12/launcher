#pragma once

#include <QWidget>
#include <QPixmap>
#include <QList>

class QTimer;

// Crossfading image slideshow — the hero background, Minecraft-launcher style.
class HeroSlideshow : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal blend READ blend WRITE setBlend)
public:
    explicit HeroSlideshow(QWidget *parent = nullptr);

    void addFrame(const QPixmap &p);
    void start(int intervalMs = 6000);

    qreal blend() const { return m_blend; }
    void setBlend(qreal b);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void advance();

    QList<QPixmap> m_frames;
    QTimer *m_timer = nullptr;
    int m_cur = 0;
    int m_next = 0;
    qreal m_blend = 1.0;   // 1.0 = current fully shown
};
