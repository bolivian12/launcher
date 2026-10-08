#include "InstanceViews.hpp"
#include "InstanceIcons.hpp"
#include "Language.hpp"
#include "Ui.hpp"
#include "ServerList.hpp"
#include <QtWidgets>
#include <algorithm>
namespace {
QString t(const char *s){return Language::key(QString::fromUtf8(s));}
constexpr int spacing=16;
const QColor light(225,225,230);
// Landscape art with the instance icon in a dark badge; the HUD at the bottom of some screenshots is cropped away.
class Banner:public QWidget {
public:
    Banner(const QString &art,const QIcon &icon,int height,QWidget *parent=nullptr):QWidget(parent),m_icon(icon){setFixedHeight(height);setAttribute(Qt::WA_TransparentForMouseEvents);Ui::loadArt(art,{768,432},this,[this](const QPixmap &p){m_art=p;m_scaled={};update();});}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setRenderHint(QPainter::SmoothPixmapTransform);
        QPainterPath clip;clip.addRoundedRect(QRectF(rect()).adjusted(0,0,0,14),13,13);p.setClipPath(clip);
        if(m_scaled.size()!=size())m_scaled=Ui::cover(m_art,size());p.drawPixmap(0,0,m_scaled);
        QLinearGradient shade(0,0,0,height());shade.setColorAt(0.4,QColor(14,14,16,0));shade.setColorAt(1,QColor(14,14,16,190));p.fillRect(rect(),shade);p.setClipping(false);
        QRect box(12,height()-58,48,48);p.setBrush(QColor(18,18,20,225));p.setPen(QPen(QColor(255,255,255,40)));p.drawRoundedRect(box,11,11);m_icon.paint(&p,box.adjusted(7,7,-7,-7));
    }
private:
    QPixmap m_art,m_scaled;QIcon m_icon;
};
class Card:public QFrame {
public:
    std::function<void()> clicked,context;
protected:
    void mouseReleaseEvent(QMouseEvent *event) override {if(event->button()==Qt::LeftButton&&rect().contains(event->position().toPoint())&&clicked)clicked();}
    void contextMenuEvent(QContextMenuEvent *event) override {if(context)context();event->accept();}
};
QString playText(const InstanceInfo &i){return i.running?t("Stop"):i.busy?t("Working…"):t("Play");} // Play also installs what is missing
QString playIcon(const InstanceInfo &i){return i.running?"square":i.busy?QString():"play";}
QLabel *muted(const QString &text){auto l=new QLabel(text);l->setObjectName("muted");l->setWordWrap(true);return l;}
int count(const QString &dir,const QStringList &names,QDir::Filters filters){return QDir(dir).entryList(names,filters|QDir::NoDotAndDotDot).size();}
QPushButton *iconButton(const QString &text,const QString &icon,const char *name,bool primary=false){
    auto b=new QPushButton(icon.isEmpty()?text:" "+text);b->setObjectName(name);b->setCursor(Qt::PointingHandCursor);if(!icon.isEmpty()){b->setIcon(Ui::icon(icon,primary?QColor(Qt::white):light));b->setIconSize(QSize(18,18));}
    if(primary)b->setProperty("play",true);return b;
}
QToolButton *toolButton(const QString &icon,const QString &tip,const char *name){
    auto b=new QToolButton;b->setObjectName(name);b->setIcon(Ui::icon(icon));b->setIconSize(QSize(20,20));b->setToolTip(tip);b->setAccessibleName(tip);b->setCursor(Qt::PointingHandCursor);return b;
}
}
QString InstanceText::loader(const QString &loader){return loader=="neoforge"?QString("NeoForge"):loader=="vanilla"||loader.isEmpty()?QString("Vanilla"):loader.left(1).toUpper()+loader.mid(1);}
QString InstanceText::lastPlayed(qint64 seconds){
    if(seconds<=0)return t("Never played");auto ago=QDateTime::currentSecsSinceEpoch()-seconds;
    if(ago<120)return t("Played just now");if(ago<3600)return t("Played %1 min ago").arg(ago/60);if(ago<86400)return t("Played %1 h ago").arg(ago/3600);
    return t("Played %1 days ago").arg(ago/86400);
}
InstanceGrid::InstanceGrid(QWidget *parent):QWidget(parent){
    setObjectName("instanceGrid");auto layout=new QVBoxLayout(this);layout->setContentsMargins(32,26,20,0);layout->setSpacing(14);
    auto header=new Ui::ResponsiveRow(1000);layout->addWidget(header);
    auto titles=new QVBoxLayout;titles->setSpacing(4);header->box()->addLayout(titles,1);
    auto title=new QLabel(t("Instances"));title->setObjectName("pageTitle");titles->addWidget(title);
    m_summary=new QLabel;m_summary->setObjectName("pageSubtitle");m_summary->setWordWrap(true);titles->addWidget(m_summary);
    auto head=new QHBoxLayout;head->setSpacing(10);header->box()->addLayout(head);
    auto group=iconButton(t("New group"),"folder-plus","newGroup");head->addWidget(group,0,Qt::AlignBottom);connect(group,&QPushButton::clicked,this,[this]{if(newGroup)newGroup();});
    auto import=iconButton(t("Import"),"download","importInstance");head->addWidget(import,0,Qt::AlignBottom);connect(import,&QPushButton::clicked,this,[this]{if(importPack)importPack();});
    auto create=iconButton(t("New instance"),"plus","newInstance",true);head->addWidget(create,0,Qt::AlignBottom);connect(create,&QPushButton::clicked,this,[this]{if(this->create)this->create();});
    head->addSpacing(12);
    auto filterRow=new Ui::ResponsiveRow(740);layout->addWidget(filterRow);auto filters=new QHBoxLayout;filters->setSpacing(6);filterRow->box()->addLayout(filters);
    for(int i=0;i<4;++i){auto tab=new QPushButton;tab->setCheckable(true);tab->setChecked(i==0);tab->setProperty("tab",true);tab->setCursor(Qt::PointingHandCursor);m_tabs<<tab;filters->addWidget(tab);connect(tab,&QPushButton::clicked,this,[this,i]{m_tab=i;for(int n=0;n<m_tabs.size();++n)m_tabs[n]->setChecked(n==i);rebuild();});}
    filters->addStretch();
    m_search=new QLineEdit;m_search->setObjectName("instanceSearch");m_search->setPlaceholderText(t("Search instances…"));m_search->setClearButtonEnabled(true);m_search->addAction(Ui::icon("search",QColor(150,150,158)),QLineEdit::LeadingPosition);m_search->setMinimumWidth(180);filterRow->box()->addWidget(m_search,1);
    connect(m_search,&QLineEdit::textChanged,this,[this]{rebuild();});
    m_scroll=new QScrollArea;m_scroll->setWidgetResizable(true);m_scroll->setFrameShape(QFrame::NoFrame);m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);layout->addWidget(m_scroll,1);
    rebuild();
}
int InstanceGrid::columns()const{int width=m_scroll->viewport()->width()-12;return qMax(1,(width+spacing)/(240+spacing));}
void InstanceGrid::resizeEvent(QResizeEvent *event){QWidget::resizeEvent(event);if(columns()!=m_columns)rebuild();}
void InstanceGrid::setInstances(const QList<InstanceInfo> &instances,const QStringList &emptyGroups,const QString &selected){m_items=instances;m_emptyGroups=emptyGroups;m_selected=selected;rebuild();}
void InstanceGrid::rebuild(){
    m_columns=columns();const int cardWidth=qBound(220,(m_scroll->viewport()->width()-12-(m_columns-1)*spacing)/m_columns,360);
    auto kind=[](const InstanceInfo &i){return i.modpack?3:i.base.loader=="vanilla"?1:2;};
    int counts[4]={int(m_items.size()),0,0,0};for(const auto &i:m_items)++counts[kind(i)];
    const QStringList titles{t("All"),"Vanilla",t("With mods"),t("Modpacks")};for(int n=0;n<4;++n)m_tabs[n]->setText(titles[n]+"  "+QString::number(counts[n]));
    m_summary->setText(t("Each instance keeps its own worlds, mods and settings."));
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
    auto content=new QWidget;content->setObjectName("instanceCards");auto layout=new QVBoxLayout(content);layout->setContentsMargins(0,4,12,16);layout->setSpacing(12);
    for(const auto &name:order){
        auto items=groups.value(name);std::sort(items.begin(),items.end(),[](const InstanceInfo &a,const InstanceInfo &b){return a.base.name.compare(b.base.name,Qt::CaseInsensitive)<0;});
        if(headers){
            auto row=new QHBoxLayout;row->setSpacing(6);layout->addLayout(row);const bool collapsed=m_collapsed.contains(name);
            auto header=new QPushButton(Ui::icon(collapsed?"chevron-right":"chevron-down",QColor(170,170,178)),"  "+(name.isEmpty()?t("No group"):name)+"   "+QString::number(items.size()));header->setObjectName("groupHeader");header->setIconSize(QSize(18,18));header->setCursor(Qt::PointingHandCursor);row->addWidget(header);row->addStretch();
            connect(header,&QPushButton::clicked,this,[this,name]{if(m_collapsed.contains(name))m_collapsed.remove(name);else m_collapsed.insert(name);rebuild();});
            if(!name.isEmpty()){auto gear=toolButton("settings",t("Group options"),"groupOptions");gear->setIconSize(QSize(16,16));row->addWidget(gear);connect(gear,&QToolButton::clicked,this,[this,name]{if(groupMenu)groupMenu(name);});}
            if(collapsed)continue;
        }
        if(items.isEmpty()){layout->addWidget(muted(t("Empty group. Use “Change group” on an instance to move it here.")));continue;}
        auto grid=new QGridLayout;grid->setHorizontalSpacing(spacing);grid->setVerticalSpacing(spacing);layout->addLayout(grid);
        for(int n=0;n<items.size();++n){
            const auto info=items[n];const auto dir=info.base.dir;
            auto card=new Card;card->setObjectName("instanceCard");card->setProperty("dir",dir);card->setProperty("selected",dir==m_selected);card->setFixedWidth(cardWidth);card->setCursor(Qt::PointingHandCursor);
            card->setToolTip(info.base.name);card->clicked=[this,dir]{if(open)open(dir);};card->context=[this,dir]{if(menu)menu(dir);};
            auto l=new QVBoxLayout(card);l->setContentsMargins(1,1,1,12);l->setSpacing(6);
            l->addWidget(new Banner(InstanceIcons::background(dir),InstanceIcons::icon(info.icon,dir),qBound(100,cardWidth*9/20,150)));
            auto body=new QVBoxLayout;body->setContentsMargins(14,4,14,0);body->setSpacing(5);l->addLayout(body);
            auto title=new QLabel;title->setObjectName("cardTitle");title->setText(title->fontMetrics().elidedText(info.base.name,Qt::ElideRight,cardWidth-28));body->addWidget(title);
            auto sub=new QLabel(InstanceText::loader(info.base.loader)+" "+info.base.mcVersion+(info.modpack?"  ·  "+t("Modpack"):QString()));sub->setObjectName("muted");body->addWidget(sub);
            auto buttons=new QHBoxLayout;buttons->setSpacing(8);body->addLayout(buttons);
            auto play=new QPushButton(" "+playText(info));play->setObjectName("cardPlay");play->setProperty(info.running?"danger":"play",true);play->setEnabled(!info.busy);play->setCursor(Qt::PointingHandCursor);
            if(!playIcon(info).isEmpty()){play->setIcon(Ui::icon(playIcon(info),info.running?QColor(255,163,174):QColor(Qt::white)));play->setIconSize(QSize(16,16));}buttons->addWidget(play,1);
            connect(play,&QPushButton::clicked,this,[this,dir]{if(this->play)this->play(dir);});
            auto gear=toolButton("settings",t("Instance settings"),"cardSettings");buttons->addWidget(gear);
            connect(gear,&QToolButton::clicked,this,[this,dir]{if(settings)settings(dir);});
            auto when=new QLabel(info.running?t("Running"):info.busy?t("Installing…"):!info.base.ready?t("Not installed yet"):InstanceText::lastPlayed(info.base.lastPlayed));when->setObjectName("cardFooter");body->addWidget(when);
            grid->addWidget(card,n/m_columns,n%m_columns,Qt::AlignTop|Qt::AlignLeft);
        }
        grid->setColumnStretch(m_columns,1);
    }
    if(m_items.isEmpty()){
        auto empty=new QFrame;empty->setObjectName("card");auto el=new QVBoxLayout(empty);el->setContentsMargins(30,30,30,30);el->setSpacing(10);
        auto icon=new QLabel;icon->setPixmap(Ui::pixmap("layout-grid",40,Ui::accent()));el->addWidget(icon);
        auto title=new QLabel(t("A place for every world"));title->setObjectName("sectionTitle");el->addWidget(title);
        el->addWidget(muted(t("Create your first instance. You can have several of the same version with different mods; each keeps its own worlds and settings.")));
        auto create=iconButton(t("New instance"),"plus","emptyNewInstance",true);create->setMaximumWidth(260);el->addWidget(create);connect(create,&QPushButton::clicked,this,[this]{if(this->create)this->create();});
        layout->addWidget(empty);
    }else if(order.isEmpty())layout->addWidget(muted(t("No instances match this search.")));
    // The click that triggered this rebuild may come from a card inside the old content: delete it later, not now.
    layout->addStretch();if(auto old=m_scroll->takeWidget())old->deleteLater();m_scroll->setWidget(content);m_content=content;
    QTimer::singleShot(0,m_scroll,[bar=m_scroll->verticalScrollBar(),scroll]{bar->setValue(scroll);});
}
InstanceDetail::InstanceDetail(QWidget *parent):QWidget(parent){
    setObjectName("instanceDetail");auto outer=new QVBoxLayout(this);outer->setContentsMargins(0,0,0,0);
    // Scrolls instead of squeezing when the window is short; the artwork stays painted behind.
    auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->viewport()->setAutoFillBackground(false);outer->addWidget(scroll);
    auto content=new QWidget;content->setObjectName("instanceDetailContent");scroll->setWidget(content);
    auto l=new QVBoxLayout(content);l->setContentsMargins(32,18,32,24);l->setSpacing(14);
    auto backRow=new QHBoxLayout;backRow->setSpacing(6);l->addLayout(backRow);auto backButton=new QPushButton(Ui::icon("chevron-left",Ui::accent()),t("All instances"));backButton->setObjectName("backButton");backButton->setProperty("link",true);backButton->setCursor(Qt::PointingHandCursor);backRow->addWidget(backButton);backRow->addStretch();
    connect(backButton,&QPushButton::clicked,this,[this]{if(back)back();});
    auto tools=backRow; // quick tools sit at the top right, over the artwork
    auto tool=[this,tools](const QString &icon,const QString &tip,const char *name,std::function<void()> InstanceDetail::*action){auto b=toolButton(icon,tip,name);tools->addWidget(b);connect(b,&QToolButton::clicked,this,[this,action]{if(this->*action)(this->*action)();});};
    tool("share-2",t("Export instance"),"exportInstance",&InstanceDetail::exportZip);tool("file-text",t("Log"),"showLog",&InstanceDetail::log);
    auto folderButton=toolButton("folder",t("Open instance folder"),"openFolder");tools->addWidget(folderButton);connect(folderButton,&QToolButton::clicked,this,[this]{if(openFolder)openFolder({});});
    tool("image",Language::text("Cambiar fondo","Change background","Mudar fundo"),"changeBackground",&InstanceDetail::changeBackground);
    tool("settings",t("Instance settings"),"instanceSettings",&InstanceDetail::settings);
    l->addSpacing(28); // the artwork shows above the title, like the instance page of the Minecraft Launcher
    auto titleRow=new Ui::ResponsiveRow(740);l->addWidget(titleRow);auto header=new QHBoxLayout;header->setSpacing(18);titleRow->box()->addLayout(header,1);
    m_icon=new QToolButton;m_icon->setFixedSize(84,84);m_icon->setObjectName("detailIcon");m_icon->setIconSize(QSize(62,62));m_icon->setCursor(Qt::PointingHandCursor);m_icon->setToolTip(t("Change icon"));m_icon->setAccessibleName(t("Change icon"));
    m_icon->setPopupMode(QToolButton::InstantPopup);m_icon->setMenu(InstanceIcons::menu(m_icon,[this](const QString &key,const QImage &custom){if(changeIcon)changeIcon(key,custom);}));header->addWidget(m_icon,0,Qt::AlignVCenter);
    auto info=new QVBoxLayout;info->setSpacing(6);header->addLayout(info,1);info->addStretch();
    m_name=new QLabel;m_name->setObjectName("detailTitle");m_name->setWordWrap(true);m_name->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);info->addWidget(m_name);
    m_state=new QLabel;m_state->setObjectName("muted");m_state->setWordWrap(true);info->addWidget(m_state);info->addStretch();
    m_play=new QPushButton;m_play->setObjectName("detailPlay");m_play->setMinimumSize(220,58);m_play->setIconSize(QSize(22,22));m_play->setCursor(Qt::PointingHandCursor);titleRow->box()->addWidget(m_play,0,Qt::AlignVCenter);connect(m_play,&QPushButton::clicked,this,[this]{if(play)play();});
    auto chipsRow=new Ui::ResponsiveRow(740);m_chips=chipsRow;chipsRow->box()->setSpacing(8);l->addWidget(m_chips);
    m_cardGrid=new QGridLayout;m_cardGrid->setSpacing(14);l->addLayout(m_cardGrid);
    struct Stat{QString icon,title,action,folder;};
    const QList<Stat> stats{{"puzzle",t("Mods"),t("MANAGE MODS"),"mods"},{"palette",t("Resource packs"),t("MANAGE RESOURCE PACKS"),"resourcepacks"},{"sparkles",t("Shader packs"),t("MANAGE SHADER PACKS"),"shaderpacks"},{"map",t("Worlds"),t("MANAGE WORLDS"),"saves"}};
    for(int n=0;n<stats.size();++n){
        auto card=new QFrame;card->setObjectName("statCard");card->setFixedHeight(150);m_cards<<card;auto cl=new QVBoxLayout(card);cl->setContentsMargins(18,16,18,12);cl->setSpacing(6);
        auto icon=new QLabel;icon->setObjectName("statIcon");m_cardIcons<<icon;icon->setFixedSize(40,40);icon->setAlignment(Qt::AlignCenter);icon->setPixmap(Ui::pixmap(stats[n].icon,22,Ui::accent()));cl->addWidget(icon);
        auto title=new QLabel(stats[n].title);title->setObjectName("statTitle");cl->addWidget(title);
        auto value=muted({});m_counts<<value;cl->addWidget(value);cl->addStretch();
        auto action=new QPushButton(stats[n].action);action->setProperty("link",true);action->setCursor(Qt::PointingHandCursor);cl->addWidget(action);
        const auto folder=stats[n].folder;connect(action,&QPushButton::clicked,this,[this,folder]{if(folder=="mods"){if(mods)mods();}else if(openFolder)openFolder(folder);});
    }
    auto actions=new QHBoxLayout;actions->setSpacing(8);l->addLayout(actions);
    auto action=[this,actions](const QString &text,const QString &icon,const char *name,std::function<void()> InstanceDetail::*callback){auto b=iconButton(text,icon,name);b->setToolTip(text);b->setAccessibleName(text);m_actions<<qMakePair(b,text);actions->addWidget(b);connect(b,&QPushButton::clicked,this,[this,callback]{if(this->*callback)(this->*callback)();});return b;};
    action(t("Find mods"),"compass","findMods",&InstanceDetail::findMods);action(t("Save mods as a pack"),"package","savePack",&InstanceDetail::savePack);action(t("Copy instance"),"copy","copyInstance",&InstanceDetail::copy);action(t("Change group"),"layers","changeGroup",&InstanceDetail::changeGroup);
    actions->addStretch();auto remove=action(t("Delete instance"),"trash-2","deleteInstance",&InstanceDetail::remove);remove->setProperty("danger",true);remove->setIcon(Ui::icon("trash-2",QColor(255,163,174)));
    // Fill the page with what the instance holds: its screenshots and its server list.
    auto extras=new Ui::ResponsiveRow(900);extras->box()->setSpacing(14);l->addWidget(extras);
    auto section=[extras](const QString &icon,const QString &title,QLabel **count){
        auto card=new QFrame;card->setObjectName("statCard");card->setMinimumHeight(250);auto cl=new QVBoxLayout(card);cl->setContentsMargins(18,16,18,16);cl->setSpacing(10);
        auto head=new QHBoxLayout;head->setSpacing(10);cl->addLayout(head);
        auto badge=new QLabel;badge->setObjectName("statIcon");badge->setFixedSize(32,32);badge->setAlignment(Qt::AlignCenter);badge->setPixmap(Ui::pixmap(icon,18,Ui::accent()));head->addWidget(badge);
        auto label=new QLabel(title);label->setObjectName("statTitle");head->addWidget(label);
        if(count){*count=muted({});head->addWidget(*count);}
        head->addStretch();extras->box()->addWidget(card,1);return qMakePair(cl,head);
    };
    auto gallery=section("image",t("Screenshots"),&m_galleryCount);
    auto openShots=new QPushButton(t("Open folder"));openShots->setProperty("link",true);openShots->setCursor(Qt::PointingHandCursor);gallery.second->addWidget(openShots);
    connect(openShots,&QPushButton::clicked,this,[this]{QDir().mkpath(m_dir+"/screenshots");if(openFolder)openFolder("screenshots");});
    m_gallery=new QListWidget;m_gallery->setObjectName("instanceGallery");m_gallery->setViewMode(QListView::IconMode);m_gallery->setFlow(QListView::LeftToRight);m_gallery->setWrapping(false);
    m_gallery->setIconSize(QSize(192,108));m_gallery->setSpacing(6);m_gallery->setMovement(QListView::Static);m_gallery->setFixedHeight(150);m_gallery->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_gallery->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);m_gallery->setFrameShape(QFrame::NoFrame);m_gallery->setCursor(Qt::PointingHandCursor);gallery.first->addWidget(m_gallery);
    connect(m_gallery,&QListWidget::itemActivated,this,[](QListWidgetItem *item){QDesktopServices::openUrl(QUrl::fromLocalFile(item->data(Qt::UserRole).toString()));});
    connect(m_gallery,&QListWidget::itemClicked,this,[](QListWidgetItem *item){QDesktopServices::openUrl(QUrl::fromLocalFile(item->data(Qt::UserRole).toString()));});
    m_galleryEmpty=muted(t("No screenshots yet. Press F2 in the game and they will appear here."));gallery.first->addWidget(m_galleryEmpty);gallery.first->addStretch();
    auto servers=section("globe",t("Servers"),nullptr);
    m_servers=new QListWidget;m_servers->setObjectName("instanceServers");m_servers->setIconSize(QSize(32,32));m_servers->setFixedHeight(150);m_servers->setFrameShape(QFrame::NoFrame);servers.first->addWidget(m_servers);
    m_serversEmpty=muted(t("No servers yet. Add one here or from the game's Multiplayer menu."));servers.first->addWidget(m_serversEmpty);
    auto serverRow=new QHBoxLayout;serverRow->setSpacing(8);servers.first->addLayout(serverRow);
    m_addServer=iconButton(t("Add server"),"plus","addServer");m_copyServer=iconButton(t("Copy address"),"copy","copyServer");m_removeServer=iconButton(t("Remove"),"trash-2","removeServer");
    serverRow->addWidget(m_addServer);serverRow->addWidget(m_copyServer);serverRow->addStretch();serverRow->addWidget(m_removeServer);
    connect(m_servers,&QListWidget::currentRowChanged,this,[this](int row){m_copyServer->setEnabled(row>=0);m_removeServer->setEnabled(row>=0&&!m_running);});
    connect(m_copyServer,&QPushButton::clicked,this,[this]{if(auto item=m_servers->currentItem())QGuiApplication::clipboard()->setText(item->data(Qt::UserRole).toString());});
    connect(m_addServer,&QPushButton::clicked,this,[this]{
        QDialog d(this);d.setObjectName("addServerDialog");d.setWindowTitle(t("Add server"));auto form=new QFormLayout(&d);form->setContentsMargins(20,18,20,16);
        auto name=new QLineEdit;name->setPlaceholderText(t("Minecraft Server"));auto address=new QLineEdit;address->setObjectName("serverAddress");address->setPlaceholderText("play.example.org");
        form->addRow(t("Name"),name);form->addRow(t("Address"),address);auto buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel);form->addRow(buttons);
        buttons->button(QDialogButtonBox::Save)->setEnabled(false);connect(address,&QLineEdit::textChanged,&d,[buttons](const QString &v){buttons->button(QDialogButtonBox::Save)->setEnabled(!v.trimmed().isEmpty()&&!v.contains(' '));});
        connect(buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
        if(Ui::openWindow(d)!=QDialog::Accepted)return;
        try{ServerList::add(m_dir+"/servers.dat",name->text(),address->text());}catch(const std::exception &e){QMessageBox::warning(this,"EBALIA",Language::message(QString::fromUtf8(e.what())));}
        showServers();
    });
    connect(m_removeServer,&QPushButton::clicked,this,[this]{
        const int row=m_servers->currentRow();if(row<0)return;
        if(QMessageBox::question(this,"EBALIA",t("Remove this server from the list?"))!=QMessageBox::Yes)return;
        try{ServerList::remove(m_dir+"/servers.dat",row);}catch(const std::exception &e){QMessageBox::warning(this,"EBALIA",Language::message(QString::fromUtf8(e.what())));}
        showServers();
    });
    l->addStretch();arrange();
}
void InstanceDetail::showScreenshots(){
    auto files=QDir(m_dir+"/screenshots").entryInfoList({"*.png","*.jpg","*.jpeg"},QDir::Files,QDir::Time);
    // Instance refreshes are frequent: only reload when the folder changed.
    const auto signature=m_dir+"|"+QString::number(files.size())+"|"+(files.isEmpty()?QString():files.first().fileName()+QString::number(files.first().lastModified().toMSecsSinceEpoch()));
    if(signature==m_gallerySignature)return;
    m_gallerySignature=signature;m_gallery->clear();
    m_galleryCount->setText(files.isEmpty()?QString():QString::number(files.size()));
    m_gallery->setVisible(!files.isEmpty());m_galleryEmpty->setVisible(files.isEmpty());
    const auto dir=m_dir;
    for(const auto &file:files.mid(0,30)){
        auto item=new QListWidgetItem(m_gallery);item->setData(Qt::UserRole,file.absoluteFilePath());item->setToolTip(file.fileName()+"\n"+QLocale().toString(file.lastModified(),QLocale::ShortFormat));item->setSizeHint(QSize(198,114));
        Ui::loadArt(file.absoluteFilePath(),{384,216},this,[this,dir,path=file.absoluteFilePath()](const QPixmap &p){
            if(m_dir!=dir)return;for(int n=0;n<m_gallery->count();++n)if(m_gallery->item(n)->data(Qt::UserRole).toString()==path)m_gallery->item(n)->setIcon(QIcon(Ui::cover(p,{192,108},8)));});
    }
}
void InstanceDetail::showServers(){
    const auto selected=m_servers->currentRow();m_servers->clear();
    const auto list=ServerList::read(m_dir+"/servers.dat");
    for(const auto &server:list){
        auto item=new QListWidgetItem(server.icon.isNull()?InstanceIcons::icon("server"):QIcon(QPixmap::fromImage(server.icon)),server.name+"\n"+server.address,m_servers);
        item->setData(Qt::UserRole,server.address);item->setSizeHint(QSize(0,44));
    }
    m_servers->setVisible(!list.isEmpty());m_serversEmpty->setVisible(list.isEmpty());
    if(selected>=0&&selected<m_servers->count())m_servers->setCurrentRow(selected);
    m_addServer->setEnabled(!m_running);m_addServer->setToolTip(m_running?t("Close the game to edit its server list."):QString());
    m_copyServer->setEnabled(m_servers->currentRow()>=0);m_removeServer->setEnabled(m_servers->currentRow()>=0&&!m_running);
}
// Cards reflow to two or one column; narrow windows keep action labels in tooltips.
void InstanceDetail::arrange(){
    const bool compact=width()<1000;const int columns=width()<700?1:compact?2:4;if(columns==m_cardColumns&&m_cardGrid->count())return;m_compact=compact;m_cardColumns=columns;
    for(auto card:m_cards)m_cardGrid->removeWidget(card);
    for(int n=0;n<m_cards.size();++n){m_cardGrid->addWidget(m_cards[n],n/columns,n%columns);m_cards[n]->setFixedHeight(compact?112:150);m_cardIcons[n]->setVisible(!compact);}
    for(const auto &action:m_actions)action.first->setText(compact?QString():" "+action.second);
}
void InstanceDetail::resizeEvent(QResizeEvent *event){QWidget::resizeEvent(event);arrange();}
void InstanceDetail::paintEvent(QPaintEvent *){
    if(m_backdrop.isNull())return;
    QPainter p(this);const int height=qMin(this->height(),340);const QSize area(width(),height);
    if(m_scaled.size()!=area)m_scaled=Ui::cover(m_backdrop,area);p.drawPixmap(0,0,m_scaled);
    QLinearGradient fade(0,0,0,height);fade.setColorAt(0,QColor(20,20,22,120));fade.setColorAt(0.55,QColor(20,20,22,200));fade.setColorAt(1,QColor(20,20,22,255));p.fillRect(QRect(QPoint(0,0),area),fade);
}
void InstanceDetail::showInstance(const InstanceInfo &i){
    if(const auto art=InstanceIcons::background(i.base.dir);m_dir!=i.base.dir||m_art!=art+QString::number(QFileInfo(art).lastModified().toMSecsSinceEpoch())){const auto dir=i.base.dir;m_dir=dir;m_art=art+QString::number(QFileInfo(art).lastModified().toMSecsSinceEpoch());m_backdrop={};m_scaled={};Ui::loadArt(art,{1920,1080},this,[this,dir](const QPixmap &p){if(m_dir==dir){m_backdrop=p;m_scaled={};update();}});}
    m_dir=i.base.dir;m_icon->setIcon(InstanceIcons::icon(i.icon,i.base.dir));m_name->setText(i.base.name);
    auto chips=static_cast<QBoxLayout*>(m_chips->layout());while(auto item=chips->takeAt(0)){if(item->widget())item->widget()->deleteLater();delete item;}
    auto chip=[chips](const QString &text,bool accent=false){auto c=new QLabel(text);c->setWordWrap(true);c->setObjectName("chip");c->setProperty("accent",accent);chips->addWidget(c);};
    chip(InstanceText::loader(i.base.loader)+(i.base.loader=="vanilla"||i.loaderVersion.isEmpty()?QString():" "+i.loaderVersion),true);chip("Minecraft "+i.base.mcVersion);chip(QString::number(i.base.xmx)+" MB");
    if(!i.group.isEmpty())chip(i.group);if(i.modpack)chip(t("Modpack"));chips->addStretch();
    m_state->setText((i.running?t("Running"):i.busy?t("Installing…"):i.base.ready?t("Ready to play"):t("Not installed yet"))+"  ·  "+InstanceText::lastPlayed(i.base.lastPlayed));
    m_play->setText("  "+playText(i).toUpper());m_play->setIcon(playIcon(i).isEmpty()?QIcon():Ui::icon(playIcon(i),Qt::white));
    m_play->setProperty("play",!i.running);m_play->setProperty("danger",i.running);m_play->setEnabled(!i.busy);m_play->style()->unpolish(m_play);m_play->style()->polish(m_play);
    const auto dir=i.base.dir;const int enabled=count(dir+"/mods",{"*.jar"},QDir::Files),disabled=count(dir+"/mods",{"*.jar.disabled"},QDir::Files);
    m_counts[0]->setText(t("%1 mods enabled").arg(enabled)+(disabled?" · "+t("%1 disabled").arg(disabled):QString()));
    const int packs=count(dir+"/resourcepacks",{},QDir::Files|QDir::Dirs),shaders=count(dir+"/shaderpacks",{},QDir::Files|QDir::Dirs),worlds=count(dir+"/saves",{},QDir::Dirs);
    m_counts[1]->setText(packs?t("%1 resource packs").arg(packs):t("No resource packs yet"));m_counts[2]->setText(shaders?t("%1 shader packs").arg(shaders):t("No shader packs yet"));m_counts[3]->setText(worlds?t("%1 worlds").arg(worlds):t("No worlds yet"));
    m_running=i.running;showScreenshots();showServers();
}
