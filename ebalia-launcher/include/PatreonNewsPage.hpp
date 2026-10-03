#pragma once
#include <QWidget>
class PatreonAuth;
class PatreonNewsPage:public QWidget {
public:
    explicit PatreonNewsPage(PatreonAuth *auth,QWidget *parent=nullptr);
};
