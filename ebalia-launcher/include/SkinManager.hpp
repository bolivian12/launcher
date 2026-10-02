#pragma once
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
class SkinManager {
public:
    explicit SkinManager(QString root):m_root(std::move(root)){}
    QJsonObject importSkin(const QString &file,const QString &name,const QString &variant);
    QJsonArray skins() const;
    static QImage preview(const QImage &skin,const QString &variant,bool back=false);
    static void upload(const QString &file,const QString &variant,const QString &token);
    static void applyLocal(const QString &file,const QString &variant,const QString &instance,const QString &clientJar);
    static void removeLocal(const QString &instance);
private:
    QString m_root;
};
