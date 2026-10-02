#include "PixelPanel.hpp"
#include "Textures.hpp"

#include <QPainter>

PixelPanel::PixelPanel(Texture tex, QWidget *parent)
    : QFrame(parent)
{
    switch (tex) {
    case Dirt:      m_tex = textures::dirt();      break;
    case GrassSide: m_tex = textures::grassSide(); break;
    case Stone:     m_tex = textures::stone();     break;
    case Deepslate: m_tex = textures::deepslate(); break;
    }
}

void PixelPanel::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter p(this);
    p.drawTiledPixmap(rect(), m_tex);

    // Minecraft-style bevel: light on top/left, dark on bottom/right
    const int b = 3;
    const QColor light(255, 255, 255, 70);
    const QColor dark(0, 0, 0, 110);

    p.fillRect(0, 0, width(), b, light);                    // top
    p.fillRect(0, 0, b, height(), light);                   // left
    p.fillRect(0, height() - b, width(), b, dark);          // bottom
    p.fillRect(width() - b, 0, b, height(), dark);          // right
}
