#include "Loaders.hpp"
#include "ModRepository.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QXmlStreamReader>
#include <QVersionNumber>
#include <QRegularExpression>
#include <algorithm>
#include <stdexcept>
QStringList Loaders::versions(const QString &loader,const QString &game){
    QStringList out;
    if(loader=="vanilla")return out;
    if(loader=="fabric"||loader=="quilt"){
        auto base=loader=="fabric"?"https://meta.fabricmc.net/v2/versions/loader/":"https://meta.quiltmc.org/v3/versions/loader/";
        auto arr=QJsonDocument::fromJson(ModRepository::fetch(QUrl(base+game))).array();
        for(const auto &v:arr)out<<v.toObject()["loader"].toObject()["version"].toString();
        sort(loader,game,out);return out;
    }
    QString prefix=game+"-";
    if(loader=="neoforge"){
        // NeoForge for 1.20.1 was published as net.neoforged:forge with Forge-style versions (1.20.1-47.1.106).
        if(game=="1.20.1") {
            auto response=QJsonDocument::fromJson(ModRepository::fetch(QUrl("https://maven.neoforged.net/api/maven/versions/releases/net/neoforged/forge"))).object();
            for(const auto &item:response["versions"].toArray())if(item.toString().startsWith("1.20.1-"))out<<item.toString();
            sort(loader,game,out);return out;
        }
        prefix=neoforgePrefix(game);if(prefix.isEmpty())return out;
        // Use the repository API, with the standard Maven index as an independent fallback.
        try {
            auto response=QJsonDocument::fromJson(ModRepository::fetch(QUrl("https://maven.neoforged.net/api/maven/versions/releases/net/neoforged/neoforge"))).object();
            for(const auto &item:response["versions"].toArray())if(item.toString().startsWith(prefix))out<<item.toString();
        } catch(const std::exception &) {}
    }
    if(out.isEmpty()) {
        auto url=loader=="forge"?"https://maven.minecraftforge.net/net/minecraftforge/forge/maven-metadata.xml":"https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml";
        QXmlStreamReader xml(ModRepository::fetch(QUrl(url)));
        while(!xml.atEnd()){xml.readNext();if(xml.isStartElement()&&xml.name()==QLatin1String("version")){auto v=xml.readElementText();if(v.startsWith(prefix))out<<v;}}
        if(xml.hasError())throw std::runtime_error("Invalid loader catalog");
    }
    sort(loader,game,out);
    return out;
}
QString Loaders::neoforgePrefix(const QString &game){
    // 1.21.1 -> 21.1.x; since Minecraft 26.1 NeoForge repeats the game version: 26.3 -> 26.3.0.x, 26.1.2 -> 26.1.2.x.
    auto parts=game.split('.');if(parts.size()<2)return {};
    if(game.startsWith("1."))return parts[1]+"."+(parts.size()>2?parts[2]:"0")+".";
    return parts[0]+"."+parts[1]+"."+(parts.size()>2?parts[2]:"0")+".";
}
void Loaders::sort(const QString &loader,const QString &game,QStringList &out){
    if(loader=="fabric"||loader=="quilt"){
        // Quilt's catalog is not ordered ("0.20.0-beta.9" comes before "0.30.1"): newest first, releases before betas.
        auto parts=[](const QString &v){
            QList<int> digits;auto matches=QRegularExpression("\\d+").globalMatch(v.section('-',1));while(matches.hasNext())digits<<matches.next().captured().toInt();
            return std::pair{QVersionNumber::fromString(v.section('-',0,0)),QVersionNumber(digits)};};
        std::stable_sort(out.begin(),out.end(),[&parts](const QString &a,const QString &b){
            const auto [ab,ap]=parts(a);const auto [bb,bp]=parts(b);
            if(ab!=bb)return ab>bb;
            const bool as=!a.contains('-'),bs=!b.contains('-');if(as!=bs)return as;
            return ap>bp;});
        std::stable_partition(out.begin(),out.end(),[](const QString &v){return !v.contains('-');});
        return;
    }
    // Forge: "1.20.1-47.4.26" (some old ones end in "-1.7.10"); NeoForge: "21.1.77" or "26.3.0.52-beta".
    const auto forgePrefix=game+"-";
    auto number=[&](const QString &v){return QVersionNumber::fromString(v.startsWith(forgePrefix)?v.mid(forgePrefix.size()).section('-',0,0):v.section('-',0,0));};
    std::stable_sort(out.begin(),out.end(),[&number](const QString &a,const QString &b){return number(a)>number(b);});
    // Stable versions precede prereleases; retain the numerical ordering within each group.
    if(loader=="neoforge")std::stable_partition(out.begin(),out.end(),[](const QString &v){return !v.contains('-');});
}
QString Loaders::installerUrl(const QString &loader,const QString &game,const QString &version){
    if(!ModRepository::safeName(version)||!ModRepository::safeName(game))throw std::runtime_error("Invalid loader version");
    if(loader=="forge"){auto v=version.startsWith(game+"-")?version:game+"-"+version;return "https://maven.minecraftforge.net/net/minecraftforge/forge/"+v+"/forge-"+v+"-installer.jar";}
    if(loader=="neoforge"&&version.startsWith("1.20.1-"))return "https://maven.neoforged.net/releases/net/neoforged/forge/"+version+"/forge-"+version+"-installer.jar";
    if(loader=="neoforge")return "https://maven.neoforged.net/releases/net/neoforged/neoforge/"+version+"/neoforge-"+version+"-installer.jar";
    throw std::runtime_error("Invalid installer loader");
}
