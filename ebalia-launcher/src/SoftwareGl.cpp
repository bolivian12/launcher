#include "SoftwareGl.hpp"
#include "Archive.hpp"
#include "Download.hpp"
#include <QtCore>
#include <stdexcept>
namespace {
// pal1000/mesa-dist-win, MSVC release build.
const char *version="26.2.4";
const char *url="https://github.com/pal1000/mesa-dist-win/releases/download/26.2.4/mesa3d-26.2.4-release-msvc.7z";
const char *sha256="351fc8c8b695878ffb3eaa044b3ead08672a48b1a045e3c3e3975811df0f6695";
const QStringList files{"opengl32.dll","libgallium_wgl.dll"};
}
namespace SoftwareGl {
bool available(){
#if defined(Q_OS_WIN)
    return QSysInfo::currentCpuArchitecture()=="x86_64";
#elif defined(Q_OS_LINUX)
    return true; // the system Mesa has llvmpipe: LIBGL_ALWAYS_SOFTWARE
#else
    return false;
#endif
}
QString prepare(const QString &root,std::function<void(int)> progress){
    const auto folder=root+"/runtimes/mesa-"+QString(version)+"-x64";const auto marker=folder+"/.complete";
    if(QFile::exists(marker))return folder;
    QDir().mkpath(root+"/runtimes");const auto archive=root+"/runtimes/mesa-"+QString(version)+".7z";
    Download::file(QUrl(url),archive,QByteArray(sha256),QCryptographicHash::Sha256,[&](qint64 done,qint64 total){if(progress&&total>0)progress(int(done*100/total));});
    QDir(folder).removeRecursively();
    QStringList wanted;for(const auto &f:files)wanted<<"x64/"+f;
    Archive::extractFiles(archive,folder,wanted);QFile::remove(archive);
    for(const auto &f:files)if(!QFile::exists(folder+"/"+f))throw std::runtime_error("The software OpenGL package is incomplete.");
    QFile done(marker);if(!done.open(QIODevice::WriteOnly))throw std::runtime_error("Could not finish the software OpenGL package.");
    return folder;
}
QStringList javaArguments(const QString &folder){
    return {"-Dorg.lwjgl.opengl.libname="+QDir::toNativeSeparators(folder+"/opengl32.dll")};
}
bool openGlFailure(const QString &log){
    static const QStringList signs{"WGL: The driver does not appear to support OpenGL","WGL: driver does not appear to support OpenGL",
        "No context is current or a function that is not available in the current context","GLFW error 65542","GLFW error 65543",
        "Pixel format not accelerated","Couldn't set pixel format","OpenGL 3.2 or later is required","Failed to create OpenGL context",
        "The driver does not appear to support OpenGL"};
    for(const auto &sign:signs)if(log.contains(sign,Qt::CaseInsensitive))return true;
    return false;
}
}
