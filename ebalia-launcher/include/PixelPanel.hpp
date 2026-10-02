#pragma once

#include <QFrame>
#include <QPixmap>

// A panel painted with a tiled Minecraft-style block texture plus a
// beveled edge (light top/left, dark bottom/right), like in-game surfaces.
class PixelPanel : public QFrame {
    Q_OBJECT
public:
    enum Texture { Dirt, GrassSide, Stone, Deepslate };

    explicit PixelPanel(Texture tex, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_tex;
};
