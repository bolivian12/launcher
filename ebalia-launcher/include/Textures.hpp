#pragma once

#include <QPixmap>

// Procedural Minecraft-style 16x16 block textures (no asset files needed).
// Each returns an upscaled tileable pixmap with nearest-neighbor pixels.
namespace textures {

QPixmap dirt(int px = 16);
QPixmap grassTop(int px = 16);
QPixmap grassSide(int px = 16);   // dirt with a jagged grass strip on top
QPixmap stone(int px = 16);
QPixmap deepslate(int px = 16);

}
