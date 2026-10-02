#include "NiceButton.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QImage>
#include <random>

// Minecraft-style stone/green block textures, generated once
static QPixmap blockTex(bool green)
{
    static QPixmap stone, grass;
    QPixmap &cache = green ? grass : stone;
    if (!cache.isNull())
        return cache;

    static const QVector<QRgb> stonePal = { 0x757575, 0x787878, 0x737373, 0x7a7a7a, 0x717171 };
    static const QVector<QRgb> greenPal = { 0x4f9d37, 0x51a039, 0x4e9c36, 0x53a23a, 0x509e38 };

    const QVector<QRgb> &pal = green ? greenPal : stonePal;
    const int px = 16;
    QImage img(px, px, QImage::Format_RGB32);
    std::mt19937 rng(green ? 0xE6A1 : 0xE6A0);
    for (int y = 0; y < px; ++y)
        for (int x = 0; x < px; ++x)
            img.setPixel(x, y, pal[rng() % pal.size()]);

    // 16px native tile — fine, subtle grain
    cache = QPixmap::fromImage(img);
    return cache;
}

NiceButton::NiceButton(const QString &text, Role role, QWidget *parent)
    : QPushButton(text, parent)
    , m_role(role)
{
    setCursor(Qt::PointingHandCursor);
}

void NiceButton::setHover(qreal h)
{
    m_hover = h;
    update();
}

void NiceButton::animateTo(qreal target)
{
    auto *a = new QPropertyAnimation(this, "hover", this);
    a->setDuration(140);
    a->setStartValue(m_hover);
    a->setEndValue(target);
    a->setEasingCurve(QEasingCurve::OutCubic);
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

void NiceButton::enterEvent(QEnterEvent *event)
{
    animateTo(1.0);
    QPushButton::enterEvent(event);
}

void NiceButton::leaveEvent(QEvent *event)
{
    animateTo(0.0);
    QPushButton::leaveEvent(event);
}

void NiceButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    const QRect r = rect();

    // ── Block texture ──
    p.drawTiledPixmap(r, blockTex(m_role == Primary));

    // Disabled / pressed / hover state shading
    if (!isEnabled()) {
        p.fillRect(r, QColor(20, 20, 20, 165));
    } else if (isDown()) {
        p.fillRect(r, QColor(0, 0, 0, 70));
    } else if (m_hover > 0.0) {
        p.fillRect(r, QColor(255, 255, 255, int(m_hover * 34)));
    }

    // ── 3D bevel (inverts when pressed, like an in-game MC button) ──
    const int b = 2;
    const bool sunken = isDown() && isEnabled();
    const QColor light = sunken ? QColor(0, 0, 0, 150)      : QColor(255, 255, 255, 110);
    const QColor dark  = sunken ? QColor(255, 255, 255, 60) : QColor(0, 0, 0, 130);

    p.fillRect(r.x(), r.y(), r.width(), b, light);                       // top
    p.fillRect(r.x(), r.y(), b, r.height(), light);                      // left
    p.fillRect(r.x(), r.bottom() - b + 1, r.width(), b, dark);           // bottom
    p.fillRect(r.right() - b + 1, r.y(), b, r.height(), dark);           // right

    // Outer 1px frame for definition
    p.setPen(QColor(0, 0, 0, 90));
    p.drawRect(r.adjusted(0, 0, -1, -1));

    // ── Label with Minecraft drop shadow ──
    QColor fg = isEnabled() ? QColor(255, 255, 255) : QColor(0x66, 0x66, 0x66);
    const QRect textRect = sunken ? r.translated(1, 1) : r;
    if (isEnabled()) {
        p.setPen(QColor(0, 0, 0, 160));
        p.drawText(textRect.translated(1, 1), Qt::AlignCenter, text());
    }
    p.setPen(fg);
    p.drawText(textRect, Qt::AlignCenter, text());
}
