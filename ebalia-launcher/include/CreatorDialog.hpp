#pragma once
#include <QDialog>
#include <QList>
#include <QString>
// EBALIA's own mods, shown on Home and in this dialog. Pictures: :/creations/<slug>.png
struct CreatorMod { QString name, slug, description; bool soon; QString url() const; };
QList<CreatorMod> creatorMods();

class CreatorDialog:public QDialog {
public:
    explicit CreatorDialog(bool patreonOnly,QWidget *parent=nullptr);
};
