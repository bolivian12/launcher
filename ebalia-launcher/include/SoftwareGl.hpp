#pragma once
#include <QString>
#include <QStringList>
#include <functional>
// OpenGL drawn by the processor (Mesa llvmpipe, OpenGL 4.5) for Windows computers whose graphics driver has no
// OpenGL: virtual machines and missing drivers. Minecraft 1.13 and later (LWJGL 3) load Mesa's opengl32.dll instead
// of the system one; nothing is installed in Windows.
namespace SoftwareGl {
// Windows x64 (Mesa downloaded by the launcher) and Linux (the system Mesa).
bool available();
// Downloads (checked with SHA-256) and unpacks Mesa once into <root>/runtimes; returns the folder with opengl32.dll.
QString prepare(const QString &root,std::function<void(int)> progress={});
// Java arguments and the PATH entry that make LWJGL 3 use Mesa from folder.
QStringList javaArguments(const QString &folder);
// The game log shows that the graphics driver has no usable OpenGL.
bool openGlFailure(const QString &log);
}
