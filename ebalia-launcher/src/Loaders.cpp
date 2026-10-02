#include "Loaders.hpp"
#include "ModRepository.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QXmlStreamReader>
#include <QVersionNumber>
#include <algorithm>
#include <stdexcept>
QStringList Loaders::versions(const QString &loader,const QString &game){
    QStringList out;
    if(loader=="vanilla")return out;
    if(loader=="fabric"||loader=="quilt"){
        auto base=loader=="fabric"?"https://meta.fabricmc.net/v2/versions/loader/":"https://meta.quiltmc.org/v3/versions/loader/";
        auto arr=QJsonDocument::fromJson(ModRepository::fetch(QUrl(base+game))).array();
        for(const auto &v:arr)out<<v.toObject()["loader"].toObject()["version"].toString();return out;
    }
    QString prefix=game+"-";
    if(loader=="neoforge"){
        if(!game.startsWith("1."))return out;
        auto parts=game.mid(2).split('.');prefix=parts[0]+"."+(parts.size()>1?parts[1]:"0")+".";
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
    std::sort(out.begin(),out.end(),[](const QString &a,const QString &b){return QVersionNumber::fromString(a.section('-',-1))>QVersionNumber::fromString(b.section('-',-1));});
    // Stable versions precede prereleases; retain the numerical ordering within each group.
    if(loader=="neoforge")std::stable_partition(out.begin(),out.end(),[](const QString &v){return !v.contains('-');});
    return out;
}
QString Loaders::installerUrl(const QString &loader,const QString &game,const QString &version){
    if(!ModRepository::safeName(version)||!ModRepository::safeName(game))throw std::runtime_error("Invalid loader version");
    if(loader=="forge"){auto v=version.startsWith(game+"-")?version:game+"-"+version;return "https://maven.minecraftforge.net/net/minecraftforge/forge/"+v+"/forge-"+v+"-installer.jar";}
    if(loader=="neoforge")return "https://maven.neoforged.net/releases/net/neoforged/neoforge/"+version+"/neoforge-"+version+"-installer.jar";
    throw std::runtime_error("Invalid installer loader");
}
