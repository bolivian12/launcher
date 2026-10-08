#include "ModCompat.hpp"
#include "Archive.hpp"
#include "Language.hpp"
#include <QtCore>
#include <algorithm>
#include <QSet>
namespace {
const QRegularExpression versionPattern(R"((\d+)\.(\d+)(?:\.(\d+))?)");
// "1.20.1" → 1.20.1; "1.20-pre1", "1.20.1-rc.1" → their release.
QVersionNumber number(const QString &text){auto m=versionPattern.match(text);if(!m.hasMatch())return {};return QVersionNumber::fromString(m.captured(0));}
QVersionNumber full(QVersionNumber v){return v.isNull()?v:QVersionNumber(v.majorVersion(),v.minorVersion(),v.microVersion());}
// Fabric/Quilt predicate: space-separated terms that all hold. "*", "1.20.x", ">=1.20 <1.21", "~1.20.1", "^1.20", "1.20.1".
bool fabricAccepts(QString predicate,const QVersionNumber &mc){
    predicate=predicate.trimmed();if(predicate.isEmpty()||predicate=="*")return true;
    for(auto term:predicate.split(' ',Qt::SkipEmptyParts)){
        QString op;for(auto o:{">=","<=",">","<","=","~","^"})if(term.startsWith(o)){op=o;term=term.mid(QString(o).size());break;}
        if(term=="*"||term.isEmpty())continue;
        const bool wildcard=term.contains(".x")||term.contains(".X")||term.contains(".*");
        auto v=full(number(term));if(v.isNull())return false; // a snapshot name: not a release
        const auto c=full(mc);
        if(wildcard){auto parts=term.split('.');int fixed=0;for(const auto &p:parts){if(p=="x"||p=="X"||p=="*")break;++fixed;}
            if(fixed>=1&&c.majorVersion()!=v.majorVersion())return false;if(fixed>=2&&c.minorVersion()!=v.minorVersion())return false;if(fixed>=3&&c.microVersion()!=v.microVersion())return false;continue;}
        const int cmp=QVersionNumber::compare(c,v);
        if(op==">="){if(cmp<0)return false;}else if(op=="<="){if(cmp>0)return false;}else if(op==">"){if(cmp<=0)return false;}else if(op=="<"){if(cmp>=0)return false;}
        else if(op=="~"){if(cmp<0||c.majorVersion()!=v.majorVersion()||c.minorVersion()!=v.minorVersion())return false;}
        else if(op=="^"){if(cmp<0||c.majorVersion()!=v.majorVersion())return false;}
        else if(cmp!=0)return false;
    }
    return true;
}
// Maven range set: "[1.20.1,1.21)", "[1.20.1]", "[1.20,)", "(,1.21)", several joined by commas. A bare version is only a preference.
bool mavenAccepts(const QString &spec,const QVersionNumber &mc){
    const auto text=spec.trimmed();if(text.isEmpty()||text=="*"||!(text.startsWith('[')||text.startsWith('(')))return true;
    static const QRegularExpression range(R"(([\[\(])\s*([^,\]\)]*)\s*(?:,\s*([^\]\)]*))?\s*([\]\)]))");
    auto it=range.globalMatch(text);const auto c=full(mc);
    while(it.hasNext()){
        auto m=it.next();const bool single=m.captured(3).isNull()&&!m.captured(0).contains(',');
        auto low=full(number(m.captured(2))),high=full(number(m.captured(3)));
        if(single){if(!low.isNull()&&QVersionNumber::compare(c,low)==0)return true;continue;}
        bool ok=true;
        if(!low.isNull()){int cmp=QVersionNumber::compare(c,low);ok=ok&&(m.captured(1)=="["?cmp>=0:cmp>0);}
        if(!high.isNull()){int cmp=QVersionNumber::compare(c,high);ok=ok&&(m.captured(4)=="]"?cmp<=0:cmp<0);}
        if(ok)return true;
    }
    return false;
}
QString tomlMinecraftRange(const QByteArray &toml){
    // [[dependencies.<mod>]] blocks: the one whose modId is "minecraft".
    for(const auto &block:QString::fromUtf8(toml).split("[[dependencies",Qt::SkipEmptyParts)){
        static const QRegularExpression id(R"(modId\s*=\s*["']minecraft["'])"),range(R"(versionRange\s*=\s*["']([^"']*)["'])");
        if(id.match(block).hasMatch()){auto m=range.match(block);if(m.hasMatch())return m.captured(1);}
    }
    return {};
}
QString tomlName(const QByteArray &toml){static const QRegularExpression name(R"(displayName\s*=\s*["']([^"']+)["'])");auto m=name.match(QString::fromUtf8(toml));return m.hasMatch()?m.captured(1):QString();}
QStringList strings(const QJsonValue &v){QStringList out;if(v.isString())out<<v.toString();else for(const auto &x:v.toArray())if(x.isString())out<<x.toString();return out;}
QString family(const QString &loader){return loader=="fabric"||loader=="quilt"?"fabric":"forge";}
}
namespace ModCompat {
Mod read(const QString &jar){
    Mod mod;mod.file=QFileInfo(jar).fileName();mod.name=mod.file;
    auto entry=[&](const char *name){try{return Archive::readEntry(jar,name,1024*1024);}catch(...){return QByteArray();}};
    if(auto quilt=entry("quilt.mod.json");!quilt.isEmpty()){
        auto o=QJsonDocument::fromJson(quilt).object()["quilt_loader"].toObject();mod.loader="quilt";mod.name=o["metadata"].toObject()["name"].toString(o["id"].toString(mod.file));
        for(const auto &d:o["depends"].toArray()){auto dep=d.toObject();if(dep["id"].toString()=="minecraft"){auto v=dep["versions"];if(v.isObject())v=v.toObject()["any"];mod.ranges=strings(v);}}
        return mod;
    }
    if(auto fabric=entry("fabric.mod.json");!fabric.isEmpty()){
        auto o=QJsonDocument::fromJson(fabric).object();mod.loader="fabric";mod.name=o["name"].toString(o["id"].toString(mod.file));
        mod.ranges=strings(o["depends"].toObject()["minecraft"]);return mod;
    }
    for(auto [file,loader]:{std::pair{"META-INF/neoforge.mods.toml","neoforge"},std::pair{"META-INF/mods.toml","forge"}}){
        if(auto toml=entry(file);!toml.isEmpty()){mod.loader=loader;mod.maven=true;auto name=tomlName(toml);if(!name.isEmpty()&&!name.contains("${"))mod.name=name;auto r=tomlMinecraftRange(toml);if(!r.isEmpty()&&!r.contains("${"))mod.ranges<<r;return mod;}
    }
    if(auto info=entry("mcmod.info");!info.isEmpty()){
        auto doc=QJsonDocument::fromJson(info);auto list=doc.isArray()?doc.array():doc.object()["modList"].toArray();mod.loader="forge";mod.maven=true;
        if(!list.isEmpty()){auto o=list[0].toObject();mod.name=o["name"].toString(mod.file);auto mc=o["mcversion"].toString();if(!number(mc).isNull())mod.ranges<<"["+mc+"]";}
        return mod;
    }
    return mod; // a library without mod metadata
}
QByteArray iconData(const QString &jar){
    auto entry=[&](const QString &name){if(name.isEmpty())return QByteArray();try{return Archive::readEntry(jar,QString(name).remove(QRegularExpression("^/")),4*1024*1024);}catch(...){return QByteArray();}};
    QStringList candidates;
    for(auto file:{"quilt.mod.json","fabric.mod.json"}){
        const auto o=QJsonDocument::fromJson(entry(file)).object();auto icon=file==QString("quilt.mod.json")?o["quilt_loader"].toObject()["metadata"].toObject()["icon"]:o["icon"];
        if(icon.isString())candidates<<icon.toString();
        else if(icon.isObject()){int best=0;QString path;const auto sizes=icon.toObject();for(auto it=sizes.begin();it!=sizes.end();++it)if(it.key().toInt()>=best){best=it.key().toInt();path=it.value().toString();}candidates<<path;}
    }
    for(auto file:{"META-INF/neoforge.mods.toml","META-INF/mods.toml"}){static const QRegularExpression logo(R"re(logoFile\s*=\s*["']([^"']+)["'])re");auto m=logo.match(QString::fromUtf8(entry(file)));if(m.hasMatch())candidates<<m.captured(1);}
    {auto doc=QJsonDocument::fromJson(entry("mcmod.info"));auto list=doc.isArray()?doc.array():doc.object()["modList"].toArray();if(!list.isEmpty())candidates<<list[0].toObject()["logoFile"].toString();}
    candidates<<"pack.png"<<"icon.png"<<"logo.png";
    for(const auto &c:candidates){auto data=entry(c);if(!data.isEmpty())return data;}
    return {};
}
QList<Mod> scan(const QString &dir){
    QList<Mod> out;for(const auto &f:QDir(dir).entryList({"*.jar"},QDir::Files,QDir::Name)){auto m=read(dir+"/"+f);if(!m.loader.isEmpty())out<<m;}
    return out;
}
bool accepts(const Mod &mod,const QString &minecraft){
    const auto mc=number(minecraft);if(mc.isNull())return false;if(mod.ranges.isEmpty())return true;
    for(const auto &r:mod.ranges)if(mod.maven?mavenAccepts(r,mc):fabricAccepts(r,mc))return true;
    return false;
}
QStringList problems(const QList<Mod> &mods,const QString &minecraft,const QString &loader){
    QStringList out;
    for(const auto &m:mods){
        const bool loaderFits=loader==m.loader||(loader=="quilt"&&m.loader=="fabric")||(loader=="neoforge"&&m.loader=="forge"&&minecraft=="1.20.1");
        if(!loaderFits)out<<Language::text("%1 es un mod de %2 y esta instancia usa %3","%1 is a %2 mod and this instance uses %3","%1 é um mod de %2 e esta instância usa %3").arg(m.name,m.loader,loader);
        else if(!accepts(m,minecraft))out<<Language::text("%1 necesita Minecraft %2","%1 needs Minecraft %2","%1 precisa do Minecraft %2").arg(m.name,m.ranges.join(" / "));
    }
    return out;
}
Result detect(const QList<Mod> &mods,const QStringList &releases){
    Result r;if(mods.isEmpty())return r;
    QMap<QString,int> count;for(const auto &m:mods)count[m.loader]++;
    const int fabricFamily=count.value("fabric")+count.value("quilt"),forgeFamily=count.value("forge")+count.value("neoforge");
    // Quilt also runs Fabric mods; NeoForge for 1.20.1 also runs Forge mods.
    r.loader=fabricFamily>=forgeFamily?(count.value("quilt")?"quilt":"fabric"):(count.value("neoforge")?"neoforge":"forge");
    QList<Mod> same;for(const auto &m:mods)if(family(m.loader)==family(r.loader))same<<m;
    // Candidates: Mojang's releases, or the versions the mods name when the list is not available.
    QStringList candidates=releases;
    if(candidates.isEmpty()){QList<QVersionNumber> named;for(const auto &m:same)for(const auto &range:m.ranges){auto it=versionPattern.globalMatch(range);while(it.hasNext()){auto v=full(QVersionNumber::fromString(it.next().captured(0)));if(!named.contains(v))named<<v;}}
        std::sort(named.begin(),named.end(),[](auto a,auto b){return QVersionNumber::compare(a,b)>0;});for(const auto &v:named)candidates<<(v.microVersion()==0&&v.segmentCount()>=2?QString("%1.%2").arg(v.majorVersion()).arg(v.minorVersion()):v.toString());}
    // The version the most mods accept; among those, the one the mods name most often (packs are made for the
    // version their mods name, "~1.20.1" means 1.20.1 more than 1.20.4), then the newest.
    QHash<QString,int> named;for(const auto &m:same)for(const auto &range:m.ranges){QSet<QString> once;auto it=versionPattern.globalMatch(range);while(it.hasNext())once.insert(full(QVersionNumber::fromString(it.next().captured(0))).toString());for(const auto &v:once)named[v]++;}
    int bestFits=-1,bestNamed=-1;
    for(const auto &c:candidates){
        int fits=0;for(const auto &m:same)fits+=accepts(m,c);
        const int mentions=named.value(full(QVersionNumber::fromString(c)).toString());
        if(fits>bestFits||(fits==bestFits&&mentions>bestNamed)){bestFits=fits;bestNamed=mentions;r.minecraft=c;}
    }
    if(r.loader=="neoforge"&&count.value("forge")&&r.minecraft!="1.20.1")r.loader=count.value("neoforge")>=count.value("forge")?"neoforge":"forge";
    r.problems=problems(mods,r.minecraft,r.loader);
    return r;
}
}
