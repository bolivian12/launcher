#pragma once
#include <QList>
#include <QString>
// Reads a game log after Minecraft closed with an error and explains, in a few words, what went wrong.
namespace CrashReport {
struct Finding {
    enum Kind { MissingMod, WrongModVersion, IncompatibleMods, GraphicsDriver, NoOpenGL, JavaVersion, OutOfMemory };
    Kind kind;
    QString mod;        // MissingMod / WrongModVersion: the mod id that is needed ("terrablender")
    QString requiredBy; // the mod that needs it ("biomesoplenty")
    QString versions;   // the versions it accepts ("26.3.0.0.6 or later"), as the log says
    QString vendor;     // GraphicsDriver: "AMD", "NVIDIA" or "Intel"
    QString detail;     // the log line it comes from
};
QList<Finding> analyze(const QString &log);
// Official driver download page for "AMD", "NVIDIA" or "Intel".
QString driverPage(const QString &vendor);
}
