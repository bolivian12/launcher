#include "InstanceViews.hpp"
#include "InstanceIcons.hpp"
#include "Icons.hpp"
#include "Language.hpp"
#include <QtWidgets>
#include <algorithm>
namespace {
QString t(const char *s){return Language::key(QString::fromUtf8(s));}
const QStringList banners{":/art/f1_2.jpg",":/art/f1_6.jpg",":/art/f2_2.jpg",":/art/f2_6.jpg",":/art/f2_10.jpg",":/art/f3_6.jpg"};
constexpr int spacing=16;
// Landscape art with the instance icon in a dark badge; the HUD at the bottom of some screenshots is cropped away.
class Banner:public QWidget {
public:
    Banner(const QString &art,const QIcon &icon,int height,QWidget *parent=nullptr):QWidget(parent),m_art(art),m_icon(icon){setFixedHeight(height);setAttribute(Qt::WA_TransparentForMouseEvents);}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath clip;clip.addRoundedRect(QRectF(rect()).adjusted(0,0,0,14),11,11);p.setClipPath(clip);
        auto scaled=m_art.scaled(size(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);p.drawPixmap(0,0,scaled,(scaled.width()-width())/2,0,width(),height());
        QLinearGradient shade(0,0,0,height());shade.setColorAt(0.45,QColor(10,14,18,0));shade.setColorAt(1,QColor(10,14,18,170));p.fillRect(rect(),shade);p.setClipping(false);
        QRect box(12,height()-60,50,50);p.setBrush(QColor(12,16,22,215));p.setPen(QPen(QColor(255,255,255,45)));p.drawRoundedRect(box,10,10);m_icon.paint(&p,box.adjusted(7,7,-7,-7));
    }
private:
    QPixmap m_art;QIcon m_icon;
};
class Card:public QFrame {
public:
    std::function<void()> clicked,context;
protected:
    void mouseReleaseEvent(QMouseEvent *event) override {if(event->button()==Qt::LeftButton&&rect().contains(event->position().toPoint())&&clicked)clicked();}
    void contextMenuEvent(QContextMenuEvent *event) override {if(context)context();event->accept();}
};
QString playText(const InstanceInfo &i){return i.running?"■  "+t("Stop"):i.busy?t("Working…"):i.base.ready?"▶  "+t("Play"):"⬇  "+t("Install");}
QLabel *muted(const QString &text){auto l=new QLabel(text);l->setObjectName("muted");l->setWordWrap(true);return l;}
int count(const QString &dir,const QStringList &names,QDir::Filters filters){return QDir(dir).entryList(names,filters|QDir::NoDotAndDotDot).size();}
}
QString InstanceText::loader(const QString &loader){return loader=="neoforge"?QString("NeoForge"):loader=="vanilla"||loader.isEmpty()?QString("Vanilla"):loader.left(1).toUpper()+loader.mid(1);}
QString InstanceText::lastPlayed(qint64 seconds){
    if(seconds<=0)return t("Never played");auto ago=QDateTime::currentSecsSinceEpoch()-seconds;
    if(ago<120)return t("Played just now");if(ago<3600)return t("Played %1 min ago").arg(ago/60);if(ago<86400)return t("Played %1 h ago").arg(ago/3600);
    return t("Played %1 days ago").arg(ago/86400);
}
InstanceGrid::InstanceGrid(QWidget *parent):QWidget(parent){
    setObjectName("instanceGrid");auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,14,0,0);layout->setSpacing(12);
    auto top=new QHBoxLayout;layout->addLayout(top);top->setSpacing(6);
    for(int i=0;i<4;++i){auto tab=new QPushButton;tab->setCheckable(true);tab->setChecked(i==0);tab->setProperty("tab",true);tab->setCursor(Qt::PointingHandCursor);m_tabs<<tab;top->addWidget(tab);connect(tab,&QPushButton::clicked,this,[this,i]{m_tab=i;for(int n=0;n<m_tabs.size();++n)m_tabs[n]->setChecked(n==i);rebuild();});}
    top->addStretch();
    auto group=new QPushButton(icons::folder(18),t("New group"));group->setObjectName("newGroup");top->addWidget(group);connect(group,&QPushButton::clicked,this,[this]{if(newGroup)newGroup();});
    auto import=new QPushButton(t("Import"));import->setObjectName("importInstance");top->addWidget(import);connect(import,&QPushButton::clicked,this,[this]{if(importPack)importPack();});
    auto create=new QPushButton("＋  "+t("New instance"));create->setObjectName("newInstance");create->setProperty("play",true);top->addWidget(create);connect(create,&QPushButton::clicked,this,[this]{if(this->create)this->create();});
    m_search=new QLineEdit;m_search->setObjectName("instanceSearch");m_search->setPlaceholderText(t("Search instances…"));m_search->setClearButtonEnabled(true);layout->addWidget(m_search);
    connect(m_search,&QLineEdit::textChanged,this,[this]{rebuild();});
    m_scroll=new QScrollArea;m_scroll->setWidgetResizable(true);m_scroll->setFrameShape(QFrame::NoFrame);m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);layout->addWidget(m_scroll,1);
    rebuild();
}
int InstanceGrid::columns()const{int width=m_scroll->viewport()->width()-12;return qMax(1,(width+spacing)/(250+spacing));}
void InstanceGrid::resizeEvent(QResizeEvent *event){QWidget::resizeEvent(event);if(columns()!=m_columns)rebuild();}
void InstanceGrid::setInstances(const QList<InstanceInfo> &instances,const QStringList &emptyGroups,const QString &selected){m_items=instances;m_emptyGroups=emptyGroups;m_selected=selected;rebuild();}
void InstanceGrid::rebuild(){
    m_columns=columns();const int cardWidth=qBound(230,(m_scroll->viewport()->width()-12-(m_columns-1)*spacing)/m_columns,360);
    auto kind=[](const InstanceInfo &i){return i.modpack?3:i.base.loader=="vanilla"?1:2;};
    int counts[4]={int(m_items.size()),0,0,0};for(const auto &i:m_items)++counts[kind(i)];
    const QStringList titles{t("All"),"Vanilla",t("With mods"),t("Modpacks")};for(int n=0;n<4;++n)m_tabs[n]->setText(titles[n]+" ("+QString::number(counts[n])+")");
    const auto query=m_search->text().trimmed();
    QMap<QString,QList<InstanceInfo>> groups;
    for(const auto &i:m_items){
        if(m_tab&&kind(i)!=m_tab)continue;
        if(!query.isEmpty()&&!(i.base.name+" "+i.base.mcVersion+" "+i.base.loader+" "+i.group).contains(query,Qt::CaseInsensitive))continue;
        groups[i.group].append(i);
    }
    QStringList order=groups.keys();if(!m_tab&&query.isEmpty())for(const auto &g:m_emptyGroups)if(!order.contains(g))order<<g;
    std::sort(order.begin(),order.end(),[](const QString &a,const QString &b){if(a.isEmpty()!=b.isEmpty())return a.isEmpty();return a.compare(b,Qt::CaseInsensitive)<0;});
    const bool headers=order.size()>1||(order.size()==1&&!order.first().isEmpty());
    const int scroll=m_scroll->verticalScrollBar()->value();
    auto content=new QWidget;content->setObjectName("instanceCards");auto layout=new QVBoxLayout(content);layout->setContentsMargins(0,0,12,12);layout->setSpacing(12);
    for(const auto &name:order){
        auto items=groups.value(name);std::sort(items.begin(),items.end(),[](const InstanceInfo &a,const InstanceInfo &b){return a.base.name.compare(b.base.name,Qt::CaseInsensitive)<0;});
        if(headers){
            auto row=new QHBoxLayout;layout->addLayout(row);const bool collapsed=m_collapsed.contains(name);
            auto header=new QPushButton(icons::folder(20),(collapsed?"▸  ":"▾  ")+(name.isEmpty()?t("No group"):name)+"  ("+QString::number(items.size())+")");header->setObjectName("groupHeader");header->setCursor(Qt::PointingHandCursor);row->addWidget(header);row->addStretch();
            connect(header,&QPushButton::clicked,this,[this,name]{if(m_collapsed.contains(name))m_collapsed.remove(name);else m_collapsed.insert(name);rebuild();});
            if(!name.isEmpty()){auto gear=new QToolButton;gear->setIcon(icons::gear(18));gear->setToolTip(t("Group options"));gear->setAccessibleName(gear->toolTip());row->addWidget(gear);connect(gear,&QToolButton::clicked,this,[this,name]{if(groupMenu)groupMenu(name);});}
            if(collapsed)continue;
        }
        if(items.isEmpty()){layout->addWidget(muted(t("Empty group. Use “Change group” on an instance to move it here.")));continue;}
        auto grid=new QGridLayout;grid->setHorizontalSpacing(spacing);grid->setVerticalSpacing(spacing);layout->addLayout(grid);
        for(int n=0;n<items.size();++n){
            const auto info=items[n];const auto dir=info.base.dir;
            auto card=new Card;card->setObjectName("instanceCard");card->setProperty("dir",dir);card->setProperty("selected",dir==m_selected);card->setFixedWidth(cardWidth);card->setCursor(Qt::PointingHandCursor);
            card->setToolTip(info.base.name);card->clicked=[this,dir]{if(open)open(dir);};card->context=[this,dir]{if(menu)menu(dir);};
            auto l=new QVBoxLayout(card);l->setContentsMargins(0,0,0,12);l->setSpacing(6);
            l->addWidget(new Banner(banners[int(qHash(dir)%uint(banners.size()))],InstanceIcons::icon(info.icon,dir),qBound(104,cardWidth*9/20,150)));
            auto body=new QVBoxLayout;body->setContentsMargins(14,4,14,0);body->setSpacing(5);l->addLayout(body);
            auto title=new QLabel;title->setObjectName("cardTitle");title->setText(title->fontMetrics().elidedText(info.base.name,Qt::ElideRight,cardWidth-28));body->addWidget(title);
            auto sub=new QLabel(InstanceText::loader(info.base.loader)+" "+info.base.mcVersion+(info.modpack?"  ·  "+t("Modpack"):QString()));sub->setObjectName("muted");body->addWidget(sub);
            auto buttons=new QHBoxLayout;buttons->setSpacing(8);body->addLayout(buttons);
            auto play=new QPushButton(playText(info));play->setObjectName("cardPlay");play->setProperty(info.running?"danger":"play",true);play->setEnabled(!info.busy);play->setCursor(Qt::PointingHandCursor);buttons->addWidget(play,1);
            connect(play,&QPushButton::clicked,this,[this,dir]{if(this->play)this->play(dir);});
            auto gear=new QToolButton;gear->setObjectName("cardSettings");gear->setIcon(icons::gear(20));gear->setToolTip(t("Instance settings"));gear->setAccessibleName(gear->toolTip());gear->setCursor(Qt::PointingHandCursor);buttons->addWidget(gear);
            connect(gear,&QToolButton::clicked,this,[this,dir]{if(settings)settings(dir);});
            auto when=new QLabel(info.running?t("Running"):info.busy?t("Installing…"):!info.base.ready?t("Not installed yet"):InstanceText::lastPlayed(info.base.lastPlayed));when->setObjectName("cardFooter");body->addWidget(when);
            grid->addWidget(card,n/m_columns,n%m_columns,Qt::AlignTop|Qt::AlignLeft);
        }
        grid->setColumnStretch(m_columns,1);
    }
    if(m_items.isEmpty()){
        auto empty=new QFrame;empty->setObjectName("card");auto el=new QVBoxLayout(empty);el->setContentsMargins(30,30,30,30);el->setSpacing(10);
        auto title=new QLabel(t("A place for every world"));title->setObjectName("sectionTitle");el->addWidget(title);
        el->addWidget(muted(t("Create your first instance. You can have several of the same version with different mods; each keeps its own worlds and settings.")));
        auto create=new QPushButton("＋  "+t("New instance"));create->setProperty("play",true);create->setMaximumWidth(260);el->addWidget(create);connect(create,&QPushButton::clicked,this,[this]{if(this->create)this->create();});
        layout->addWidget(empty);
    }else if(order.isEmpty())layout->addWidget(muted(t("No instances match this search.")));
    layout->addStretch();m_scroll->setWidget(content);m_content=content;
    QTimer::singleShot(0,m_scroll,[bar=m_scroll->verticalScrollBar(),scroll]{bar->setValue(scroll);});
}
InstanceDetail::InstanceDetail(QWidget *parent):QWidget(parent){
    setObjectName("instanceDetail");auto l=new QVBoxLayout(this);l->setContentsMargins(0,12,0,0);l->setSpacing(14);
    auto backRow=new QHBoxLayout;l->addLayout(backRow);auto backButton=new QPushButton("←  "+t("All instances"));backButton->setObjectName("backButton");backButton->setProperty("link",true);backButton->setCursor(Qt::PointingHandCursor);backRow->addWidget(backButton);backRow->addStretch();
    connect(backButton,&QPushButton::clicked,this,[this]{if(back)back();});
    auto header=new QHBoxLayout;header->setSpacing(16);l->addLayout(header);
    m_icon=new QLabel;m_icon->setFixedSize(76,76);m_icon->setObjectName("detailIcon");m_icon->setAlignment(Qt::AlignCenter);header->addWidget(m_icon,0,Qt::AlignTop);
    auto info=new QVBoxLayout;info->setSpacing(8);header->addLayout(info,1);
    m_name=new QLabel;m_name->setObjectName("detailTitle");info->addWidget(m_name);
    m_state=new QLabel;m_state->setObjectName("muted");info->addWidget(m_state);info->addStretch();
    auto chipRow=new QHBoxLayout;chipRow->setSpacing(8);l->addLayout(chipRow);
    m_chips=new QWidget;auto chips=new QHBoxLayout(m_chips);chips->setContentsMargins(0,0,0,0);chips->setSpacing(8);chipRow->addWidget(m_chips,1);
    auto tools=new QHBoxLayout;tools->setSpacing(6);chipRow->addLayout(tools);
    auto tool=[this,tools](const QPixmap &icon,const QString &tip,const char *name,std::function<void()> InstanceDetail::*action){auto b=new QToolButton;b->setObjectName(name);b->setIcon(icon);b->setIconSize(QSize(22,22));b->setToolTip(tip);b->setAccessibleName(tip);b->setCursor(Qt::PointingHandCursor);tools->addWidget(b);connect(b,&QToolButton::clicked,this,[this,action]{if(this->*action)(this->*action)();});};
    tool(icons::server(22),t("Export instance"),"exportInstance",&InstanceDetail::exportZip);tool(icons::news(22),t("Log"),"showLog",&InstanceDetail::log);
    auto folderButton=new QToolButton;folderButton->setObjectName("openFolder");folderButton->setIcon(icons::folder(22));folderButton->setIconSize(QSize(22,22));folderButton->setToolTip(t("Open instance folder"));folderButton->setAccessibleName(folderButton->toolTip());tools->addWidget(folderButton);
    connect(folderButton,&QToolButton::clicked,this,[this]{if(openFolder)openFolder({});});
    tool(icons::gear(22),t("Instance settings"),"instanceSettings",&InstanceDetail::settings);
    m_play=new QPushButton;m_play->setObjectName("detailPlay");m_play->setMinimumSize(200,54);m_play->setCursor(Qt::PointingHandCursor);header->addWidget(m_play,0,Qt::AlignTop);connect(m_play,&QPushButton::clicked,this,[this]{if(play)play();});
    auto cards=new QGridLayout;cards->setSpacing(14);l->addLayout(cards);
    struct Stat{QPixmap icon;QString title,action,folder;};
    const QList<Stat> stats{{icons::mods(20),t("Mods"),t("MANAGE MODS"),"mods"},{icons::fanart(20),t("Resource packs"),t("MANAGE RESOURCE PACKS"),"resourcepacks"},{icons::release(20),t("Shader packs"),t("MANAGE SHADER PACKS"),"shaderpacks"},{icons::wiki(20),t("Worlds"),t("MANAGE WORLDS"),"saves"}};
    for(int n=0;n<stats.size();++n){
        auto card=new QFrame;card->setObjectName("statCard");card->setMinimumHeight(118);auto cl=new QVBoxLayout(card);cl->setContentsMargins(18,16,18,12);cl->setSpacing(6);
        auto titleRow=new QHBoxLayout;cl->addLayout(titleRow);auto icon=new QLabel;icon->setPixmap(stats[n].icon);titleRow->addWidget(icon);auto title=new QLabel(stats[n].title);title->setObjectName("statTitle");titleRow->addWidget(title,1);
        auto value=muted({});m_counts<<value;cl->addWidget(value);cl->addStretch();
        auto action=new QPushButton(stats[n].action);action->setProperty("link",true);action->setCursor(Qt::PointingHandCursor);cl->addWidget(action);
        const auto folder=stats[n].folder;connect(action,&QPushButton::clicked,this,[this,folder]{if(folder=="mods"){if(mods)mods();}else if(openFolder)openFolder(folder);});
        cards->addWidget(card,0,n);
    }
    auto actions=new QHBoxLayout;actions->setSpacing(8);l->addLayout(actions);
    auto action=[this,actions](const QString &text,const char *name,std::function<void()> InstanceDetail::*callback){auto b=new QPushButton(text);b->setObjectName(name);b->setCursor(Qt::PointingHandCursor);actions->addWidget(b);connect(b,&QPushButton::clicked,this,[this,callback]{if(this->*callback)(this->*callback)();});return b;};
    action(t("Find mods"),"findMods",&InstanceDetail::findMods);action(t("Save mods as a pack"),"savePack",&InstanceDetail::savePack);action(t("Copy instance"),"copyInstance",&InstanceDetail::copy);action(t("Change group"),"changeGroup",&InstanceDetail::changeGroup);
    actions->addStretch();auto remove=action(t("Delete instance"),"deleteInstance",&InstanceDetail::remove);remove->setProperty("danger",true);
    l->addStretch();
}
void InstanceDetail::showInstance(const InstanceInfo &i){
    m_dir=i.base.dir;m_icon->setPixmap(InstanceIcons::icon(i.icon,i.base.dir).pixmap(60,60));m_name->setText(i.base.name);
    auto chips=static_cast<QHBoxLayout*>(m_chips->layout());while(auto item=chips->takeAt(0)){if(item->widget())item->widget()->deleteLater();delete item;}
    auto chip=[chips](const QString &text,bool accent=false){auto c=new QLabel(text);c->setObjectName("chip");c->setProperty("accent",accent);chips->addWidget(c);};
    chip(InstanceText::loader(i.base.loader)+(i.base.loader=="vanilla"||i.loaderVersion.isEmpty()?QString():" "+i.loaderVersion),true);chip("Minecraft "+i.base.mcVersion);chip(QString::number(i.base.xmx)+" MB");
    if(!i.group.isEmpty())chip(i.group);if(i.modpack)chip(t("Modpack"));chips->addStretch();
    m_state->setText((i.running?t("Running"):i.busy?t("Installing…"):i.base.ready?t("Ready to play"):t("Not installed yet"))+"  ·  "+InstanceText::lastPlayed(i.base.lastPlayed));
    m_play->setText(playText(i));m_play->setProperty("play",!i.running);m_play->setProperty("danger",i.running);m_play->setEnabled(!i.busy);m_play->style()->unpolish(m_play);m_play->style()->polish(m_play);
    const auto dir=i.base.dir;const int enabled=count(dir+"/mods",{"*.jar"},QDir::Files),disabled=count(dir+"/mods",{"*.jar.disabled"},QDir::Files);
    m_counts[0]->setText(t("%1 mods enabled").arg(enabled)+(disabled?" · "+t("%1 disabled").arg(disabled):QString()));
    const int packs=count(dir+"/resourcepacks",{},QDir::Files|QDir::Dirs),shaders=count(dir+"/shaderpacks",{},QDir::Files|QDir::Dirs),worlds=count(dir+"/saves",{},QDir::Dirs);
    m_counts[1]->setText(packs?t("%1 resource packs").arg(packs):t("No resource packs yet"));m_counts[2]->setText(shaders?t("%1 shader packs").arg(shaders):t("No shader packs yet"));m_counts[3]->setText(worlds?t("%1 worlds").arg(worlds):t("No worlds yet"));
}
