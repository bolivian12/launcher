#pragma once
#include <QWidget>
class PatreonAuth;
class CommunityPage:public QWidget {
public:
    CommunityPage(int section,const QString &root,PatreonAuth *auth,QWidget *parent=nullptr);
};
