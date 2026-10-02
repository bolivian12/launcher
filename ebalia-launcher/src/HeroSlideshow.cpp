#include "HeroSlideshow.hpp"

#include <QPainter>
#include <QTimer>
#include <QPropertyAnimation>

HeroSlideshow::HeroSlideshow(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void HeroSlideshow::addFrame(const QPixmap &p)
{
    if (!p.isNull())
        m_frames.append(p);
}

void HeroSlideshow::start(int intervalMs)
{
    if (m_frames.size() < 2 || m_timer)
        return;
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &HeroSlideshow::advance);
    m_timer->start(intervalMs);
}

void HeroSlideshow::setBlend(qreal b)
{
    m_blend = b;
    update();
}

void HeroSlideshow::advance()
{
    m_next = (m_cur + 1) % m_frames.size();

    auto *anim = new QPropertyAnimation(this, "blend", this);
    anim->setDuration(700);
    anim->setStartValue(1.0);
    anim->setEndValue(0.0);   // fades current out, revealing next underneath
    anim->setEasingCurve(QEasingCurve::InOutQuad);

    connect(anim, &QPropertyAnimation::finished, this, [this] {
        m_cur = m_next;
        m_blend = 1.0;
        update();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void HeroSlideshow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(15, 15, 15));
    if (m_frames.isEmpty())
        return;

    const auto drawCover = [this](QPainter &p, const QPixmap &f) {
        const QSize scaled = f.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
        const QRect target((width() - scaled.width()) / 2,
                           (height() - scaled.height()) / 2,
                           scaled.width(), scaled.height());
        p.drawPixmap(target, f);
    };

    // During a crossfade, next sits underneath and current fades out
    if (m_blend < 1.0)
        drawCover(p, m_frames[m_next]);

    p.save();
    p.setOpacity(m_blend);
    drawCover(p, m_frames[m_cur]);
    p.restore();

    // Readability dim for the overlaid UI
    p.fillRect(rect(), QColor(10, 10, 10, 120));
}
