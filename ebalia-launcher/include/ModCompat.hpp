#pragma once
#include <QList>
#include <QString>
#include <QStringList>
// Reads what each mod jar declares (fabric.mod.json, quilt.mod.json, META-INF/neoforge.mods.toml, META-INF/mods.toml,
// mcmod.info) to find the Minecraft version and loader a folder of mods needs, and which mods do not fit.
namespace ModCompat {
struct Mod {
    QString file, name, loader; // loader: fabric, quilt, forge or neoforge
    QStringList ranges;         // Minecraft versions the mod accepts; any of them (empty: any version)
    bool maven = false;         // Forge/NeoForge ranges ("[1.20.1,1.21)"); otherwise Fabric/Quilt predicates ("~1.20.1")
};
QList<Mod> scan(const QString &modsDir);
Mod read(const QString &jar);
bool accepts(const Mod &mod, const QString &minecraft);
struct Result {
    QString minecraft, loader;  // empty minecraft: no mod says which version it needs
    QStringList problems;       // mods that do not fit, in plain words
    bool compatible() const { return problems.isEmpty(); }
};
// releases: Minecraft releases, newest first (the newest one that every mod accepts is chosen).
Result detect(const QList<Mod> &mods, const QStringList &releases);
// Mods that do not run with this Minecraft version and loader.
QStringList problems(const QList<Mod> &mods, const QString &minecraft, const QString &loader);
}
