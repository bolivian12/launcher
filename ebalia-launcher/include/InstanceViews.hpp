#pragma once
#include <QWidget>
#include <QSet>
#include <QPixmap>
#include <functional>
#include "McInstanceManager.hpp"
class QVBoxLayout;class QLineEdit;class QLabel;class QPushButton;class QGridLayout;class QScrollArea;
struct InstanceInfo {
    McInstance base;
    QString group,icon,loaderVersion;
    bool modpack=false,running=false,busy=false;
};
// Instance library: grouped cards with a Play button, like the profile pages of modern launchers.
class InstanceGrid:public QWidget {
public:
    explicit InstanceGrid(QWidget *parent=nullptr);
    void setInstances(const QList<InstanceInfo> &instances,const QStringList &emptyGroups,const QString &selected);
    std::function<void(const QString &dir)> open,play,settings,menu;
    std::function<void()> create,importPack,newGroup;
    std::function<void(const QString &group)> groupMenu;
protected:
    void resizeEvent(QResizeEvent *event) override;
private:
    void rebuild();
    int columns() const;
    QList<InstanceInfo> m_items;QStringList m_emptyGroups;QString m_selected;QSet<QString> m_collapsed;
    QLineEdit *m_search;QLabel *m_summary;QScrollArea *m_scroll;QWidget *m_content=nullptr;QList<QPushButton*> m_tabs;int m_tab=0,m_columns=0;
};
// One instance over its artwork: chips with version and loader, a large Play button and cards for mods, resource packs, shaders and worlds.
class InstanceDetail:public QWidget {
public:
    explicit InstanceDetail(QWidget *parent=nullptr);
    void showInstance(const InstanceInfo &instance);
    QString dir() const {return m_dir;}
    std::function<void()> back,play,settings,log,exportZip,copy,remove,mods,findMods,savePack,changeGroup;
    std::function<void(const QString &folder)> openFolder;
protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    void arrange();
    QGridLayout *m_cardGrid;QList<QWidget*> m_cards,m_cardIcons;QList<QPair<QPushButton*,QString>> m_actions;bool m_compact=false;int m_cardColumns=0;
    QPixmap m_backdrop,m_scaled;QString m_dir;QLabel *m_icon,*m_name,*m_state;QWidget *m_chips;QPushButton *m_play;
    QList<QLabel*> m_counts;
};
namespace InstanceText {QString lastPlayed(qint64 seconds);QString loader(const QString &loader);}
