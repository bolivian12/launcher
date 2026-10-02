#include "Textures.hpp"

#include <QImage>
#include <random>

namespace textures {

static QPixmap makeNoise(int px, const QVector<QRgb> &palette, quint32 seed, int scale)
{
    QImage img(px, px, QImage::Format_RGB32);
    std::mt19937 rng(seed);
    for (int y = 0; y < px; ++y)
        for (int x = 0; x < px; ++x)
            img.setPixel(x, y, palette[rng() % palette.size()]);
    return QPixmap::fromImage(img).scaled(px * scale, px * scale,
                                          Qt::IgnoreAspectRatio,
                                          Qt::FastTransformation);
}

QPixmap dirt(int px)
{
    static const QVector<QRgb> pal = { 0x7a5a34, 0x8a683c, 0x6b4d2b, 0x94724a, 0x5d4125 };
    return makeNoise(px, pal, 0xEBA11A, 3);
}

QPixmap grassTop(int px)
{
    static const QVector<QRgb> pal = { 0x6aae3f, 0x7cbd4b, 0x5e9c3c, 0x86c957, 0x548f33 };
    return makeNoise(px, pal, 0xEBA11B, 3);
}

QPixmap stone(int px)
{
    static const QVector<QRgb> pal = { 0x7d7d7d, 0x8a8a8a, 0x6e6e6e, 0x959595, 0x636363 };
    return makeNoise(px, pal, 0xEBA11C, 3);
}

QPixmap deepslate(int px)
{
    static const QVector<QRgb> pal = { 0x3c3c42, 0x34343a, 0x46464d, 0x2d2d33, 0x505057 };
    return makeNoise(px, pal, 0xEBA11D, 3);
}

QPixmap grassSide(int px)
{
    QImage img(px, px, QImage::Format_RGB32);
    std::mt19937 rng(0xEBA11E);

    static const QVector<QRgb> dirtPal  = { 0x7a5a34, 0x8a683c, 0x6b4d2b, 0x94724a, 0x5d4125 };
    static const QVector<QRgb> grassPal = { 0x6aae3f, 0x7cbd4b, 0x5e9c3c, 0x86c957, 0x548f33 };

    // Jagged grass edge: each column gets grass 3..5 px deep
    QVector<int> depth(px);
    for (int x = 0; x < px; ++x)
        depth[x] = 3 + static_cast<int>(rng() % 3);

    for (int y = 0; y < px; ++y)
        for (int x = 0; x < px; ++x) {
            const bool grass = y < depth[x];
            img.setPixel(x, y, grass ? grassPal[rng() % grassPal.size()]
                                     : dirtPal[rng() % dirtPal.size()]);
        }

    const int scale = 3;
    return QPixmap::fromImage(img).scaled(px * scale, px * scale,
                                          Qt::IgnoreAspectRatio,
                                          Qt::FastTransformation);
}

}
