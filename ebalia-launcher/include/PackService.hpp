#pragma once
#include "ModRepository.hpp"
class PackService {
public:
    explicit PackService(QString root,ModRepository::Transport transport={}):m_root(std::move(root)),m_transport(std::move(transport)){}
    QJsonArray search(const QString &provider,const QString &query,int page=0);
    QJsonArray versions(const QJsonObject &pack);
    QString install(const QJsonObject &pack,const QJsonObject &version,const QString &name,const QString &group={},int memory=4096);
    static QString safePath(QString path);
    // Instances of Prism Launcher, PolyMC, MultiMC, CurseForge and the FTB App found on this computer.
    static QJsonArray localInstances();
    static void components(const QJsonObject &pack,QJsonObject &config);
private:
    QJsonDocument get(const QString &url,bool curse=false);
    QByteArray bytes(const QString &url,bool curse=false);
    QJsonObject unpack(const QString &archive,const QString &stage);
    QJsonObject importFolder(QString base,const QString &stage);
    QJsonObject ftbManifest(const QJsonObject &data,const QString &stage);
    void download(const QString &url,const QString &destination,const QJsonObject &hashes={});
    void copyTree(const QString &from,const QString &to,const QStringList &skip={});
    void profileLoader(const QJsonObject &profile,QJsonObject &config);
    QString m_root;
    ModRepository::Transport m_transport;
};
