#pragma once
#include <QDialog>

class CreatorDialog:public QDialog {
public:
    explicit CreatorDialog(bool patreonOnly,QWidget *parent=nullptr);
};
