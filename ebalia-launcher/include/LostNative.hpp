#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <functional>
// Starts a lost version with Java directly, the same way on Windows, macOS and Linux,
// from the "native" description of its catalog entry (resources/versions.json):
//   java       Java major version (8 for every LWJGL 2 game)
//   workingDir folder the game runs in, relative to the installed package
//   classpath  package files relative to the package root; "@lwjgl2" stands for Mojang's
//              official LWJGL 2 and JInput jars for the running system, "@launchwrapper" for
//              Mojang's launchwrapper 1.5 (with jopt-simple and ASM)
//   mainClass, jvmArgs, gameArgs
//   natives    "lwjgl2": Mojang's natives for the running system are extracted for the game
//   stdin      lines written to the game's standard input (MultiMC's NewLaunch reads its script there)
// Every version gets its own home (user.home and, on Windows, APPDATA) inside the package.
// Placeholders in arguments: ${username}, ${installDir}, ${workingDir}, ${natives}.
namespace LostNative {
struct Command {QString workingDir;QStringList arguments;QMap<QString,QString> environment;QByteArray input;};
bool available(const QJsonObject &spec);
// "windows", "osx" or "linux": the Mojang classifier of this system.
QString system();
// Downloads (checked with SHA-1) the shared libraries the description needs into librariesDir
// and extracts this system's natives into the package. Safe to call before every start.
void prepare(const QJsonObject &spec,const QString &installDir,const QString &librariesDir,std::function<void(int,int)> progress={});
// Java arguments (without the Java executable) and the folder to start in.
Command command(const QJsonObject &spec,const QString &installDir,const QString &librariesDir,const QString &username);
}
