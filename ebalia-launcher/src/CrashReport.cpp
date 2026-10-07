#include "CrashReport.hpp"
#include "SoftwareGl.hpp"
#include <QRegularExpression>
#include <QSet>
namespace CrashReport {
QList<Finding> analyze(const QString &log){
    QList<Finding> out;QSet<QString> seen;
    auto add=[&](Finding f){const auto key=QString::number(f.kind)+"|"+f.mod+"|"+f.requiredBy+"|"+f.vendor;if(seen.contains(key))return;seen.insert(key);out<<f;};
    // Forge and NeoForge: "Mod ID: 'terrablender', Requested by: 'biomesoplenty', Expected range: '[26.3.0.0.6,)', Actual version: '[MISSING]'"
    static const QRegularExpression forge(R"(Mod ID: '([^']+)', Requested by: '([^']+)', Expected range: '([^']*)', Actual version: '([^']*)')");
    for(auto it=forge.globalMatch(log);it.hasNext();){auto m=it.next();if(m.captured(1)=="minecraft"||m.captured(1)=="forge"||m.captured(1)=="neoforge")continue;
        add({m.captured(4)=="[MISSING]"?Finding::MissingMod:Finding::WrongModVersion,m.captured(1),m.captured(2),m.captured(3),{},m.captured(0)});}
    // Fabric and Quilt: "Mod 'Biomes O' Plenty' (biomesoplenty) 1.0 requires version 2.0 or later of terrablender, which is missing!"
    static const QRegularExpression fabricMissing(R"(Mod '([^']+)' \(([^)]+)\) \S+ requires (.+?) of (?:mod '[^']*' \()?([A-Za-z0-9_.\-]+)\)?, which is missing)");
    for(auto it=fabricMissing.globalMatch(log);it.hasNext();){auto m=it.next();if(m.captured(4)=="minecraft"||m.captured(4)=="fabricloader"||m.captured(4)=="java")continue;
        auto versions=m.captured(3);versions.remove(QRegularExpression("^(any )?version(s)? ?"));add({Finding::MissingMod,m.captured(4),m.captured(2),versions,{},m.captured(0)});}
    static const QRegularExpression fabricWrong(R"(Mod '([^']+)' \(([^)]+)\) \S+ requires (.+?) of (?:mod '[^']*' \()?([A-Za-z0-9_.\-]+)\)?, but only the wrong version is present)");
    for(auto it=fabricWrong.globalMatch(log);it.hasNext();){auto m=it.next();add({Finding::WrongModVersion,m.captured(4),m.captured(2),m.captured(3),{},m.captured(0)});}
    static const QRegularExpression fabricIncompatible(R"(Mod '([^']+)' \(([^)]+)\) \S+ is incompatible with[^\n]*)");
    for(auto it=fabricIncompatible.globalMatch(log);it.hasNext();){auto m=it.next();add({Finding::IncompatibleMods,{},m.captured(2),{},{},m.captured(0)});}
    // A crash inside the graphics driver: "Problematic frame: C [atio6axx.dll+0x192b60]"
    static const QRegularExpression frame(R"(Problematic frame:\s*\n?#?\s*C\s+\[([^\]+]+))");
    if(auto m=frame.match(log);m.hasMatch()){
        const auto lib=m.captured(1).toLower();QString vendor;
        if(lib.startsWith("atio")||lib.startsWith("atig")||lib.startsWith("amdxc")||lib.contains("radeonsi"))vendor="AMD";
        else if(lib.startsWith("nvoglv")||lib.startsWith("nvwgf")||lib.contains("libnvidia"))vendor="NVIDIA";
        else if(lib.startsWith("ig")&&lib.contains("icd")||lib.startsWith("igxelp")||lib.contains("iris_dri")||lib.contains("i965"))vendor="Intel";
        if(!vendor.isEmpty())add({Finding::GraphicsDriver,{},{},{},vendor,m.captured(0).simplified()});
    }
    if(SoftwareGl::openGlFailure(log))add({Finding::NoOpenGL,{},{},{},{},{}});
    static const QRegularExpression classVersion(R"(class file version (\d+)\.0\) ?[^\n]*?up to (\d+)\.0)");
    if(auto m=classVersion.match(log);m.hasMatch())add({Finding::JavaVersion,{},{},QString::number(m.captured(1).toInt()-44),{},m.captured(0)});
    if(log.contains("java.lang.OutOfMemoryError"))add({Finding::OutOfMemory,{},{},{},{},"java.lang.OutOfMemoryError"});
    return out;
}
QString driverPage(const QString &vendor){
    if(vendor=="AMD")return "https://www.amd.com/en/support/download/drivers.html";
    if(vendor=="NVIDIA")return "https://www.nvidia.com/en-us/drivers/";
    if(vendor=="Intel")return "https://www.intel.com/content/www/us/en/support/detect.html";
    return {};
}
}
