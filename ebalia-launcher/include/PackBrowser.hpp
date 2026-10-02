#pragma once
#include <QDialog>
#include <QJsonObject>
class PackBrowser:public QDialog {
public:
    PackBrowser(const QString &root,const QString &provider,QWidget *parent=nullptr);
    QJsonObject selectedPack,selectedVersion;
};
