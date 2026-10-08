#include <QStatusBar>
#include "MainWindow.hpp"
#include "GuidedTour.hpp"
#include "TourMascot.hpp"
#include "CommunityPage.hpp"
#include "BedrockPage.hpp"
#include "PatreonAuth.hpp"
#include "ModRepository.hpp"
#include "Language.hpp"
#include "LostInstaller.hpp"
#include "SkinsPage.hpp"
#include "VersionManager.hpp"
#include "AccountManager.hpp"
#include "MsAuth.hpp"
#include "JavaRunner.hpp"
#include "LostNative.hpp"
#include "SoftwareGl.hpp"
#include "CrashReport.hpp"
#include "GpuInfo.hpp"
#include "JavaRuntime.hpp"
#include "SetupDialog.hpp"
#include "CreateInstanceDialog.hpp"
#include "PackService.hpp"
#include "InstanceViews.hpp"
#include "InstanceIcons.hpp"
#include "Icons.hpp"
#include "Ui.hpp"
#include "HomeBanner.hpp"
#include "CreatorDialog.hpp"
#include "PatreonNewsPage.hpp"
#include <QtWidgets>
#include <QtNetwork>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QCryptographicHash>
#include <stdexcept>
#include <algorithm>
using Language::text;
namespace {
QString k(const char *source){return Language::key(QString::fromUtf8(source));}
const QColor dim(180,180,188);
class PlayBar:public QFrame {
protected:
    void resizeEvent(QResizeEvent *event) override {
        QFrame::resizeEvent(event);auto grid=qobject_cast<QGridLayout*>(layout());if(!grid)return;
        auto play=findChild<QPushButton*>("homePlay");if(!play)return;
        const bool compact=width()<740;if(property("compact").isValid()&&property("compact").toBool()==compact)return;
        setProperty("compact",compact);grid->removeWidget(play);
        if(compact)grid->addWidget(play,1,0,1,3,Qt::AlignHCenter);else grid->addWidget(play,0,1,Qt::AlignCenter);
        grid->setVerticalSpacing(14);grid->invalidate();updateGeometry();
    }
};
class ArchivePanels:public QWidget {
public:
    ArchivePanels(QWidget *list,QWidget *detail):m_list(list),m_detail(detail){m_layout=new QBoxLayout(QBoxLayout::LeftToRight,this);m_layout->setContentsMargins(0,0,0,0);m_layout->setSpacing(18);m_layout->addWidget(list);m_layout->addWidget(detail,1);}
protected:
    void resizeEvent(QResizeEvent *event)override{
        QWidget::resizeEvent(event);bool compact=width()<760;if(property("compact").isValid()&&property("compact").toBool()==compact)return;
        setProperty("compact",compact);m_layout->setDirection(compact?QBoxLayout::TopToBottom:QBoxLayout::LeftToRight);
        m_list->setMaximumWidth(compact?QWIDGETSIZE_MAX:336);m_list->setMinimumHeight(compact?170:100);m_list->setMaximumHeight(compact?210:QWIDGETSIZE_MAX);
        m_detail->setMinimumHeight(compact?440:340);m_layout->invalidate();updateGeometry();
    }
private:
    QWidget *m_list,*m_detail;QBoxLayout *m_layout;
};
// Covers fill their box by default; news use an aspect-ratio box and show the whole image.
class Cover:public QWidget {
public:
    explicit Cover(int radius,bool allCorners=false,QWidget *parent=nullptr):QWidget(parent),m_radius(radius),m_all(allCorners){setAttribute(Qt::WA_TransparentForMouseEvents);}
    void setPixmap(const QPixmap &pixmap){m_resource.clear();assign(pixmap);}
    void setArtwork(const QString &path){if(m_resource==path)return;m_resource=path;assign({});Ui::loadArt(path,{1920,1080},this,[this,path](const QPixmap &p){if(m_resource==path)assign(p);});}
    void setShaded(bool shaded){m_shaded=shaded;update();}
    void setAspectRatio(qreal ratio){m_ratio=ratio;auto policy=QSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);policy.setHeightForWidth(true);setSizePolicy(policy);updateGeometry();}
    QSize sizeHint() const override {return m_ratio>0?QSize(300,heightForWidth(300)):QWidget::sizeHint();}
    QSize minimumSizeHint() const override {return m_ratio>0?QSize(0,0):QWidget::minimumSizeHint();}
    int heightForWidth(int width) const override {return m_ratio>0?qRound(width/m_ratio):QWidget::heightForWidth(width);}
protected:
    void paintEvent(QPaintEvent *) override {
        const auto target=size()*devicePixelRatioF();
        if(m_targetSize!=target&&!m_pixmap.isNull()){m_scaled=m_ratio>0?m_pixmap.scaled(target,Qt::KeepAspectRatio,Qt::SmoothTransformation):Ui::cover(m_pixmap,target);m_scaled.setDevicePixelRatio(devicePixelRatioF());m_targetSize=target;}
        QPainter p(this);p.setRenderHint(QPainter::Antialiasing);QPainterPath clip;clip.addRoundedRect(m_all?QRectF(rect()):QRectF(rect()).adjusted(0,0,0,m_radius),m_radius,m_radius);p.setClipPath(clip);
        p.fillRect(rect(),QColor(24,24,27));
        if(!m_scaled.isNull()){const auto logical=m_scaled.deviceIndependentSize();p.drawPixmap(QPointF((width()-logical.width())/2,(height()-logical.height())/2),m_scaled);}
        if(m_shaded){QLinearGradient shade(0,height()*0.25,0,height());shade.setColorAt(0,Qt::transparent);shade.setColorAt(1,QColor(18,18,24,240));p.fillRect(rect(),shade);}
    }
private:
    void assign(const QPixmap &pixmap){m_pixmap=pixmap;m_scaled={};m_targetSize={};update();}
    QPixmap m_pixmap,m_scaled;QSize m_targetSize;QString m_resource;int m_radius;bool m_all,m_shaded=false;qreal m_ratio=0;
};
// QPushButton's default size hint ignores its child layout.
class ContentButton:public QPushButton {
public:
    using QPushButton::QPushButton;
    QSize sizeHint() const override {return layout()?layout()->sizeHint():QPushButton::sizeHint();}
    QSize minimumSizeHint() const override {return layout()?layout()->minimumSize():QPushButton::minimumSizeHint();}
    bool hasHeightForWidth() const override {return layout()&&layout()->hasHeightForWidth();}
    int heightForWidth(int width) const override {return layout()?layout()->totalHeightForWidth(width):QPushButton::heightForWidth(width);}
};
// One-line label that shrinks with "…" instead of being cut (picker, account and player names).
class Elided:public QLabel {
public:
    using QLabel::QLabel;
    QSize minimumSizeHint() const override {return {0,QLabel::minimumSizeHint().height()};}
protected:
    void paintEvent(QPaintEvent *) override {QPainter p(this);const auto r=contentsRect();style()->drawItemText(&p,r,int(alignment()),palette(),isEnabled(),fontMetrics().elidedText(text(),Qt::ElideRight,r.width()),foregroundRole());}
};
QLabel *elided(QLayout *layout,const char *name,Qt::Alignment alignment=Qt::AlignLeft|Qt::AlignVCenter){auto l=new Elided;l->setObjectName(name);l->setAlignment(alignment);l->setTextFormat(Qt::PlainText);layout->addWidget(l);return l;}
QPushButton *button(const QString &label,QLayout *layout,std::function<void()> action,QObject *owner,bool primary=false,const QString &icon={}) {
    auto caption=label;caption.replace("&","&&"); // Labels are literal text, not Qt keyboard mnemonics.
    auto b=new QPushButton(icon.isEmpty()||caption.isEmpty()?caption:" "+caption);b->setCursor(Qt::PointingHandCursor);if(primary)b->setProperty("play",true);
    if(!icon.isEmpty()){b->setIcon(Ui::icon(icon,primary?QColor(Qt::white):QColor(225,225,230)));b->setIconSize(QSize(18,18));}
    if(layout)layout->addWidget(b);QObject::connect(b,&QPushButton::clicked,owner,std::move(action));return b;
}
QLabel *label(const QString &value,QLayout *layout,const char *name="",bool wrap=true) {
    auto l=new QLabel(value);l->setObjectName(name);l->setWordWrap(wrap);l->setTextFormat(Qt::PlainText);if(layout)layout->addWidget(l);return l;
}
// Every section starts the same way: big title, a short line below and room for actions on the right.
struct Section {QWidget *widget;QVBoxLayout *layout;QHBoxLayout *actions;};
Section section(const QString &title,const QString &subtitle) {
    auto page=new QWidget;auto layout=new QVBoxLayout(page);layout->setContentsMargins(32,26,32,22);layout->setSpacing(16);
    auto row=new Ui::ResponsiveRow(760);layout->addWidget(row);auto texts=new QVBoxLayout;texts->setSpacing(4);row->box()->addLayout(texts,1);
    label(title,texts,"pageTitle");if(!subtitle.isEmpty())label(subtitle,texts,"pageSubtitle");
    auto actions=new QHBoxLayout;actions->setSpacing(10);row->box()->addLayout(actions);return {page,layout,actions};
}
// A rounded card with an icon, a title and an optional description, used by Settings.
QVBoxLayout *settingsCard(QVBoxLayout *parent,const QString &icon,const QString &title,const QString &description) {
    auto frame=new QFrame;frame->setObjectName("settingsCard");auto l=new QVBoxLayout(frame);l->setContentsMargins(22,18,22,20);l->setSpacing(12);
    auto head=new QHBoxLayout;head->setSpacing(10);l->addLayout(head);auto i=new QLabel;i->setPixmap(Ui::pixmap(icon,22,Ui::accent()));head->addWidget(i);label(title,head,"sectionTitle",false);head->addStretch();
    if(!description.isEmpty())label(description,l,"muted");parent->addWidget(frame);return l;
}
QString exception() {try{throw;}catch(const std::exception &e){return QString::fromUtf8(e.what());}catch(...){return "Unknown error";}}
QString filename(QListWidget *list) {return list->currentItem()?list->currentItem()->data(Qt::UserRole).toString():QString();}
QAction *menuAction(QMenu &menu,const QString &icon,const QString &label,QObject *context,std::function<void()> action) {
    auto a=menu.addAction(icon.isEmpty()?QIcon():Ui::icon(icon),label);QObject::connect(a,&QAction::triggered,context,std::move(action));return a;
}
bool minecraftLink(const QUrl &url){return url.scheme()=="https"&&(url.host()=="minecraft.net"||url.host().endsWith(".minecraft.net"));}
class ArchiveDelegate:public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &,const QModelIndex &)const override{return {210,94};}
    void paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index)const override{
        const auto data=index.data(Qt::UserRole+1).toJsonObject();const auto r=option.rect.adjusted(3,3,-3,-3);p->save();p->setRenderHint(QPainter::Antialiasing);
        QPainterPath clip;clip.addRoundedRect(r,10,10);p->setClipPath(clip);
        const auto path=data["art"].toString();auto thumbnail=Ui::cachedArt(path,{768,432});
        if(thumbnail.isNull()&&!m_pending.contains(path)){
            m_pending.insert(path);auto self=const_cast<ArchiveDelegate*>(this);QPointer<QWidget> view=const_cast<QWidget*>(option.widget);
            Ui::loadArt(path,{768,432},self,[self,path,view](const QPixmap &){self->m_pending.remove(path);if(view)view->update();if(auto list=qobject_cast<QAbstractItemView*>(view.data()))list->viewport()->update();});
        }
        p->fillRect(r,QColor(27,31,36));if(!thumbnail.isNull()){const qreal cropHeight=thumbnail.width()*qreal(r.height())/r.width();p->setRenderHint(QPainter::SmoothPixmapTransform);p->drawPixmap(r,thumbnail,QRectF(0,0,thumbnail.width(),qMin(cropHeight,qreal(thumbnail.height()))));}
        QLinearGradient shade(r.topLeft(),r.topRight());shade.setColorAt(0,QColor(10,10,16,220));shade.setColorAt(1,QColor(10,10,16,65));p->fillRect(r,shade);
        const auto category=data["category"].toString();p->drawPixmap(r.left()+14,r.center().y()-16,InstanceIcons::icon(category=="horror"?"horror":category=="release"?"crafting":"grass").pixmap(32,32));
        auto font=option.font;font.setPixelSize(14);font.setBold(true);p->setFont(font);p->setPen(Qt::white);const int width=r.width()-72;
        p->drawText(QRect(r.left()+58,r.top()+20,width,24),Qt::AlignVCenter,QFontMetrics(font).elidedText(data["name"].toString(),Qt::ElideRight,width));
        font.setPixelSize(11);font.setBold(false);p->setFont(font);p->setPen(QColor(214,220,224));p->drawText(QRect(r.left()+58,r.top()+46,width,20),Qt::AlignVCenter,data["label"].toString());
        p->setClipping(false);p->setBrush(Qt::NoBrush);p->setPen(QPen(option.state&QStyle::State_Selected?Ui::accent():QColor(65,65,74),option.state&QStyle::State_Selected?2:1));p->drawRoundedRect(r,10,10);p->restore();
    }
private:
    mutable QSet<QString> m_pending;
};
}
namespace {
class TaskBarFilter:public QObject {
public:
    explicit TaskBarFilter(QStatusBar *bar):QObject(bar),m_bar(bar){}
    bool eventFilter(QObject *object,QEvent *event) override {
        if(object==m_bar&&event->type()==QEvent::ChildAdded)if(auto w=qobject_cast<QWidget*>(static_cast<QChildEvent*>(event)->child()))w->installEventFilter(this);
        if(event->type()==QEvent::ChildAdded||event->type()==QEvent::ChildRemoved||event->type()==QEvent::ShowToParent||event->type()==QEvent::HideToParent||event->type()==QEvent::Show||event->type()==QEvent::Hide)
            QMetaObject::invokeMethod(this,[this]{update();},Qt::QueuedConnection);
        return false;
    }
private:
    void update(){bool any=false;for(auto w:m_bar->findChildren<QWidget*>(QString(),Qt::FindDirectChildrenOnly))if(!w->isWindow()&&w->objectName()!="qt_statusbar_sizegrip"&&w->isVisibleTo(m_bar)&&w->sizeHint().height()>0&&!qobject_cast<QLabel*>(w))any=true;m_bar->setVisible(any);}
    QStatusBar *m_bar;
};
}
MainWindow::MainWindow(QWidget *parent):QMainWindow(parent) {
    // The window's own status bar only holds background tasks (installs, updates): hidden while it has none,
    // so no empty strip shows under the page.
    {auto bar=statusBar();bar->setSizeGripEnabled(false);bar->hide();
     auto filter=new TaskBarFilter(bar);bar->installEventFilter(filter);}
    m_root=qEnvironmentVariable("EBALIA_DATA_DIR");if(m_root.isEmpty())m_root=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(m_root);Language::current=QSettings().value("ui/language",QLocale::system().name().left(2)).toString();
    if(qEnvironmentVariableIsSet("EBALIA_LANGUAGE"))Language::current=qEnvironmentVariable("EBALIA_LANGUAGE");
    if(!Language::available().contains(Language::current))Language::current="en";
    m_mc=new McInstanceManager(m_root,this);m_mc->nameFolders();
    // Instance icons with Minecraft's own textures once a game version was downloaded.
    QTimer::singleShot(0,this,[this]{InstanceIcons::useGameTextures(m_mc->mcDir(),this,[this]{refreshInstances();});});
    // Instances of other launchers pasted into the instances folder become EBALIA instances, also while the launcher is open.
    {auto watcher=new QFileSystemWatcher({m_mc->instancesRoot()},this);m_pasteTimer=new QTimer(this);m_pasteTimer->setSingleShot(true);m_pasteTimer->setInterval(3000);
     connect(watcher,&QFileSystemWatcher::directoryChanged,m_pasteTimer,qOverload<>(&QTimer::start));connect(m_pasteTimer,&QTimer::timeout,this,[this]{adoptPastedInstances();});m_pasteTimer->start(1500);}m_versions=new VersionManager(this);m_versions->loadVersions();
    m_patreon=new PatreonAuth(this);
    m_accounts=new AccountManager(m_mc->mcDir(),this);m_java=new JavaRunner(this);
    qApp->setStyleSheet(Ui::styleSheet()); // application-wide, so separate windows (new instance, settings, logs) share the look
    build();setWindowTitle("EBALIA Launcher");setMinimumSize(640,480);Ui::fitToScreen(this,{1280,800});
    connect(m_mc,&McInstanceManager::manifestReady,this,[this](const QList<McVersion> &v){m_manifest=v;m_catalog->setText(text("Catálogo actualizado · ","Catalog updated · ","Catálogo atualizado · ")+QString::number(v.size())+text(" versiones"," versions"," versões"));});
    connect(m_mc,&McInstanceManager::manifestFailed,this,[this](const QString &e){m_catalog->setText(Language::message(e));});
    connect(m_mc,&McInstanceManager::installProgress,this,[this](const QString &,int p,const QString &stage){m_progress->show();m_progress->setRange(0,100);m_progress->setValue(p);m_status->setText(Language::message(stage));});
    connect(m_mc,&McInstanceManager::installDone,this,[this](const QString &dir,bool ok,const QString &e){m_installing.remove(dir);if(ok)InstanceIcons::useGameTextures(m_mc->mcDir(),this,[this]{refreshInstances();});const bool start=m_launchAfterInstall.remove(dir)&&ok;if(start)QTimer::singleShot(0,this,[this,dir]{for(const auto &i:m_mc->instances())if(i.dir==dir&&i.ready)launchInstance(i);});m_progress->setValue(ok?100:0);m_progress->setVisible(!m_installing.isEmpty());m_status->setText(ok?text("Instancia lista para jugar","Instance ready to play","Instância pronta para jogar"):Language::message(e));refreshInstances();if(!ok)error(e);});
    connect(m_mc,&McInstanceManager::gameStarted,this,[this](const QString &){refreshInstances();m_status->setText(text("Minecraft en ejecución","Minecraft is running","Minecraft em execução"));});
    connect(m_mc,&McInstanceManager::gameEnded,this,[this](const QString &dir,int code){refreshInstances();m_status->setText(text("Minecraft terminó · código ","Minecraft exited · code ","Minecraft terminou · código ")+QString::number(code));if(code)showCrash(dir);});
    connect(m_mc,&McInstanceManager::launchFailed,this,[this](const QString &,const QString &e){error(e);refreshInstances();});
    connect(m_accounts,&AccountManager::accountsChanged,this,&MainWindow::refreshAccounts);
    connect(m_java,&JavaRunner::processError,this,&MainWindow::error);
    connect(m_java,&JavaRunner::processStarted,this,[this]{m_status->setText(text("Versión perdida en ejecución","Lost version is running","Versão perdida em execução"));lostSelection();});
    connect(m_java,&JavaRunner::processFinished,this,[this]{lostSelection();m_status->setText(text("Todo listo","Ready","Tudo pronto"));});

    if(!qEnvironmentVariableIsSet("EBALIA_NO_NETWORK")){m_mc->fetchManifest();refreshNews();m_patreon->refreshNews();auto timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{m_mc->fetchManifest();refreshNews();});timer->start(30*60*1000);}
    if(!QSettings().value("ui/tutorialSeen",false).toBool())QTimer::singleShot(400,this,[this]{startTour();});
    if(!qEnvironmentVariableIsSet("EBALIA_NO_NETWORK")&&!QSettings().value("ui/patreonInviteSeen",false).toBool()&&!QSettings().value("ui/hidePatreonInvite",false).toBool()){
        auto invite=new QTimer(this);invite->setInterval(15000);
        connect(invite,&QTimer::timeout,this,[this,invite]{
            if(m_patreon->verified()||QSettings().value("ui/hidePatreonInvite",false).toBool()){invite->stop();return;}
            if(!isActiveWindow()||m_pages->currentIndex()!=Home||Ui::openWindows()||m_jobs||findChild<QWidget*>("interactiveTour"))return;
            invite->stop();QSettings().setValue("ui/patreonInviteSeen",true);openCreatorDialog(true);
        });invite->start();
    }
}
void MainWindow::build() {
    for(auto page:findChildren<QWidget*>("bedrockLabPage"))
        if(page->property("bedrockInstalling").toBool())return;

    const int current=m_pages?m_pages->currentIndex():int(Home);
    auto old=takeCentralWidget();if(old)old->deleteLater();
    auto root=new QWidget;root->setObjectName("content");setCentralWidget(root);
    auto outer=new QHBoxLayout(root);outer->setContentsMargins(0,0,0,0);outer->setSpacing(0);outer->addWidget(buildSidebar());
    auto right=new QVBoxLayout;right->setContentsMargins(0,0,0,0);right->setSpacing(0);outer->addLayout(right,1);
    m_pages=new QStackedWidget;right->addWidget(m_pages,1);
    // Same order as Page and the sidebar.
    for(auto page:{buildHome(),buildInstances(),buildExplore(),buildLost(),buildPacks(),buildSkins(),buildNews(),buildCommunity(),buildGuide(),static_cast<QWidget*>(new BedrockPage(m_root)),buildSettings()}){
        auto scroll=new QScrollArea;scroll->setObjectName("pageScroll");scroll->setFrameShape(QFrame::NoFrame);scroll->setWidgetResizable(true);scroll->setWidget(page);
        if(page->layout())page->layout()->setSizeConstraint(QLayout::SetMinimumSize);m_pages->addWidget(scroll);
    }
    auto status=new QFrame;status->setObjectName("statusBar");auto sl=new QHBoxLayout(status);sl->setContentsMargins(20,7,20,7);sl->setSpacing(14);
    m_status=label(text("Todo listo","Ready","Tudo pronto"),sl,"statusText",false);m_status->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);sl->setStretch(0,1);
    m_progress=new QProgressBar;m_progress->setFixedWidth(200);m_progress->setValue(0);m_progress->setTextVisible(false);m_progress->hide();sl->addWidget(m_progress);right->addWidget(status);
    refreshAccounts();refreshInstances();refreshPacks();refreshLost();refreshNews(false);showPage(current);adaptSidebar();
}
QWidget *MainWindow::buildSidebar() {
    auto bar=new QFrame;bar->setObjectName("sidebar");bar->setFixedWidth(244);auto l=new QVBoxLayout(bar);l->setContentsMargins(14,16,14,12);l->setSpacing(10);
    auto brand=new QHBoxLayout;brand->setContentsMargins(6,0,0,4);brand->setSpacing(10);l->addLayout(brand);
    auto logo=new QLabel;logo->setPixmap(QPixmap(":/icon.png").scaled(30,30,Qt::KeepAspectRatio,Qt::SmoothTransformation));brand->addWidget(logo);label("EBALIA",brand,"brand",false);brand->addStretch();
    m_accountButton=new QPushButton;m_accountButton->setObjectName("accountButton");m_accountButton->setCursor(Qt::PointingHandCursor);m_accountButton->setFixedHeight(62);
    auto al=new QHBoxLayout(m_accountButton);al->setContentsMargins(10,8,12,8);al->setSpacing(10);
    m_accountAvatar=new QLabel;m_accountAvatar->setFixedSize(40,40);al->addWidget(m_accountAvatar);
    auto names=new QVBoxLayout;names->setSpacing(1);al->addLayout(names,1);m_accountName=elided(names,"accountName");m_accountType=elided(names,"accountType");
    auto chevron=new QLabel;chevron->setObjectName("accountChevron");chevron->setPixmap(Ui::pixmap("chevron-down",16,dim));al->addWidget(chevron);
    for(QWidget *w:{static_cast<QWidget*>(m_accountAvatar),static_cast<QWidget*>(m_accountName),static_cast<QWidget*>(m_accountType),static_cast<QWidget*>(chevron)})w->setAttribute(Qt::WA_TransparentForMouseEvents);
    connect(m_accountButton,&QPushButton::clicked,this,[this]{accountMenu();});l->addWidget(m_accountButton);
    m_nav=new QListWidget;m_nav->setObjectName("navigation");m_nav->setIconSize(QSize(28,28));m_nav->setFocusPolicy(Qt::NoFocus);m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);m_nav->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);m_nav->setFrameShape(QFrame::NoFrame);
    const QList<QPair<QString,QString>> items{{"house",k("Home")},{"layout-grid",k("Instances")},{"compass",k("Discover mods")},{"ghost",k("Lost versions")},{"package",k("My packs")},{"shirt",k("Skins")},{"newspaper",k("News")},{"users",k("Community")},{"book-open",k("Guide & tutorial")},{"package","Bedrock · Beta"}};
    for(const auto &item:items){
        auto entry=new QListWidgetItem(Ui::navigationIcon(item.first),"  "+item.second,m_nav);entry->setSizeHint(QSize(0,42));entry->setToolTip(item.second);
    }
    l->addWidget(m_nav,1);
    connect(m_nav,&QListWidget::currentRowChanged,this,[this](int row){if(row>=0)showPage(row);});
    connect(m_nav,&QListWidget::itemClicked,this,[this](QListWidgetItem *item){if(m_nav->row(item)==Instances)m_library->setCurrentWidget(m_grid);}); // the sidebar entry always leads back to the library
    m_sidebarUpdate=new QPushButton(Ui::icon("download",Qt::white),{});m_sidebarUpdate->setObjectName("launcherUpdateAvailable");m_sidebarUpdate->setProperty("play",true);m_sidebarUpdate->setCursor(Qt::PointingHandCursor);m_sidebarUpdate->setIconSize(QSize(20,20));m_sidebarUpdate->setMinimumHeight(42);m_sidebarUpdate->hide();
    connect(m_sidebarUpdate,&QPushButton::clicked,this,[this]{emit installLauncherUpdateRequested();});l->addWidget(m_sidebarUpdate);
    m_navSettings=new QPushButton(Ui::navigationIcon("settings"),"  "+k("Settings"));m_navSettings->setObjectName("navSettings");m_navSettings->setCheckable(true);m_navSettings->setIconSize(QSize(28,28));m_navSettings->setCursor(Qt::PointingHandCursor);
    connect(m_navSettings,&QPushButton::clicked,this,[this]{showPage(Settings);});l->addWidget(m_navSettings);
    auto version=label("EBALIA Launcher "+QCoreApplication::applicationVersion(),l,"sidebarVersion",false);version->setContentsMargins(12,0,0,0);
    if(QCoreApplication::applicationVersion().isEmpty())version->setText("EBALIA Launcher 1.0.0");
    return bar;
}
QWidget *MainWindow::buildHome() {
    auto home=new QWidget;home->setObjectName("homePage");auto l=new QVBoxLayout(home);l->setContentsMargins(0,0,0,0);l->setSpacing(0);
    auto hero=new HomeBanner;
    l->addWidget(hero,1);
    auto bar=new PlayBar;bar->setObjectName("playBar");auto bl=new QGridLayout(bar);bl->setContentsMargins(28,14,28,16);bl->setHorizontalSpacing(16);
    m_instancePicker=new ContentButton;m_instancePicker->setObjectName("instancePicker");m_instancePicker->setCursor(Qt::PointingHandCursor);m_instancePicker->setFixedHeight(62);m_instancePicker->setMinimumWidth(190);m_instancePicker->setMaximumWidth(360);m_instancePicker->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    auto pl=new QHBoxLayout(m_instancePicker);pl->setContentsMargins(10,8,12,8);pl->setSpacing(10);m_pickerIcon=new QLabel;m_pickerIcon->setFixedSize(40,40);m_pickerIcon->setAlignment(Qt::AlignCenter);pl->addWidget(m_pickerIcon);
    auto pickerText=new QVBoxLayout;pickerText->setSpacing(1);pl->addLayout(pickerText,1);m_pickerName=elided(pickerText,"pickerName");m_pickerSub=elided(pickerText,"pickerSub");
    auto up=new QLabel;up->setPixmap(Ui::pixmap("chevron-down",16,dim).transformed(QTransform().rotate(180)));pl->addWidget(up);
    for(QWidget *w:{static_cast<QWidget*>(m_pickerIcon),static_cast<QWidget*>(m_pickerName),static_cast<QWidget*>(m_pickerSub),static_cast<QWidget*>(up)})w->setAttribute(Qt::WA_TransparentForMouseEvents);
    connect(m_instancePicker,&QPushButton::clicked,this,[this]{instancePickerMenu();});
    auto pickerSlot=new QWidget;auto pickerSlotLayout=new QHBoxLayout(pickerSlot);pickerSlotLayout->setContentsMargins(0,0,0,0);pickerSlotLayout->addWidget(m_instancePicker,0,Qt::AlignLeft|Qt::AlignVCenter);pickerSlotLayout->addStretch();bl->addWidget(pickerSlot,0,0);
    m_playButton=new QPushButton;m_playButton->setObjectName("homePlay");m_playButton->setProperty("play",true);m_playButton->setCursor(Qt::PointingHandCursor);m_playButton->setMinimumSize(250,64);m_playButton->setIconSize(QSize(24,24));
    connect(m_playButton,&QPushButton::clicked,this,[this]{play();});bl->addWidget(m_playButton,0,1,Qt::AlignCenter);
    auto player=new QWidget;player->setMaximumWidth(310);auto playerLayout=new QHBoxLayout(player);playerLayout->setContentsMargins(0,0,0,0);playerLayout->setSpacing(10);
    auto playerText=new QVBoxLayout;playerText->setSpacing(1);playerLayout->addLayout(playerText,1);m_playerName=elided(playerText,"playerName",Qt::AlignRight|Qt::AlignVCenter);m_playerType=elided(playerText,"playerType",Qt::AlignRight|Qt::AlignVCenter);
    m_playerAvatar=new QLabel;m_playerAvatar->setObjectName("playerAvatar");m_playerAvatar->setFixedSize(44,44);playerLayout->addWidget(m_playerAvatar,0,Qt::AlignVCenter);
    auto playerSlot=new QWidget;auto playerSlotLayout=new QHBoxLayout(playerSlot);playerSlotLayout->setContentsMargins(0,0,0,0);playerSlotLayout->addStretch();playerSlotLayout->addWidget(player);bl->addWidget(playerSlot,0,2);bl->setColumnStretch(0,1);bl->setColumnStretch(2,1);l->addWidget(bar);
    // EBALIA's own mods, with covers like the news below them.
    auto modsSection=new QWidget;modsSection->setObjectName("homeMods");auto ml=new QVBoxLayout(modsSection);ml->setContentsMargins(28,16,28,0);ml->setSpacing(12);
    auto modsHead=new QHBoxLayout;modsHead->setSpacing(12);ml->addLayout(modsHead);label("EBALIA Mods",modsHead,"sectionTitle",false);modsHead->addStretch();
    auto patreon=button("EBALIA · Patreon",modsHead,[this]{openCreatorDialog(true);},this,false,"heart");patreon->setObjectName("patreonInviteButton");patreon->setProperty("patreon",true);patreon->setIcon(Ui::icon("heart",Qt::white));
    auto allMods=button(k("See all"),modsHead,[this]{openCreatorDialog(false);},this);allMods->setObjectName("myModsButton");allMods->setProperty("link",true);allMods->setIcon(Ui::icon("chevron-right",Ui::accent()));allMods->setLayoutDirection(Qt::RightToLeft);
    auto modCards=new QHBoxLayout;modCards->setSpacing(14);ml->addLayout(modCards);
    {const auto art=InstanceIcons::backgrounds();
     for(const auto &mod:creatorMods()){
        auto card=new ContentButton;card->setObjectName("modCard");card->setCursor(Qt::PointingHandCursor);card->setAccessibleName(mod.name);card->setToolTip(Language::key(mod.description));card->setMinimumWidth(150);card->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
        auto cl=new QVBoxLayout(card);cl->setContentsMargins(1,1,1,12);cl->setSpacing(8);auto cover=new Cover(11);cover->setObjectName("newsCover");cover->setAspectRatio(16.0/9.0);cl->addWidget(cover);
        auto body=new QVBoxLayout;body->setContentsMargins(14,0,14,0);body->setSpacing(4);cl->addLayout(body);
        auto meta=elided(body,"newsMeta");meta->setText("EBALIA  ·  "+(mod.soon?text("PRÓXIMAMENTE","COMING SOON","EM BREVE"):QString("CURSEFORGE")));auto title=label(mod.name,body,"newsTitle");title->setAlignment(Qt::AlignLeft|Qt::AlignTop);body->addStretch();
        for(auto w:{static_cast<QWidget*>(meta),static_cast<QWidget*>(title)})w->setAttribute(Qt::WA_TransparentForMouseEvents);
        connect(card,&QPushButton::clicked,this,[this,mod]{if(mod.soon)openCreatorDialog(false);else QDesktopServices::openUrl(QUrl(mod.url()));});modCards->addWidget(card,1);
        // The square mod picture over a darkened launcher background, so it fills the cover like a news image.
        const auto background=!mod.background.isEmpty()?mod.background:art.isEmpty()?QString(":/art/f1_2.jpg"):art[int(qHash(mod.slug)%uint(art.size()))];const auto picture=":/creations/"+mod.slug+".png";
        Ui::loadArt(background,{640,360},cover,[cover,picture](const QPixmap &p){
            QPixmap canvas(640,360);canvas.fill(QColor(20,20,24));QPainter painter(&canvas);painter.setRenderHint(QPainter::SmoothPixmapTransform);painter.setRenderHint(QPainter::Antialiasing);
            painter.drawPixmap(0,0,Ui::cover(p,{640,360}));painter.fillRect(canvas.rect(),QColor(8,8,12,170));
            const QPixmap image(picture);if(!image.isNull()){QRect box(0,0,240,240);box.moveCenter(canvas.rect().center());QPainterPath frame;frame.addRoundedRect(QRectF(box.adjusted(-6,-6,6,6)),24,24);painter.fillPath(frame,QColor(0,0,0,120));
                QPainterPath clip;clip.addRoundedRect(QRectF(box),18,18);painter.setClipPath(clip);painter.drawPixmap(box,image);}
            painter.end();cover->setPixmap(canvas);});
     }}
    l->addWidget(modsSection);
    auto news=new QWidget;news->setObjectName("homeNews");auto nl=new QVBoxLayout(news);nl->setContentsMargins(28,14,28,20);nl->setSpacing(12);
    auto head=new QHBoxLayout;nl->addLayout(head);label(k("What's new"),head,"sectionTitle",false);head->addStretch();
    auto all=button(k("See all"),head,[this]{showPage(News);},this);all->setProperty("link",true);all->setIcon(Ui::icon("chevron-right",Ui::accent()));all->setLayoutDirection(Qt::RightToLeft);
    m_homeNews=new QHBoxLayout;m_homeNews->setSpacing(14);nl->addLayout(m_homeNews);l->addWidget(news);
    return home;
}
QWidget *MainWindow::buildInstances() {
    m_library=new QStackedWidget;m_library->setObjectName("instancesPage");m_grid=new InstanceGrid;m_detail=new InstanceDetail;m_library->addWidget(m_grid);m_library->addWidget(m_detail);
    auto choose=[this](const QString &dir){m_selectedDir=dir;selection();};
    m_grid->open=[this,choose](const QString &dir){choose(dir);m_library->setCurrentWidget(m_detail);refreshInstances();};
    m_grid->play=[this,choose](const QString &dir){choose(dir);play();};
    m_grid->settings=[this,choose](const QString &dir){choose(dir);editInstance();};
    m_grid->menu=[this,choose](const QString &dir){choose(dir);instanceMenu();};
    m_grid->create=[this]{createInstance();};m_grid->importPack=[this]{createInstance(false,CreateInstanceDialog::Import);};
    m_grid->newGroup=[this]{bool ok;auto name=QInputDialog::getText(this,Language::key("New group"),Language::key("Group name"),QLineEdit::Normal,{},&ok).trimmed();if(!ok||name.isEmpty())return;auto groups=QSettings().value("ui/groups").toStringList();if(!groups.contains(name))groups<<name;QSettings().setValue("ui/groups",groups);refreshInstances();};
    m_grid->groupMenu=[this](const QString &group){groupMenu(group);};
    m_detail->back=[this]{m_library->setCurrentWidget(m_grid);refreshInstances();};
    m_detail->play=[this]{play();};m_detail->settings=[this]{editInstance();};m_detail->log=[this]{showLog(selected().dir,this);};
    m_detail->changeIcon=[this](const QString &key,const QImage &custom){setInstanceIcon(m_detail->dir(),key,custom);refreshInstances();};
    m_detail->changeBackground=[this]{changeBackground(m_detail->dir());};
    m_detail->exportZip=[this]{exportInstance();};m_detail->copy=[this]{copyInstance();};m_detail->remove=[this]{removeInstance();};
    m_detail->mods=[this]{manageMods();};m_detail->savePack=[this]{savePack();};m_detail->changeGroup=[this]{changeGroup();};
    m_detail->findMods=[this]{auto i=selected();if(i.dir.isEmpty())return;if(i.loader=="vanilla"){error(text("Esta instancia es Vanilla. Creá una instancia con Fabric, Quilt, Forge o NeoForge para usar mods.","This is a Vanilla instance. Create an instance with Fabric, Quilt, Forge or NeoForge to use mods.","Esta instância é Vanilla. Crie uma instância com Fabric, Quilt, Forge ou NeoForge para usar mods."));return;}showPage(Explore);m_target->setCurrentIndex(m_target->findData(i.dir));};
    m_detail->openFolder=[this](const QString &folder){auto i=selected();if(i.dir.isEmpty())return;auto path=folder.isEmpty()?i.dir:i.dir+"/"+folder;QDir().mkpath(path);QDesktopServices::openUrl(QUrl::fromLocalFile(path));};
    auto deleteShortcut=new QShortcut(QKeySequence::Delete,m_library);deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);connect(deleteShortcut,&QShortcut::activated,this,[this]{removeInstance();});
    return m_library;
}
void MainWindow::openCreatorDialog(bool patreonOnly){
    const auto name=patreonOnly?"patreonInvite":"myModsDialog";
    if(auto dialog=findChild<QDialog*>(name)){dialog->show();dialog->raise();dialog->activateWindow();return;}
    auto dialog=new CreatorDialog(patreonOnly,this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowModality(Qt::NonModal);dialog->setSizeGripEnabled(true);dialog->show();dialog->raise();dialog->activateWindow();
}
QWidget *MainWindow::buildExplore() {
    auto s=section(k("Discover mods"),Language::key("Mods for your Minecraft version and loader. Select one or more mods."));auto l=s.layout;
    auto filters=new QHBoxLayout;filters->setSpacing(10);l->addLayout(filters);
    m_target=new QComboBox;m_target->setObjectName("modTarget");m_target->setMinimumWidth(240);filters->addWidget(m_target,1);connect(m_target,qOverload<int>(&QComboBox::activated),this,[this]{m_results->clear();m_hits={};searchMods();});
    m_modProvider=new QComboBox;m_modProvider->addItem(InstanceIcons::provider("modrinth"),"Modrinth","modrinth");m_modProvider->addItem(InstanceIcons::provider("curseforge"),"CurseForge","curseforge");m_modProvider->setMinimumWidth(170);filters->addWidget(m_modProvider);
    connect(m_modProvider,qOverload<int>(&QComboBox::activated),this,[this]{m_results->clear();m_offset=0;searchMods();});
    auto search=new QHBoxLayout;search->setSpacing(10);l->addLayout(search);m_query=new QLineEdit;m_query->setObjectName("modQuery");m_query->setPlaceholderText("Sodium, Dynamic Lights, FallingTree, Veinminer…");m_query->addAction(Ui::icon("search",dim),QLineEdit::LeadingPosition);m_query->setClearButtonEnabled(true);search->addWidget(m_query,1);
    button(text("Buscar","Search","Buscar"),search,[this]{searchMods();},this,true,"search");connect(m_query,&QLineEdit::returnPressed,this,[this]{searchMods();});
    m_results=new QListWidget;m_results->setObjectName("modResults");m_results->setSelectionMode(QAbstractItemView::ExtendedSelection);m_results->setIconSize(QSize(44,44));m_results->setWordWrap(true);l->addWidget(m_results,1);
    auto browseRow=new Ui::ResponsiveRow(720);l->addWidget(browseRow);auto browse=browseRow->box();browse->setSpacing(10);
    button(text("Ver instalación y dependencias","Preview installation & dependencies","Ver instalação e dependências"),browse,[this]{QJsonArray projects;for(auto item:m_results->selectedItems()){auto p=item->data(Qt::UserRole).toJsonObject();projects.append(QJsonObject{{"project_id",p["project_id"]},{"name",p["title"]}});}if(!projects.isEmpty())preview(projects,m_target->currentData().toString());},this,true,"download");
    browse->addStretch();
    // Like Prism Launcher: reaching the end of the list loads the next results.
    connect(m_results->verticalScrollBar(),&QScrollBar::valueChanged,this,[this](int value){
        auto bar=m_results->verticalScrollBar();
        if(m_jobs||m_results->count()==0||m_results->property("lastPage").toBool()||value<bar->maximum()-48)return;
        searchMods(m_offset+30);});
    return s.widget;
}
QWidget *MainWindow::buildLost() {
    auto s=section(k("Lost versions"),text("El archivo de EBALIA. Cada versión conserva su instalación y su forma de inicio original.","The EBALIA archive. Each version keeps its own installation and original launch method.","O arquivo da EBALIA. Cada versão mantém sua instalação e sua forma original de iniciar."));auto l=s.layout;
    auto filters=new QHBoxLayout;filters->setSpacing(14);l->addLayout(filters);
    m_lostFilter=new QLineEdit;m_lostFilter->setObjectName("lostFilter");m_lostFilter->setPlaceholderText(text("Buscar por nombre o categoría…","Search by name or category…","Buscar por nome ou categoria…"));m_lostFilter->setClearButtonEnabled(true);m_lostFilter->addAction(Ui::icon("search",dim),QLineEdit::LeadingPosition);m_lostFilter->setMinimumWidth(150);
    filters->addWidget(m_lostFilter,1);button(k("Setup & diagnostics"),filters,[this]{SetupDialog dialog(this);Ui::openWindow(dialog);},this,false,"wrench")->setObjectName("lostDiagnostics");
    connect(m_lostFilter,&QLineEdit::textChanged,this,[this]{refreshLost();});
    m_lost=new QListWidget;m_lost->setObjectName("lostList");m_lost->setItemDelegate(new ArchiveDelegate(m_lost));m_lost->setMinimumWidth(210);m_lost->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto detail=new QFrame;detail->setObjectName("lostDetail");detail->setMinimumWidth(310);auto dl=new QVBoxLayout(detail);dl->setContentsMargins(1,1,1,1);dl->setSpacing(0);
    auto panels=new ArchivePanels(m_lost,detail);panels->setObjectName("archivePanels");l->addWidget(panels,1);
    auto cover=new Cover(13);cover->setObjectName("lostCover");cover->setShaded(true);cover->setMinimumHeight(180);cover->setMaximumHeight(420);cover->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);m_lostImage=cover;dl->addWidget(cover,3);
    auto overlay=new QVBoxLayout(cover);overlay->setContentsMargins(20,20,20,20);overlay->addStretch();label("EBALIA  /  "+k("Lost versions").toUpper(),overlay,"archiveEyebrow");m_lostTitle=label({},overlay,"detailTitle");m_lostTitle->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);
    auto description=new QScrollArea;description->setWidgetResizable(true);description->setFrameShape(QFrame::NoFrame);description->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);description->setMinimumHeight(75);dl->addWidget(description,2);
    auto textBody=new QWidget;description->setWidget(textBody);auto body=new QVBoxLayout(textBody);body->setContentsMargins(20,18,20,18);body->setSpacing(10);
    m_lostInfo=label({},body,"muted");body->addStretch();
    auto actions=new QHBoxLayout;actions->setContentsMargins(16,14,16,16);actions->setSpacing(12);dl->addLayout(actions);
    m_lostPlay=button({},actions,[this]{lostAction(true);},this,true);m_lostPlay->setObjectName("lostPlay");m_lostPlay->setMinimumSize(100,48);m_lostPlay->setIconSize(QSize(18,18));
    m_lostInstall=button({},actions,[this]{lostAction(false);},this);m_lostInstall->setObjectName("lostInstall");m_lostInstall->setProperty("secondary",true);m_lostInstall->setMinimumSize(110,48);m_lostInstall->setIcon(Ui::icon("download",Qt::white));m_lostInstall->setIconSize(QSize(18,18));
    auto folder=new QToolButton;folder->setIcon(Ui::icon("folder"));folder->setIconSize(QSize(20,20));folder->setToolTip(k("Open instance folder"));folder->setAccessibleName(folder->toolTip());folder->setCursor(Qt::PointingHandCursor);actions->addWidget(folder);
    connect(folder,&QToolButton::clicked,this,[this]{auto id=filename(m_lost);for(const auto &v:m_versions->getVersions())if(v.id==id&&m_versions->isVersionInstalled(v))QDesktopServices::openUrl(QUrl::fromLocalFile(m_versions->getInstallPath(v)));});
    actions->addStretch();
    auto maintenance=new QHBoxLayout;maintenance->setContentsMargins(16,0,16,12);dl->addLayout(maintenance);
    m_lostUninstall=button(text("Desinstalar","Uninstall","Desinstalar"),maintenance,[this] {
        if(m_jobs||m_java->isRunning())return;
        const auto id=filename(m_lost);
        for(const auto &v:m_versions->getVersions())if(v.id==id&&m_versions->isVersionInstalled(v)) {
            const auto dest=m_versions->getInstallPath(v);
            const QFileInfo target(dest);
            const auto base=QFileInfo(m_versions->getVersionsDir()).canonicalFilePath();
            if(base.isEmpty()||target.isSymLink()||target.canonicalFilePath()!=base+"/"+v.id||v.id.contains('/')||v.id.contains('\\')) {
                error(text("Ruta de instalación inválida.","Invalid installation path.","Caminho de instalação inválido."));return;
            }
            const auto message=text("Se moverá a la papelera la carpeta completa de %1, incluidos sus mundos y ajustes. ¿Continuar?",
                "Move the entire %1 folder to the trash, including its worlds and settings?",
                "Mover a pasta completa de %1 para a lixeira, incluindo mundos e configurações?").arg(v.name);
            if(QMessageBox::question(this,text("Desinstalar versión","Uninstall version","Desinstalar versão"),message,QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
            work(text("Desinstalando…","Uninstalling…","Desinstalando…"),[dest] {
                if(!QFile::moveToTrash(dest))throw std::runtime_error("No se pudo mover la instalación a la papelera. No se borró permanentemente.");
                return QJsonObject{};
            },[this](QJsonObject){refreshLost();m_status->setText(text("Versión desinstalada · archivos en la papelera","Version uninstalled · files in trash","Versão desinstalada · arquivos na lixeira"));});
            return;
        }
    },this,false,"trash-2");m_lostUninstall->setObjectName("lostUninstall");maintenance->addStretch();
    connect(m_lost,&QListWidget::currentRowChanged,this,[this]{lostSelection();});
    label(text("Los paquetes de Windows necesitan Windows o Wine. La disponibilidad depende del archivo original.","Windows packages require Windows or Wine. Availability depends on the original archive.","Pacotes do Windows precisam de Windows ou Wine. A disponibilidade depende do arquivo original."),l,"muted");
    return s.widget;
}
QWidget *MainWindow::buildPacks() {
    auto s=section(k("My packs"),text("Un pack recuerda tus mods favoritos y busca sus versiones compatibles en la instancia de destino.","A pack remembers your favorite mods and finds compatible versions for the target instance.","Um pack lembra seus mods favoritos e encontra versões compatíveis para a instância de destino."));auto l=s.layout;
    button(text("Importar pack","Import pack","Importar pack"),s.actions,[this]{auto file=QFileDialog::getOpenFileName(this,{}, {},"EBALIA pack (*.json)");if(file.isEmpty())return;try{ModRepository(m_root).savePack(ModRepository::read(file));refreshPacks();}catch(...){error(exception());}},this,false,"upload");
    button(text("Exportar pack","Export pack","Exportar pack"),s.actions,[this]{int i=m_packs->currentRow();if(i<0||i>=m_packData.size())return;auto file=QFileDialog::getSaveFileName(this,{},"pack.ebalia.json","EBALIA pack (*.json)");if(file.isEmpty())return;try{auto pack=m_packData[i].toObject();pack.remove("path");ModRepository::write(file,pack);}catch(...){error(exception());}},this,false,"share-2");
    m_packs=new QListWidget;m_packs->setObjectName("packList");m_packs->setIconSize(QSize(36,36));l->addWidget(m_packs,1);m_packDetails=label({},l,"muted");
    connect(m_packs,&QListWidget::currentRowChanged,this,[this]{int i=m_packs->currentRow();if(i<0||i>=m_packData.size()){m_packDetails->clear();return;}QStringList names;for(const auto &p:m_packData[i].toObject()["projects"].toArray())names<<p.toObject()["name"].toString();m_packDetails->setText(names.join(" · "));});
    auto row=new QHBoxLayout;l->addLayout(row);button(text("Aplicar a una instancia","Apply to an instance","Aplicar a uma instância"),row,[this]{applyPack();},this,true,"download");row->addStretch();
    label(text("Para crear un pack: Instancias → Guardar mods como pack. No necesitás un RAR ni copiar carpetas.","To create a pack: My instances → Save mods as a pack. No archives or folder copying needed.","Para criar um pack: Instâncias → Salvar mods como pack. Sem RAR nem cópia de pastas."),l,"muted");
    return s.widget;
}
QWidget *MainWindow::buildSkins() {
    auto s=section(k("Skins"),{});
    s.layout->addWidget(new SkinsPage(m_root,m_accounts,m_mc,[this](const QString &title,std::function<QJsonObject()> job,std::function<void(QJsonObject)> done){work(title,job,done);},this),1);
    return s.widget;
}
QWidget *MainWindow::buildNews() {
    auto s=section(k("News"),k("Minecraft updates and the latest creations from EBALIA."));
    button(text("Actualizar","Refresh","Atualizar"),s.actions,[this]{refreshNews();m_patreon->refreshNews();},this,false,"refresh-cw");
    auto tabs=new QTabWidget;tabs->setObjectName("newsTabs");tabs->setDocumentMode(true);s.layout->addWidget(tabs,1);auto minecraft=new QWidget;auto l=new QVBoxLayout(minecraft);l->setContentsMargins(0,12,0,0);tabs->addTab(minecraft,"Minecraft");tabs->addTab(new PatreonNewsPage(m_patreon),"EBALIA · Patreon");
    connect(tabs,&QTabWidget::currentChanged,this,[this](int index){if(index==1)m_patreon->refreshNews();});
    m_news=new QListWidget;m_news->setObjectName("newsList");m_news->setIconSize(QSize(176,80));m_news->setWordWrap(true);l->addWidget(m_news,1);
    connect(m_news,&QListWidget::itemDoubleClicked,this,[](QListWidgetItem *i){auto u=QUrl(i->data(Qt::UserRole).toString());if(minecraftLink(u))QDesktopServices::openUrl(u);});
    auto row=new QHBoxLayout;l->addLayout(row);button(text("Leer artículo","Read article","Ler artigo"),row,[this]{if(auto i=m_news->currentItem())emit m_news->itemDoubleClicked(i);},this,true,"external-link");row->addStretch();
    return s.widget;
}
QWidget *MainWindow::buildCommunity() {
    auto s=section(k("Community"),{});auto tabs=new QTabWidget;tabs->setObjectName("communityTabs");tabs->setIconSize(QSize(18,18));tabs->setDocumentMode(true);tabs->setUsesScrollButtons(false);tabs->setElideMode(Qt::ElideRight);s.layout->addWidget(tabs,1);
    const QList<QPair<QString,QString>> sections{{"image",k("Fan arts")},{"globe",k("Servers")},{"book-open",k("Learn")},{"heart",k("Support & community")},{"sparkles",k("EBALIA · Patreon")}};
    for(int n=0;n<sections.size();++n)tabs->addTab(new CommunityPage(n,m_root,m_patreon),Ui::icon(sections[n].first),QString(sections[n].second).replace("&","&&")); // "&" would mark a keyboard shortcut and show as "_"
    return s.widget;
}
void MainWindow::startTour() {
    if(auto tour=findChild<QWidget*>("interactiveTour")){tour->raise();return;}
    if(property("launcherUpdating").toBool())return;
    new GuidedTour(this,[this](int page){showPage(page);});
}
QWidget *MainWindow::buildGuide() {
    auto s=section(k("Guide & tutorial"),text("Esta guía siempre está disponible en la barra lateral.","This guide is always available in the sidebar.","Este guia está sempre disponível na barra lateral."));auto l=s.layout;
    auto hero=new QFrame;hero->setObjectName("guideMascotCard");hero->setStyleSheet("#guideMascotCard{background:#1b2019;border:1px solid #2f3d2a;border-radius:14px;}");l->addWidget(hero);
    auto heroRow=new QHBoxLayout(hero);heroRow->setContentsMargins(12,8,18,10);heroRow->setSpacing(14);
    auto mascot=new TourMascot;mascot->setObjectName("guideMascot");mascot->setFixedSize(100,146);mascot->setToolTip("Silence");heroRow->addWidget(mascot,0,Qt::AlignBottom);
    auto wave=new QTimer(mascot);wave->setInterval(7000);wave->start();
    connect(wave,&QTimer::timeout,mascot,[mascot]{static const TourMascot::Mood moods[]{TourMascot::Happy,TourMascot::Wink,TourMascot::Idle,TourMascot::Cheer,TourMascot::Think,TourMascot::Point};static int next=0;mascot->react(moods[next++%6]);});
    mascot->setMood(TourMascot::Happy);
    auto heroText=new QVBoxLayout;heroText->setSpacing(10);heroRow->addLayout(heroText,1);heroText->addStretch();
    auto hello=new QLabel(text("¡Hola! Soy Silence. Te acompaño en la guía interactiva y te muestro cada parte del launcher.","Hi! I'm Silence. I'll walk you through the interactive tour and show you every part of the launcher.","Olá! Eu sou o Silence. Vou te acompanhar no guia interativo e mostrar cada parte do launcher."));
    hello->setObjectName("guideMascotHello");hello->setWordWrap(true);hello->setStyleSheet("font-size:15px;");heroText->addWidget(hello);
    auto tourRow=new QHBoxLayout;heroText->addLayout(tourRow);heroText->addStretch();button(text("Iniciar guía interactiva","Start interactive tour","Iniciar guia interativo"),tourRow,[this]{startTour();},this,true,"compass")->setObjectName("startInteractiveTour");tourRow->addStretch();
    auto guide=new QTextBrowser;guide->setObjectName("guide");guide->setOpenExternalLinks(false);guide->setHtml(text(
        "<h2>1. Elegí tu modo</h2><p><b>Mis instancias</b> es el cliente normal de Minecraft. <b>Versiones perdidas</b> abre el archivo de EBALIA.</p><h2>2. Creá una instancia</h2><p>Elegí un nombre, una versión y un cargador. Vanilla es el juego original; Fabric, Quilt, Forge o NeoForge permiten mods. Los mundos y ajustes quedan separados. El catálogo se actualiza al abrir y cada 30 minutos. Las instancias existentes conservan su versión para cuidar tus mundos.</p><h2>3. Agregá mods</h2><p>En <b>Explorar mods</b>, seleccioná tu instancia y buscá Sodium, luces dinámicas, FallingTree o Veinminer. Revisá las dependencias y confirmá la instalación. La compatibilidad publicada no garantiza que todos los mods funcionen juntos.</p><h2>4. Guardá tus favoritos como pack</h2><p>En tu instancia, usá <b>Guardar mods como pack</b>. Luego aplicalo desde <b>Mis packs</b> a otra instancia. Buscamos una edición para su versión y cargador; si no existe, te lo mostramos antes de descargar. No copiamos un JAR incompatible. Los mods locales deben poder identificarse en Modrinth; los desactivados no se incluyen.</p><h2>5. Jugá</h2><p>Agregá una cuenta, instalá Java y pulsá <b>Jugar / Instalar</b>. Una vez terminada la instalación, pulsá Jugar. Si falta Java, el launcher indica qué versión requiere el juego. Podés elegir su ruta y memoria en los ajustes de instancia.</p><h2>Si algo falla</h2><p>Abrí <b>Ajustes de instancia → Registro</b>. Reparar vuelve a comprobar las descargas del juego. Quitar una instancia la mueve a la papelera local con sus mundos. Forge y NeoForge se instalan con sus instaladores oficiales y necesitan Java antes de comenzar. Las versiones perdidas pueden requerir Wine, Java antiguo o paquetes originales que ya no estén disponibles.</p>",
        "<h2>1. Choose your mode</h2><p><b>My instances</b> is the regular Minecraft client. <b>Lost versions</b> opens the EBALIA archive.</p><h2>2. Create an instance</h2><p>Choose a name, game version and loader. Vanilla is the original game; Fabric, Quilt, Forge or NeoForge support mods. Worlds and settings stay separate. The catalog refreshes at startup and every 30 minutes. Existing instances keep their game version to protect your worlds.</p><h2>3. Add mods</h2><p>In <b>Discover mods</b>, choose your instance and search for Sodium, dynamic lights, FallingTree or Veinminer. Review dependencies and confirm installation. Published compatibility does not guarantee that all mods work together.</p><h2>4. Save your favorites as a pack</h2><p>Use <b>Save mods as a pack</b> on your instance. Apply it to another instance from <b>My packs</b>. We look up builds for its game version and loader; unavailable mods are listed before downloading. Incompatible JARs are never copied. Local mods must be identifiable on Modrinth; disabled mods are excluded.</p><h2>5. Play</h2><p>Add an account, install Java and press <b>Play / Install</b>. After installation finishes, press Play. If Java is missing, the launcher tells you which version the game requires. Choose its path and memory in instance settings.</p><h2>Troubleshooting</h2><p>Open <b>Instance settings → Log</b>. Repair verifies game downloads again. Removing an instance moves it to local trash with its worlds. Forge and NeoForge use their official installers and require Java before installation. Lost versions may require Wine, older Java or original packages that are no longer available.</p>",
        "<h2>1. Escolha seu modo</h2><p><b>Minhas instâncias</b> é o cliente normal do Minecraft. <b>Versões perdidas</b> abre o arquivo da EBALIA.</p><h2>2. Crie uma instância</h2><p>Escolha um nome, uma versão e um carregador. Vanilla é o jogo original; Fabric, Quilt, Forge ou NeoForge permitem mods. Mundos e ajustes ficam separados. O catálogo é atualizado ao abrir e a cada 30 minutos. Instâncias existentes mantêm sua versão para preservar seus mundos.</p><h2>3. Adicione mods</h2><p>Em <b>Explorar mods</b>, escolha a instância e busque Sodium, luzes dinâmicas, FallingTree ou Veinminer. Revise as dependências e confirme a instalação. A compatibilidade publicada não garante que todos os mods funcionem juntos.</p><h2>4. Salve seus favoritos como pack</h2><p>Use <b>Salvar mods como pack</b> na instância. Depois aplique em outra instância por <b>Meus packs</b>. Buscamos edições para a versão e o carregador de destino; mods indisponíveis são mostrados antes do download. Não copiamos JARs incompatíveis. Mods locais precisam ser identificados no Modrinth; mods desativados não entram no pack.</p><h2>5. Jogue</h2><p>Adicione uma conta, instale Java e clique em <b>Jogar / Instalar</b>. Ao terminar a instalação, clique em Jogar. Se faltar Java, o launcher indica a versão necessária. Escolha o caminho e a memória nos ajustes da instância.</p><h2>Se algo falhar</h2><p>Abra <b>Ajustes da instância → Registro</b>. Reparar verifica novamente os downloads do jogo. Remover uma instância move seus mundos para a lixeira local. Forge e NeoForge usam seus instaladores oficiais e precisam de Java antes da instalação. Versões perdidas podem precisar de Wine, Java antigo ou pacotes originais indisponíveis.</p>"));l->addWidget(guide,1);
    auto row=new QHBoxLayout;l->addLayout(row);button(text("Crear mi primera instancia","Create my first instance","Criar minha primeira instância"),row,[this]{showPage(Instances);createInstance();},this,true,"plus");row->addStretch();
    // Questions and answers: a question opens its answer.
    label(text("Preguntas frecuentes","Questions & answers","Perguntas frequentes"),l,"sectionTitle",false)->setObjectName("guideFaqTitle");
    struct Qa{const char *es,*en,*pt,*aes,*aen,*apt;};
    const Qa faq[]{
        {"¿Cómo importo un modpack o una instancia de otro launcher?","How do I import a modpack or another launcher's instance?","Como importo um modpack ou uma instância de outro launcher?",
         "En Instancias, pulsá Importar y elegí el archivo (.zip, .mrpack) o la carpeta. Sirven packs de CurseForge y Modrinth, instancias de Prism, MultiMC, la app de CurseForge, FTB, ATLauncher, GDLauncher, Modrinth App, TLauncher y carpetas .minecraft. También podés pegar la carpeta dentro de la carpeta de instancias: el launcher la convierte sola. La versión de Minecraft y el cargador se detectan automáticamente.",
         "In Instances, press Import and choose the file (.zip, .mrpack) or folder. CurseForge and Modrinth packs work, and so do Prism, MultiMC, CurseForge app, FTB, ATLauncher, GDLauncher, Modrinth App and TLauncher instances and .minecraft folders. You can also paste the folder into the instances folder: the launcher converts it by itself. The Minecraft version and loader are detected automatically.",
         "Em Instâncias, clique em Importar e escolha o arquivo (.zip, .mrpack) ou a pasta. Funcionam packs do CurseForge e Modrinth, instâncias do Prism, MultiMC, app do CurseForge, FTB, ATLauncher, GDLauncher, Modrinth App, TLauncher e pastas .minecraft. Você também pode colar a pasta dentro da pasta de instâncias: o launcher a converte sozinho. A versão do Minecraft e o carregador são detectados automaticamente."},
        {"El juego se cierra al abrirlo, ¿qué hago?","The game closes when it starts. What do I do?","O jogo fecha ao abrir. O que eu faço?",
         "Al cerrarse, el launcher muestra qué falló y un botón para arreglarlo: instalar un mod que falta, descargar el driver de tu tarjeta gráfica, usar gráficos por software o cambiar la memoria. Si no reconoce la causa, abrí el registro completo y compartilo en el Discord de EBALIA.",
         "When it closes, the launcher shows what failed and a button to fix it: install a missing mod, download your graphics card's driver, use software graphics or change the memory. If the cause is not recognised, open the full log and share it on the EBALIA Discord.",
         "Ao fechar, o launcher mostra o que falhou e um botão para corrigir: instalar um mod que falta, baixar o driver da sua placa de vídeo, usar gráficos por software ou mudar a memória. Se a causa não for reconhecida, abra o registro completo e compartilhe no Discord da EBALIA."},
        {"¿Necesito comprar Minecraft?","Do I need to buy Minecraft?","Preciso comprar o Minecraft?",
         "Con una cuenta Microsoft que tenga Minecraft podés jugar en servidores oficiales y con tu skin. El perfil local sirve para jugar sin conexión. Bedrock necesita su propia compra (Minecraft para Windows o Google Play).",
         "A Microsoft account that owns Minecraft lets you play on official servers and with your skin. A local profile is for offline play. Bedrock needs its own purchase (Minecraft for Windows or Google Play).",
         "Uma conta Microsoft com Minecraft permite jogar em servidores oficiais e com sua skin. O perfil local serve para jogar offline. O Bedrock precisa de compra própria (Minecraft para Windows ou Google Play)."},
        {"¿Cómo instalo mods?","How do I install mods?","Como instalo mods?",
         "Creá una instancia con Fabric, Quilt, Forge o NeoForge, abrí Explorar mods, elegí la instancia y buscá el mod. El launcher elige la versión compatible e instala sus dependencias.",
         "Create an instance with Fabric, Quilt, Forge or NeoForge, open Discover mods, choose the instance and search for the mod. The launcher picks the compatible version and installs its dependencies.",
         "Crie uma instância com Fabric, Quilt, Forge ou NeoForge, abra Explorar mods, escolha a instância e procure o mod. O launcher escolhe a versão compatível e instala as dependências."},
        {"¿Qué Java necesito?","Which Java do I need?","Qual Java eu preciso?",
         "Ninguno: el launcher descarga el Java que necesita cada versión de Minecraft. Si querés usar otro, elegilo en los ajustes de la instancia.",
         "None: the launcher downloads the Java each Minecraft version needs. To use another one, choose it in the instance settings.",
         "Nenhum: o launcher baixa o Java que cada versão do Minecraft precisa. Para usar outro, escolha-o nas configurações da instância."},
        {"¿Funciona en una máquina virtual o sin tarjeta gráfica?","Does it work in a virtual machine or without a graphics card?","Funciona em máquina virtual ou sem placa de vídeo?",
         "Sí, con gráficos por software: el launcher te lo ofrece si el juego no encuentra OpenGL, o activalo en los ajustes de la instancia. Va más lento que con una tarjeta gráfica.",
         "Yes, with software graphics: the launcher offers it when the game finds no OpenGL, or turn it on in the instance settings. It runs slower than with a graphics card.",
         "Sim, com gráficos por software: o launcher oferece quando o jogo não encontra OpenGL, ou ative nas configurações da instância. Fica mais lento do que com placa de vídeo."},
        {"¿Dónde están mis mundos y mods?","Where are my worlds and mods?","Onde estão meus mundos e mods?",
         "Cada instancia tiene su carpeta con su nombre. Abrila con el botón de carpeta en la página de la instancia: ahí están saves (mundos), mods, resourcepacks y shaderpacks.",
         "Each instance has its own folder with its name. Open it with the folder button on the instance page: saves (worlds), mods, resourcepacks and shaderpacks are there.",
         "Cada instância tem sua pasta com seu nome. Abra com o botão de pasta na página da instância: lá estão saves (mundos), mods, resourcepacks e shaderpacks."},
        {"¿Cómo actualizo el launcher?","How do I update the launcher?","Como atualizo o launcher?",
         "Cuando hay una versión nueva aparece Actualizar en la barra lateral: descarga, verifica y reinicia el launcher. Tus instancias, mundos y cuentas se conservan.",
         "When a new version is out, Update appears in the sidebar: it downloads, verifies and restarts the launcher. Your instances, worlds and accounts are kept.",
         "Quando há uma versão nova, Atualizar aparece na barra lateral: baixa, verifica e reinicia o launcher. Suas instâncias, mundos e contas são mantidos."},
        {"¿Puedo usarlo sin instalarlo, desde un USB?","Can I use it without installing, from a USB stick?","Posso usar sem instalar, de um pendrive?",
         "Sí: descargá la versión Portable desde la página. Todo (instancias, cuentas y ajustes) queda en su carpeta ebalia-data.",
         "Yes: download the Portable version from the website. Everything (instances, accounts and settings) stays in its ebalia-data folder.",
         "Sim: baixe a versão Portable no site. Tudo (instâncias, contas e configurações) fica na pasta ebalia-data."},
        {"¿Cómo cambio el icono, el fondo o mi foto de perfil?","How do I change the icon, background or my profile picture?","Como mudo o ícone, o fundo ou minha foto de perfil?",
         "Icono y fondo: en los ajustes de la instancia o en su página (Cambiar fondo). Foto de perfil: en el menú de tu cuenta, arriba a la izquierda, Cambiar imagen de perfil.",
         "Icon and background: in the instance settings or on its page (Change background). Profile picture: in your account menu, top left, Change profile picture.",
         "Ícone e fundo: nas configurações da instância ou na sua página (Mudar fundo). Foto de perfil: no menu da sua conta, no canto superior esquerdo, Mudar imagem de perfil."},
        {"¿Qué son las versiones perdidas?","What are the lost versions?","O que são as versões perdidas?",
         "Versiones raras y antiguas de Minecraft rescatadas por EBALIA. Se descargan y verifican desde el launcher y se juegan con Java directamente, sin instalar nada más.",
         "Rare and old Minecraft versions rescued by EBALIA. They are downloaded and verified from the launcher and run with Java directly, with nothing else to install.",
         "Versões raras e antigas do Minecraft resgatadas pela EBALIA. São baixadas e verificadas pelo launcher e rodam com Java diretamente, sem instalar mais nada."},
        {"Mi antivirus o Windows avisa al descargar, ¿es seguro?","My antivirus or Windows warns when downloading. Is it safe?","Meu antivírus ou o Windows avisa ao baixar. É seguro?",
         "El launcher todavía no está firmado digitalmente, por eso Windows puede avisar. Descargalo solo desde la página oficial o GitHub y comprobá su SHA-256, que figura junto a cada descarga.",
         "The launcher is not digitally signed yet, so Windows may warn. Download it only from the official website or GitHub and check its SHA-256, shown next to each download.",
         "O launcher ainda não tem assinatura digital, por isso o Windows pode avisar. Baixe somente pelo site oficial ou GitHub e confira o SHA-256, mostrado ao lado de cada download."}};
    auto faqBox=new QVBoxLayout;faqBox->setSpacing(8);l->addLayout(faqBox);
    for(const auto &qa:faq){
        auto card=new QFrame;card->setObjectName("faqItem");card->setStyleSheet("#faqItem{background:#1e1e22;border:1px solid #2b2b30;border-radius:12px;}#faqItem QLabel{background:transparent;}");faqBox->addWidget(card);
        auto cl=new QVBoxLayout(card);cl->setContentsMargins(6,4,14,8);cl->setSpacing(2);
        auto question=new ContentButton;question->setObjectName("faqQuestion");question->setCheckable(true);question->setCursor(Qt::PointingHandCursor);question->setAccessibleName(text(qa.es,qa.en,qa.pt));
        question->setStyleSheet("#faqQuestion{border:0;background:transparent;padding:0;text-align:left;}");question->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);cl->addWidget(question);
        auto ql=new QHBoxLayout(question);ql->setContentsMargins(4,8,4,8);ql->setSpacing(8);auto arrow=new QLabel;arrow->setPixmap(Ui::pixmap("chevron-right",16,Ui::accent()));ql->addWidget(arrow,0,Qt::AlignTop);
        auto qtext=label(text(qa.es,qa.en,qa.pt),ql,"faqQuestionText");qtext->setStyleSheet("font-weight:700;font-size:14px;");qtext->setAlignment(Qt::AlignLeft|Qt::AlignVCenter);ql->setStretch(1,1);arrow->setFixedWidth(18);for(QWidget *w:{static_cast<QWidget*>(arrow),static_cast<QWidget*>(qtext)})w->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto answer=label(text(qa.aes,qa.aen,qa.apt),cl,"muted");answer->setObjectName("faqAnswer");answer->setContentsMargins(28,0,0,6);answer->hide();
        connect(question,&QPushButton::toggled,card,[arrow,answer](bool open){answer->setVisible(open);arrow->setPixmap(Ui::pixmap(open?"chevron-down":"chevron-right",16,Ui::accent()));});
    }
    return s.widget;
}
QWidget *MainWindow::buildSettings() {
    auto s=section(k("Settings"),text("Cuentas, almacenamiento y preferencias del launcher.","Accounts, storage and launcher preferences.","Contas, armazenamento e preferências do launcher."));
    auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);s.layout->addWidget(scroll,1);
    auto content=new QWidget;content->setObjectName("settingsContent");scroll->setWidget(content);auto outer=new QHBoxLayout(content);outer->setContentsMargins(0,0,12,12);
    auto column=new QVBoxLayout;column->setSpacing(16);outer->addLayout(column,1);outer->addStretch(0);content->setMaximumWidth(16777215);
    // Accounts
    auto accounts=settingsCard(column,"user-round",k("Accounts"),text("Microsoft permite usar servidores autenticados con tu cuenta. El perfil local sirve para jugar sin conexión.","Microsoft lets you use authenticated servers with your account. A local profile is for offline play.","A conta Microsoft permite acessar servidores autenticados. O perfil local serve para jogar offline."));
    m_accountList=new QListWidget;m_accountList->setObjectName("accountList");m_accountList->setIconSize(QSize(30,30));accounts->addWidget(m_accountList);
    connect(m_accountList,&QListWidget::itemClicked,this,[this](QListWidgetItem *item){auto uuid=item->data(Qt::UserRole).toString();if(!uuid.isEmpty())m_accounts->setActive(uuid);});
    auto accountRow=new QHBoxLayout;accountRow->setSpacing(10);accounts->addLayout(accountRow);
    button(text("Agregar cuenta Microsoft","Add Microsoft account","Adicionar conta Microsoft"),accountRow,[this]{account(true);},this,true,"plus");
    button(text("Agregar perfil local","Add local profile","Adicionar perfil local"),accountRow,[this]{account(false);},this,false,"user-round");
    auto remove=button(text("Quitar cuenta activa","Remove active account","Remover conta ativa"),accountRow,[this]{auto a=m_accounts->active();if(!a.uuid.isEmpty()&&QMessageBox::question(this,"EBALIA",text("¿Quitar la cuenta del launcher?","Remove this account from the launcher?","Remover esta conta do launcher?"))==QMessageBox::Yes)m_accounts->removeAccount(a.uuid);},this,false,"trash-2");
    remove->setProperty("danger",true);accountRow->addStretch();
    // Language
    auto language=settingsCard(column,"globe",k("Language"),{});
    auto languages=new QComboBox;languages->setObjectName("languageChoice");languages->setMaximumWidth(320);
    for(const auto &entry:QList<QPair<QString,QString>>{{"Español","es"},{"English","en"},{"Português","pt"},{"Deutsch","de"},{"Français","fr"},{"Italiano","it"},{"Русский","ru"},{"日本語","ja"},{"한국어","ko"},{"简体中文","zh"}})languages->addItem(entry.first,entry.second);
    languages->setCurrentIndex(languages->findData(Language::current));language->addWidget(languages);
    connect(languages,qOverload<int>(&QComboBox::activated),this,[this,languages]{Language::current=languages->currentData().toString();QSettings().setValue("ui/language",Language::current);QTimer::singleShot(0,this,[this]{build();});});
    // Theme: applied at once, like the language.
    auto appearance=settingsCard(column,"palette",text("Tema","Theme","Tema"),text("Cambia el color de acento y el tono de fondo del launcher.","Changes the launcher's accent colour and background tone.","Muda a cor de destaque e o tom de fundo do launcher."));
    auto themeRow=new QGridLayout;themeRow->setSpacing(8);appearance->addLayout(themeRow);int themeIndex=0;
    for(const auto &t:Ui::themes()){
        auto choice=new QPushButton(t.name);choice->setObjectName("themeChoice");choice->setProperty("theme",t.id);choice->setCheckable(true);choice->setChecked(Ui::theme()==t.id);choice->setCursor(Qt::PointingHandCursor);
        QPixmap dot(14,14);dot.fill(Qt::transparent);{QPainter p(&dot);p.setRenderHint(QPainter::Antialiasing);p.setBrush(QColor::fromHslF(t.accentHue/360.0f,0.55f,0.5f));p.setPen(Qt::NoPen);p.drawEllipse(0,0,14,14);}choice->setIcon(QIcon(dot));
        themeRow->addWidget(choice,themeIndex/3,themeIndex%3);++themeIndex;
        connect(choice,&QPushButton::clicked,this,[id=t.id,themeRow]{
            // Only the stylesheet changes: rebuilding every page made switching slow.
            Ui::setTheme(id);qApp->setStyleSheet(Ui::styleSheet());
            for(int n=0;n<themeRow->count();++n)if(auto b=qobject_cast<QPushButton*>(themeRow->itemAt(n)->widget()))b->setChecked(b->property("theme").toString()==id);});
    }
    // Java and diagnostics
    auto java=settingsCard(column,"wrench",k("Setup & diagnostics"),k("Java, OpenGL, OpenAL and Wine checks for every version. EBALIA downloads the Java each version needs."));
    auto javaRow=new QHBoxLayout;java->addLayout(javaRow);button(k("Setup & diagnostics"),javaRow,[this]{SetupDialog dialog(this);Ui::openWindow(dialog);},this,false,"wrench");javaRow->addStretch();
    // Providers
    auto providers=settingsCard(column,"key-round",k("Provider settings"),k("Use your EBALIA application credentials. No passwords or keys from another launcher are required."));
    auto form=new QFormLayout;form->setHorizontalSpacing(14);form->setVerticalSpacing(10);providers->addLayout(form);
    auto curse=new QLineEdit(QSettings().value("integrations/curseforgeKey").toString());curse->setObjectName("curseForgeKey");curse->setEchoMode(QLineEdit::Password);curse->setPlaceholderText(curse->text().isEmpty()&&!ModRepository::curseForgeKey().isEmpty()?k("Built-in key in use"):QString("$2a$10$…"));
    auto microsoft=new QLineEdit(QSettings().value("auth/microsoftClientId").toString());microsoft->setObjectName("microsoftClientId");
    form->addRow("CurseForge API key",curse);form->addRow("Microsoft OAuth client ID",microsoft);
    auto providerRow=new QHBoxLayout;providerRow->setSpacing(10);providers->addLayout(providerRow);
    button(Language::key("Save"),providerRow,[this,curse,microsoft]{QSettings settings;settings.setValue("integrations/curseforgeKey",curse->text().trimmed());settings.setValue("auth/microsoftClientId",microsoft->text().trimmed());m_status->setText(k("Saved"));},this,true);
    auto getKey=button(k("Get a key"),providerRow,[]{QDesktopServices::openUrl(QUrl("https://console.curseforge.com/"));},this,false,"external-link");getKey->setProperty("link",true);providerRow->addStretch();
    // Data
    auto data=settingsCard(column,"folder",k("Data"),text("EBALIA %1 · La actualización automática es del catálogo y las noticias. No cambia los mundos, mods ni la versión de tus instancias.","EBALIA %1 · Automatic refresh updates the catalog and news. It does not change worlds, mods or existing instance versions.","EBALIA %1 · A atualização automática é do catálogo e das notícias. Não altera mundos, mods ou versões das instâncias.").arg(QCoreApplication::applicationVersion()));
    auto dataRow=new QHBoxLayout;dataRow->setSpacing(10);data->addLayout(dataRow);
    button(text("Abrir carpeta de datos","Open data folder","Abrir pasta de dados"),dataRow,[this]{QDesktopServices::openUrl(QUrl::fromLocalFile(m_root));},this,false,"folder");
    button(text("Abrir papelera de instancias","Open instance trash","Abrir lixeira de instâncias"),dataRow,[this]{QDir().mkpath(m_mc->mcDir()+"/trash");QDesktopServices::openUrl(QUrl::fromLocalFile(m_mc->mcDir()+"/trash"));},this,false,"trash-2");
    button(text("Actualizar catálogo de versiones","Refresh version catalog","Atualizar catálogo de versões"),dataRow,[this]{m_mc->fetchManifest();},this,false,"refresh-cw");dataRow->addStretch();
    m_catalog=label(m_manifest.isEmpty()?text("Consultando versiones…","Checking versions…","Consultando versões…"):text("Catálogo actualizado · ","Catalog updated · ","Catálogo atualizado · ")+QString::number(m_manifest.size())+text(" versiones"," versions"," versões"),data,"muted");
    column->addStretch();
    return s.widget;
}
void MainWindow::showPage(int i) {
    if(!m_pages)return;i=qBound(0,i,m_pages->count()-1);m_pages->setCurrentIndex(i);
    {QSignalBlocker block(m_nav);if(i==Settings){m_nav->setCurrentRow(-1);m_nav->clearSelection();}else m_nav->setCurrentRow(i);}
    m_navSettings->setChecked(i==Settings);
    // The mod list loads when the tab opens: popular mods for the selected instance until something is searched.
    if(i==Explore&&m_results&&m_results->count()==0&&!m_target->currentData().toString().isEmpty())searchMods();
}
void MainWindow::setLauncherUpdate(LauncherUpdate state,const QString &version){
    m_launcherUpdate=state;if(!version.isEmpty())m_launcherVersion=version;
    const bool available=state==LauncherUpdate::Available&&!m_launcherVersion.isEmpty();
    if(m_sidebarUpdate){
        const auto title=text("Actualizar a %1","Update to %1","Atualizar para %1").arg(m_launcherVersion);
        m_sidebarUpdate->setVisible(available||state==LauncherUpdate::Installing);m_sidebarUpdate->setEnabled(available);
        const bool compact=centralWidget()&&centralWidget()->findChild<QFrame*>("sidebar")&&centralWidget()->findChild<QFrame*>("sidebar")->property("compact").toBool();
        m_sidebarUpdate->setText(compact?QString():"  "+(state==LauncherUpdate::Installing?text("Actualizando…","Updating…","Atualizando…"):title));m_sidebarUpdate->setToolTip(title);m_sidebarUpdate->setAccessibleName(title);
    }
}
void MainWindow::resizeEvent(QResizeEvent *event){QMainWindow::resizeEvent(event);adaptSidebar();}
void MainWindow::adaptSidebar(){
    if(!centralWidget()||!m_nav)return;auto sidebar=centralWidget()->findChild<QFrame*>("sidebar");if(!sidebar)return;
    const bool compact=width()<1100;if(sidebar->property("compact").isValid()&&sidebar->property("compact").toBool()==compact)return;sidebar->setProperty("compact",compact);sidebar->setFixedWidth(compact?76:244);sidebar->layout()->setContentsMargins(compact?8:14,16,compact?8:14,12);
    for(auto name:{"brand","sidebarVersion","accountName","accountType","accountChevron"})if(auto w=sidebar->findChild<QWidget*>(name))w->setVisible(!compact);
    m_accountButton->layout()->setContentsMargins(compact?4:10,8,compact?4:12,8);
    for(int n=0;n<m_nav->count();++n){auto item=m_nav->item(n);item->setText(compact?QString():"  "+item->toolTip());item->setData(Qt::AccessibleTextRole,item->toolTip());}
    m_nav->setStyleSheet(compact?"QListWidget{padding:0;} QListWidget::item{padding:7px 4px;margin:1px 0px;}":QString());
    m_navSettings->setText(compact?QString():"  "+k("Settings"));m_navSettings->setToolTip(k("Settings"));m_navSettings->setAccessibleName(k("Settings"));m_navSettings->setStyleSheet(compact?"padding:8px 4px;":QString());
    setLauncherUpdate(m_launcherUpdate);
}
void MainWindow::error(const QString &e){QMessageBox::warning(this,"EBALIA",Language::message(e));}
void MainWindow::refreshAccounts(){
    const auto active=m_accounts->active();const bool none=active.uuid.isEmpty();
    const auto type=[](const auto &a){return a.type=="msa"?QString("Microsoft"):k("Local profile");};
    if(m_accountName){m_accountName->setText(none?k("No account"):active.name);m_accountType->setText(none?k("Add account"):type(active));m_accountAvatar->setPixmap(none?Ui::avatar(QString(),40):m_accounts->picture(active,40));m_accountButton->setAccessibleName(m_accountName->text());m_accountButton->setToolTip(m_accountName->text());}
    if(m_playerName){m_playerName->setText(none?k("No account"):active.name);m_playerType->setText(none?k("Local profile"):type(active));}
    if(m_playerAvatar)m_playerAvatar->setPixmap(none?Ui::avatar(QString(),44):m_accounts->picture(active,44));
    if(m_accountList){
        m_accountList->clear();for(const auto &a:m_accounts->accounts()){auto item=new QListWidgetItem(QIcon(m_accounts->picture(a,30)),a.name+"\n"+type(a),m_accountList);item->setData(Qt::UserRole,a.uuid);if(a.uuid==active.uuid)m_accountList->setCurrentItem(item);}
        if(!m_accountList->count()){auto item=new QListWidgetItem(QIcon(Ui::avatar({},30)),k("No account"),m_accountList);item->setFlags(Qt::ItemIsEnabled);}
        m_accountList->setFixedHeight(qMin(m_accountList->count(),3)*62+16);
    }
}
void MainWindow::accountMenu(){
    QMenu menu(this);menu.setObjectName("accountMenu");const auto active=m_accounts->active().uuid;
    for(const auto &a:m_accounts->accounts()){auto action=menu.addAction(QIcon(m_accounts->picture(a,24)),a.name+"   ·   "+(a.type=="msa"?QString("Microsoft"):k("Local profile")));action->setCheckable(true);action->setChecked(a.uuid==active);connect(action,&QAction::triggered,this,[this,uuid=a.uuid]{m_accounts->setActive(uuid);});}
    if(!m_accounts->accounts().isEmpty())menu.addSeparator();
    menuAction(menu,"plus",k("Add Microsoft account"),this,[this]{account(true);});
    menuAction(menu,"user-round",k("Add local profile"),this,[this]{account(false);});
    if(!active.isEmpty()){
        menuAction(menu,"image",text("Cambiar imagen de perfil…","Change profile picture…","Mudar imagem de perfil…"),this,[this,active]{auto picture=InstanceIcons::chooseImage(this,256);if(!picture.isNull())m_accounts->setCustomPicture(active,picture);});
        if(m_accounts->hasCustomPicture(active))menuAction(menu,"refresh-cw",m_accounts->active().type=="msa"?text("Volver a la imagen de Microsoft","Back to the Microsoft picture","Voltar à imagem da Microsoft"):text("Quitar imagen de perfil","Remove profile picture","Remover imagem de perfil"),this,[this,active]{m_accounts->clearCustomPicture(active);});
    }
    if(!active.isEmpty())menuAction(menu,"trash-2",k("Remove active account"),this,[this]{auto a=m_accounts->active();if(QMessageBox::question(this,"EBALIA",text("¿Quitar la cuenta del launcher?","Remove this account from the launcher?","Remover esta conta do launcher?"))==QMessageBox::Yes)m_accounts->removeAccount(a.uuid);});
    menu.addSeparator();menuAction(menu,"settings",k("Settings"),this,[this]{showPage(Settings);});
    menu.setMinimumWidth(m_accountButton->width());menu.exec(m_accountButton->mapToGlobal(QPoint(0,m_accountButton->height()+4)));
}
void MainWindow::instancePickerMenu(){
    QMenu menu(this);menu.setObjectName("instancePickerMenu");auto all=m_mc->instances();
    std::stable_sort(all.begin(),all.end(),[](const McInstance &a,const McInstance &b){return a.lastPlayed>b.lastPlayed;});
    for(const auto &i:all){
        QString icon;try{icon=ModRepository::read(i.dir+"/instance.json")["icon"].toString();}catch(...){}
        auto action=menu.addAction(InstanceIcons::icon(icon,i.dir),i.name+"     "+InstanceText::loader(i.loader)+" "+i.mcVersion);action->setCheckable(true);action->setChecked(i.dir==m_selectedDir);
        connect(action,&QAction::triggered,this,[this,dir=i.dir]{m_selectedDir=dir;refreshInstances();});
    }
    if(!all.isEmpty())menu.addSeparator();
    menuAction(menu,"plus",k("New instance"),this,[this]{createInstance();});menuAction(menu,"layout-grid",k("Instances"),this,[this]{showPage(Instances);});
    menu.setMinimumWidth(m_instancePicker->width());const auto height=menu.sizeHint().height();
    menu.exec(m_instancePicker->mapToGlobal(QPoint(0,-height-6))); // opens upwards, like the Minecraft Launcher
}
McInstance MainWindow::selected() const {if(m_selectedDir.isEmpty())return {};for(const auto &i:m_mc->instances())if(i.dir==m_selectedDir)return i;return {};}
void MainWindow::refreshInstances(){
    auto target=m_target->currentData().toString();m_target->clear();
    auto all=m_mc->instances();std::stable_sort(all.begin(),all.end(),[](const McInstance &a,const McInstance &b){return a.lastPlayed>b.lastPlayed;}); // most recently played first, like the Minecraft Launcher
    if(std::none_of(all.begin(),all.end(),[this](const McInstance &i){return i.dir==m_selectedDir;}))m_selectedDir=all.isEmpty()?QString():all.first().dir;
    QList<InstanceInfo> infos;QStringList used;InstanceInfo current;
    for(const auto &i:all){
        QJsonObject meta;try{meta=ModRepository::read(i.dir+"/instance.json");}catch(...){}
        InstanceInfo info{i,meta["group"].toString().trimmed(),meta["icon"].toString(),meta["loaderVersion"].toString(),!meta["packProvider"].toString().isEmpty(),m_mc->isRunning(i.dir),m_installing.contains(i.dir)||m_mc->isInstalling(i.dir)};
        infos<<info;used<<info.group;if(i.dir==m_selectedDir)current=info;
        if(i.loader!="vanilla")m_target->addItem(InstanceIcons::icon(info.icon,i.dir),i.name+"  ·  "+InstanceText::loader(i.loader)+" "+i.mcVersion,i.dir);
    }
    QStringList emptyGroups;for(const auto &g:QSettings().value("ui/groups").toStringList())if(!used.contains(g))emptyGroups<<g;
    m_grid->setInstances(infos,emptyGroups,m_selectedDir);
    if(m_library->currentWidget()==m_detail){if(current.base.dir.isEmpty())m_library->setCurrentWidget(m_grid);else m_detail->showInstance(current);}
    int idx=m_target->findData(target);if(idx>=0)m_target->setCurrentIndex(idx);
    if(m_pickerIcon){
        if(current.base.dir.isEmpty()){m_pickerIcon->setPixmap(Ui::pixmap("plus",26,Ui::accent()));m_pickerName->setText(k("Create my first instance"));m_pickerSub->setText("Vanilla · Fabric · Forge · NeoForge");}
        else{m_pickerIcon->setPixmap(InstanceIcons::icon(current.icon,current.base.dir).pixmap(36,36));m_pickerName->setText(current.base.name);m_pickerSub->setText(InstanceText::loader(current.base.loader)+" "+current.base.mcVersion);}
        m_instancePicker->setAccessibleName(m_pickerName->text());m_instancePicker->setToolTip(m_pickerName->text()+"\n"+m_pickerSub->text());
    }
    selection();
}
void MainWindow::selection(){
    auto i=selected();const bool running=m_mc->isRunning(i.dir),busy=!i.dir.isEmpty()&&(m_installing.contains(i.dir)||m_mc->isInstalling(i.dir));
    const QColor white(Qt::white);
    if(running){m_playButton->setText("  "+k("Stop").toUpper());m_playButton->setIcon(Ui::icon("square",white));}
    else if(busy){m_playButton->setText(k("Installing…"));m_playButton->setIcon({});}
    else{m_playButton->setText("  "+k("Play").toUpper());m_playButton->setIcon(Ui::icon("play",white));}
    m_playButton->setEnabled(!busy);m_playButton->setProperty("danger",running);m_playButton->setProperty("play",!running);m_playButton->style()->unpolish(m_playButton);m_playButton->style()->polish(m_playButton);
}
void MainWindow::refreshPacks(){
    m_packs->clear();
    try{m_packData=ModRepository(m_root).packs();for(const auto &v:m_packData){auto p=v.toObject();m_packs->addItem(new QListWidgetItem(Ui::icon("package",Ui::accent()),p["name"].toString()+"\n"+QString::number(p["projects"].toArray().size())+" mods"));}
        if(m_packs->count())m_packs->setCurrentRow(0);else m_packDetails->setText(text("Todavía no hay packs. Guardá los mods de una instancia para empezar.","No packs yet. Save an instance's mods to get started.","Ainda não há packs. Salve os mods de uma instância para começar."));}
    catch(...){m_packDetails->setText(exception());}
}
void MainWindow::refreshLost(){
    auto keep=filename(m_lost);QSignalBlocker block(m_lost);m_lost->clear();
    for(const auto &v:m_versions->getVersions()){
        if(!(v.name+" "+v.category+" "+lostCategory(v.category)).contains(m_lostFilter->text().trimmed(),Qt::CaseInsensitive))continue;
        auto item=new QListWidgetItem(m_lost);item->setData(Qt::UserRole,v.id);item->setToolTip(v.name);item->setData(Qt::AccessibleTextRole,v.name);
        item->setData(Qt::UserRole+1,QJsonObject{{"name",v.name},{"category",v.category},{"art",Ui::lostArtFor(v.id,v.category)},
            {"label",lostCategory(v.category)+(m_versions->isVersionInstalled(v)?" · "+text("Instalada","Installed","Instalada"):QString())}});
        if(v.id==keep)m_lost->setCurrentItem(item);
    }
    if(m_lost->currentRow()<0&&m_lost->count())m_lost->setCurrentRow(0);block.unblock();lostSelection();
}
QString MainWindow::lostCategory(const QString &category){return category=="horror"?Language::key("Horror"):category=="release"?Language::key("Release"):category=="alpha"?Language::key("Alpha"):category;}
void MainWindow::lostSelection(){
    auto id=filename(m_lost);VersionInfo v;for(const auto &entry:m_versions->getVersions())if(entry.id==id)v=entry;
    auto cover=static_cast<Cover*>(m_lostImage);cover->setArtwork(Ui::lostArtFor(v.id,v.category));
    const bool running=m_java->isRunning();
    m_lostUninstall->setVisible(!v.id.isEmpty()&&m_versions->isVersionInstalled(v));m_lostUninstall->setEnabled(!running&&!m_jobs);
    m_lostPlay->setText("  "+(running?k("Stop"):k("Play")).toUpper());m_lostPlay->setIcon(Ui::icon(running?"square":"play",Qt::white));m_lostPlay->setProperty("danger",running);m_lostPlay->setProperty("play",!running);m_lostPlay->style()->unpolish(m_lostPlay);m_lostPlay->style()->polish(m_lostPlay);
    m_lostPlay->setEnabled(!v.id.isEmpty());m_lostInstall->setEnabled(!v.id.isEmpty());if(v.id.isEmpty()){m_lostTitle->setText(Language::key("No versions match this search."));m_lostInfo->clear();m_lostInstall->setText("  "+text("Instalar","Install","Instalar").toUpper());return;}
    const bool installed=m_versions->isVersionInstalled(v);m_lostTitle->setText(v.name);
    QStringList info{lostCategory(v.category)+"  ·  "+v.description,installed?text("Instalada","Installed","Instalada"):text("Pendiente de instalación","Not installed yet","Ainda não instalada")};
    if(!LostNative::available(v.native)&&LostInstaller::windowsPackage(v))info<<Language::key("Original Windows package. Runs with Java 8, which EBALIA prepares automatically. On Linux and macOS it runs in Wine with its own prefix.");
    else info<<Language::key("Runs with Java 8, which EBALIA prepares automatically.");
    if(!installed&&v.archiveSize>0)info<<Language::key("The first installation downloads the original archive (%1 MB); later versions reuse it.").arg(v.archiveSize/1024/1024);
    m_lostInfo->setText(info.join("\n\n"));m_lostInstall->setText("  "+(installed?Language::key("Reinstall"):text("Instalar","Install","Instalar")).toUpper());m_lostPlay->setEnabled(installed||running);
}
void MainWindow::work(const QString &title,std::function<QJsonObject()> job,std::function<void(QJsonObject)> done){
    if(m_jobs)return;++m_jobs;lostSelection();
    auto task=new QWidget(this);task->setObjectName("backgroundTaskProgress");task->setMaximumWidth(300);
    auto layout=new QVBoxLayout(task);layout->setContentsMargins(8,4,8,4);layout->setSpacing(4);
    auto caption=new QLabel(title,task);caption->setWordWrap(true);caption->setTextFormat(Qt::PlainText);caption->setToolTip(title);layout->addWidget(caption);
    auto progress=new QProgressBar(task);progress->setObjectName("backgroundTaskBar");progress->setRange(0,0);progress->setTextVisible(false);progress->setFixedHeight(8);layout->addWidget(progress);
    statusBar()->addPermanentWidget(task);task->show();
    auto watcher=new QFutureWatcher<QJsonObject>(this);connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,task,watcher,done]{auto result=watcher->result();watcher->deleteLater();statusBar()->removeWidget(task);task->deleteLater();--m_jobs;lostSelection();if(result.contains("_error"))error(result["_error"].toString());else done(result);});
    watcher->setFuture(QtConcurrent::run([job]{try{return job();}catch(...){return QJsonObject{{"_error",exception()}};}}));
}
void MainWindow::createInstance(bool copy,int page){
    auto source=selected();if(copy&&source.dir.isEmpty())return;
    if(copy&&(m_mc->isRunning(source.dir)||m_installing.contains(source.dir))){error(text("Cerrá el juego antes de copiar sus mods.","Close the game before copying its mods.","Feche o jogo antes de copiar seus mods."));return;}
    if(m_manifest.isEmpty())m_mc->fetchManifest(); // the dialog fills the version list when the catalog arrives; imports do not need it
    CreateInstanceDialog dialog(m_mc,m_manifest,m_packData,source,copy,this);if(page)dialog.showPage(page);Ui::fitToScreen(&dialog,{1060,760});
    if(Ui::openWindow(dialog)!=QDialog::Accepted)return;auto c=dialog.configuration();
    auto reveal=[this](const QString &dir){m_selectedDir=dir;showPage(Instances);m_library->setCurrentWidget(m_detail);refreshInstances();};
    if(!c["providerPack"].toObject().isEmpty()){auto root=m_root;work(Language::key("Installing modpack…"),[root,c]{auto dir=PackService(root).install(c["providerPack"].toObject(),c["providerVersion"].toObject(),c["name"].toString(),c["group"].toString(),c["xmx"].toInt(4096));auto info=ModRepository::read(dir+"/instance.json");if(info["icon"].toString()!="custom"){info["icon"]=c["icon"];ModRepository::write(dir+"/instance.json",info);}return QJsonObject{{"dir",dir}};},[this,reveal,custom=c["icon"].toString()=="custom"?dialog.customIcon():QImage()](QJsonObject result){if(!custom.isNull())setInstanceIcon(result["dir"].toString(),"custom",custom);reveal(result["dir"].toString());showImportWarnings(result["dir"].toString());m_status->setText(Language::key("Modpack ready. Press Install to prepare Minecraft."));});return;}
    try{auto dir=m_mc->createInstance(c["name"].toString(),c["mcVersion"].toString(),c["loader"].toString(),c["loaderVersion"].toString());auto info=ModRepository::read(dir+"/instance.json");for(auto key:{"xmx","group","icon"})info[key]=c[key];ModRepository::write(dir+"/instance.json",info);
        if(c["icon"].toString()=="custom")setInstanceIcon(dir,"custom",dialog.customIcon());
        reveal(dir);
        if(copy){auto root=m_root;work(text("Identificando los mods…","Identifying mods…","Identificando mods…"),[root,source]{return ModRepository(root).capture(source.dir,source.name);},[this,dir](QJsonObject p){preview(p["projects"].toArray(),dir);});}
        else if(!c["pack"].toObject().isEmpty())preview(c["pack"].toObject()["projects"].toArray(),dir);
    }catch(...){error(exception());}
}
// Built-in icon key, or "custom" with the picture saved as instance-icon.png.
void MainWindow::setInstanceIcon(const QString &dir,const QString &key,const QImage &custom){
    if(dir.isEmpty())return;
    auto icon=key;
    if(key=="custom"&&!InstanceIcons::saveCustom(dir,custom)){error(text("No se pudo guardar el icono.","Could not save the icon.","Não foi possível salvar o ícone."));icon="grass";}
    try{auto info=ModRepository::read(dir+"/instance.json");info["icon"]=icon;ModRepository::write(dir+"/instance.json",info);}catch(...){error(exception());}
}
void MainWindow::showImportWarnings(const QString &dir){
    // The pack keeps its Minecraft version and loader; mods that do not fit them are named before the first start.
    QJsonObject info;try{info=ModRepository::read(dir+"/instance.json");}catch(...){return;}
    const auto warnings=info["compatibilityWarnings"].toArray();if(warnings.isEmpty())return;
    QStringList lines;for(const auto &w:warnings)lines<<"• "+w.toString();
    QMessageBox box(QMessageBox::Warning,"EBALIA",text("El pack se importó para Minecraft %1 con %2, pero estos mods no coinciden:","The pack was imported for Minecraft %1 with %2, but these mods do not match:","O pack foi importado para Minecraft %1 com %2, mas estes mods não combinam:").arg(info["mcVersion"].toString(),InstanceText::loader(info["loader"].toString())),QMessageBox::Ok,this);
    box.setInformativeText(lines.join("\n")+"\n\n"+text("Puede que el juego no arranque hasta que los quites o los cambies por la versión correcta en Administrar mods.","The game may not start until you remove them or replace them with the right version in Manage mods.","O jogo pode não abrir até você removê-los ou trocá-los pela versão certa em Gerenciar mods."));
    box.exec();
}
void MainWindow::adoptPastedInstances(){
    QStringList pasted;for(const auto &p:PackService::pastedEntries(m_mc->instancesRoot()))if(!m_pasteFailed.contains(p))pasted<<p;
    if(pasted.isEmpty()){m_pasteSnapshot.clear();return;}
    // Explorer may still be copying: wait until the files stop changing.
    QString snapshot;for(const auto &p:pasted){qint64 size=0,count=0;QDirIterator it(p,QDir::Files|QDir::Hidden,QDirIterator::Subdirectories);while(it.hasNext()){it.next();size+=it.fileInfo().size();++count;}if(QFileInfo(p).isFile())size=QFileInfo(p).size();snapshot+=p+":"+QString::number(size)+":"+QString::number(count)+";";}
    if(snapshot!=m_pasteSnapshot||m_jobs){m_pasteSnapshot=snapshot;m_pasteTimer->start();return;}
    m_pasteSnapshot.clear();auto root=m_root;
    work(text("Importando instancias pegadas…","Importing pasted instances…","Importando instâncias coladas…"),[root,pasted]{
        QJsonArray done,failed;
        for(const auto &p:pasted){try{done.append(PackService(root).adopt(p));}catch(const std::exception &e){failed.append(QJsonObject{{"path",p},{"error",QString::fromUtf8(e.what())}});}}
        return QJsonObject{{"done",done},{"failed",failed}};
    },[this](QJsonObject r){
        refreshInstances();QStringList names;for(const auto &d:r["done"].toArray()){names<<QFileInfo(d.toString()).fileName();showImportWarnings(d.toString());}
        if(!names.isEmpty())m_status->setText(text("Instancias importadas desde la carpeta: %1","Instances imported from the folder: %1","Instâncias importadas da pasta: %1").arg(names.join(", ")));
        QStringList problems;for(const auto &f:r["failed"].toArray()){const auto o=f.toObject();m_pasteFailed.insert(o["path"].toString());problems<<"• "+QFileInfo(o["path"].toString()).fileName()+": "+Language::message(o["error"].toString());}
        if(!problems.isEmpty())QMessageBox::warning(this,"EBALIA",text("Estas carpetas de la carpeta de instancias no se pudieron convertir en instancias (quedaron sin cambios):","These items in the instances folder could not become instances (they were left unchanged):","Estes itens da pasta de instâncias não puderam virar instâncias (ficaram sem alterações):")+"\n\n"+problems.join("\n"));
    });
}
void MainWindow::useSoftwareRendering(const QString &dir){
    try{auto info=ModRepository::read(dir+"/instance.json");info["softwareRendering"]=true;ModRepository::write(dir+"/instance.json",info);}catch(...){error(exception());return;}
    for(const auto &i:m_mc->instances())if(i.dir==dir){m_status->setText(text("Iniciando con gráficos por software…","Starting with software graphics…","Iniciando com gráficos por software…"));launchInstance(i);break;}
}
void MainWindow::installDependency(const QString &dir,const QString &mod,std::function<void(bool)> done){
    QJsonObject info;try{info=ModRepository::read(dir+"/instance.json");}catch(...){error(exception());done(false);return;}
    const auto game=info["mcVersion"].toString(),loader=info["loader"].toString();auto root=m_root;
    work(text("Instalando %1…","Installing %1…","Instalando %1…").arg(mod),[root,dir,mod,game,loader]{
        ModRepository repo(root);
        // The mod id is usually the Modrinth project; otherwise the search result with the same name.
        auto normal=[](QString s){return s.toLower().remove(QRegularExpression("[^a-z0-9]"));};
        QJsonObject plan;
        try{plan=repo.plan(QJsonArray{QJsonObject{{"project_id",mod},{"name",mod}}},game,loader);}catch(...){}
        if(plan["versions"].toArray().isEmpty()){
            QString project;
            for(const auto &h:repo.search(mod,game,loader)){auto o=h.toObject();if(normal(o["slug"].toString())==normal(mod)||normal(o["title"].toString())==normal(mod)){project=o["project_id"].toString();break;}}
            if(project.isEmpty())for(const auto &h:repo.search(mod,game,loader)){auto o=h.toObject();if(normal(o["slug"].toString()).startsWith(normal(mod))||normal(o["title"].toString()).startsWith(normal(mod))){project=o["project_id"].toString();break;}}
            if(project.isEmpty())throw std::runtime_error(Language::text("No encontramos %1 en Modrinth para Minecraft %2 con %3.","%1 was not found on Modrinth for Minecraft %2 with %3.","%1 não foi encontrado no Modrinth para Minecraft %2 com %3.").arg(mod,game,loader).toStdString());
            plan=repo.plan(QJsonArray{QJsonObject{{"project_id",project},{"name",mod}}},game,loader);
        }
        if(plan["versions"].toArray().isEmpty())throw std::runtime_error(Language::text("%1 no tiene una versión para Minecraft %2 con %3.","%1 has no version for Minecraft %2 with %3.","%1 não tem versão para Minecraft %2 com %3.").arg(mod,game,loader).toStdString());
        repo.apply(dir,plan);return QJsonObject{{"ok",true}};
    },[done](QJsonObject r){done(r["ok"].toBool());});
}
// "[26.3.0.0.6,)" → "version 26.3.0.0.6 or later"; Fabric's own wording is kept.
static QString readableRange(const QString &range){
    static const QRegularExpression maven(R"(^\s*([\[\(])\s*([^,\]\)]*)\s*(,\s*([^\]\)]*))?\s*([\]\)])\s*$)");auto m=maven.match(range);if(!m.hasMatch())return range.trimmed();
    const auto low=m.captured(2).trimmed(),high=m.captured(4).trimmed();
    if(m.captured(3).isEmpty())return Language::text("versión %1","version %1","versão %1").arg(low);
    if(high.isEmpty())return Language::text("versión %1 o mayor","version %1 or later","versão %1 ou maior").arg(low);
    if(low.isEmpty())return Language::text("versión anterior a %1","version before %1","versão anterior a %1").arg(high);
    return Language::text("versión %1 a %2","version %1 to %2","versão %1 a %2").arg(low,high);
}
void MainWindow::showCrash(const QString &dir){
    QString log;{QFile f(dir+"/launcher.log");if(f.open(QIODevice::ReadOnly)){if(f.size()>4*1024*1024)f.seek(f.size()-4*1024*1024);log=QString::fromUtf8(f.readAll());}}
    const auto findings=CrashReport::analyze(log);QJsonObject info;try{info=ModRepository::read(dir+"/instance.json");}catch(...){}
    const auto instanceName=info["name"].toString();
    QDialog d(this);d.setObjectName("crashSummaryWindow");d.setWindowTitle(text("Minecraft se cerró","Minecraft closed","Minecraft fechou")+" · "+instanceName);Ui::fitToScreen(&d,{760,460});
    QVBoxLayout lay(&d);lay.setContentsMargins(22,20,22,18);lay.setSpacing(12);
    label(findings.isEmpty()?text("Minecraft se cerró con un error","Minecraft closed with an error","Minecraft fechou com um erro"):text("Minecraft se cerró. Esto es lo que falló:","Minecraft closed. This is what failed:","Minecraft fechou. Isto é o que falhou:"),&lay,"sectionTitle");
    if(findings.isEmpty())label(text("No reconocimos la causa. El registro tiene los detalles; si pedís ayuda, compartilo.","The cause was not recognised. The log has the details; share it if you ask for help.","A causa não foi reconhecida. O registro tem os detalhes; compartilhe-o se pedir ajuda."),&lay,"muted");
    auto replay=[this,dir,&d]{d.accept();for(const auto &i:m_mc->instances())if(i.dir==dir){launchInstance(i);break;}};
    for(const auto &f:findings){
        auto card=new QFrame;card->setObjectName("crashFinding");card->setStyleSheet("#crashFinding{background:#1f1f24;border:1px solid #34343c;border-radius:12px;}");
        auto cl=new QVBoxLayout(card);cl->setContentsMargins(16,12,16,12);cl->setSpacing(6);lay.addWidget(card);
        QString title,body;auto actionsRow=new Ui::ResponsiveRow(700);auto actions=actionsRow->box();actions->setSpacing(8);
        switch(f.kind){
        case CrashReport::Finding::MissingMod:{
            title=text("Falta el mod %1","The mod %1 is missing","Falta o mod %1").arg(f.mod);
            body=text("%1 lo necesita para funcionar%2.","%1 needs it to work%2.","%1 precisa dele para funcionar%2.").arg(f.requiredBy,readableRange(f.versions).isEmpty()?QString():" ("+readableRange(f.versions)+")");
            auto install=button(text("Instalar %1","Install %1","Instalar %1").arg(f.mod),actions,{},&d,true,"download");install->setObjectName("installMissingMod");
            connect(install,&QPushButton::clicked,&d,[this,dir,mod=f.mod,install,replay]{install->setEnabled(false);install->setText(text("Instalando…","Installing…","Instalando…"));
                installDependency(dir,mod,[install,replay](bool ok){if(!ok){install->setEnabled(true);install->setText(text("Reintentar","Retry","Tentar de novo"));return;}
                    install->setText(text("Instalado · Jugar","Installed · Play","Instalado · Jogar"));install->setEnabled(true);QObject::disconnect(install,nullptr,nullptr,nullptr);QObject::connect(install,&QPushButton::clicked,install,replay);});});
            break;}
        case CrashReport::Finding::WrongModVersion:
            title=text("%1 no tiene la versión que pide %2","%1 is not the version %2 needs","%1 não tem a versão que %2 pede").arg(f.mod,f.requiredBy);
            body=text("%1 necesita %2 (%3). Actualizá o quitá uno de los dos.","%1 needs %2 (%3). Update or remove one of them.","%1 precisa de %2 (%3). Atualize ou remova um deles.").arg(f.requiredBy,f.mod,readableRange(f.versions));
            button(text("Administrar mods","Manage mods","Gerenciar mods"),actions,[this,dir,&d]{d.accept();m_selectedDir=dir;manageMods();},&d,false,"puzzle");break;
        case CrashReport::Finding::IncompatibleMods:
            title=text("Mods incompatibles","Incompatible mods","Mods incompatíveis");body=f.detail;
            button(text("Administrar mods","Manage mods","Gerenciar mods"),actions,[this,dir,&d]{d.accept();m_selectedDir=dir;manageMods();},&d,false,"puzzle");break;
        case CrashReport::Finding::GraphicsDriver:
        case CrashReport::Finding::NoOpenGL:{
            const bool crash=f.kind==CrashReport::Finding::GraphicsDriver;
            const auto gpus=GpuInfo::detect();const auto gpu=GpuInfo::pick(gpus,f.vendor);
            const auto card=gpu.name.isEmpty()?QString():gpu.name+(gpu.integrated?text(" (integrada en el procesador)"," (built into the processor)"," (integrada no processador)"):QString());
            if(crash){
                title=text("Falló el driver de gráficos %1","The %1 graphics driver crashed","O driver de vídeo %1 falhou").arg(f.vendor);
                body=text("El juego se cerró dentro del driver de tu tarjeta (no es un problema de Minecraft ni del launcher). Instalá el driver más reciente y reiniciá el equipo.","The game closed inside your graphics driver (not a Minecraft or launcher problem). Install the latest driver and restart the computer.","O jogo fechou dentro do driver da sua placa (não é um problema do Minecraft nem do launcher). Instale o driver mais recente e reinicie o computador.");
            }else if(gpu.virtualAdapter||gpus.isEmpty()){
                title=text("Tu tarjeta gráfica no ofrece OpenGL","Your graphics card does not provide OpenGL","Sua placa de vídeo não oferece OpenGL");
                body=text("Parece una máquina virtual o un equipo sin driver de gráficos (%1). Los gráficos por software funcionan en cualquier equipo, pero van más lentos.","This looks like a virtual machine or a computer without a graphics driver (%1). Software graphics work on any computer but run slower.","Parece uma máquina virtual ou um computador sem driver de vídeo (%1). Os gráficos por software funcionam em qualquer computador, mas são mais lentos.").arg(gpu.name.isEmpty()?text("sin tarjeta detectada","no card detected","nenhuma placa detectada"):gpu.name);
            }else{
                title=text("Tu tarjeta gráfica no ofrece OpenGL","Your graphics card does not provide OpenGL","Sua placa de vídeo não oferece OpenGL");
                body=text("Minecraft lo necesita y llega con el driver de tu tarjeta. Instalá el driver más reciente; mientras tanto podés jugar con gráficos por software (más lento).","Minecraft needs it and it comes with your card's driver. Install the latest driver; meanwhile you can play with software graphics (slower).","O Minecraft precisa dele e ele vem com o driver da sua placa. Instale o driver mais recente; enquanto isso, você pode jogar com gráficos por software (mais lento).");
            }
            if(!card.isEmpty()&&!gpu.virtualAdapter)body+="\n"+text("Tu tarjeta: %1","Your graphics card: %1","Sua placa: %1").arg(card);
            const bool linuxMesa=QSysInfo::productType()!="windows"&&QSysInfo::productType()!="macos"&&gpu.vendor!="NVIDIA"&&!gpu.vendor.isEmpty();
            if(linuxMesa)body+="\n"+text("En Linux el driver de %1 llega con las actualizaciones del sistema (Mesa): actualizá tu distribución.","On Linux the %1 driver comes with system updates (Mesa): update your distribution.","No Linux o driver %1 vem com as atualizações do sistema (Mesa): atualize sua distribuição.").arg(gpu.vendor);
            if(!gpu.vendor.isEmpty()&&!gpu.virtualAdapter&&!linuxMesa){
                // The exact driver page of this card, found when pressed (NVIDIA and AMD are asked online); the maker's detection tool otherwise.
                auto driver=button(text("Descargar driver para %1","Download the driver for %1","Baixar driver para %1").arg(gpu.name),actions,{},&d,true,"download");driver->setObjectName("downloadGraphicsDriver");
                connect(driver,&QPushButton::clicked,&d,[driver,gpu]{
                    driver->setEnabled(false);const auto label=driver->text();driver->setText(" "+text("Buscando el driver de tu tarjeta…","Looking for your card's driver…","Procurando o driver da sua placa…"));
                    auto watcher=new QFutureWatcher<GpuInfo::DriverLink>(driver);
                    QObject::connect(watcher,&QFutureWatcher<GpuInfo::DriverLink>::finished,driver,[driver,watcher,label,gpu]{
                        auto link=watcher->result();watcher->deleteLater();if(link.url.isEmpty())link=GpuInfo::autoDetect(gpu.vendor);
                        driver->setEnabled(true);driver->setText(label);driver->setToolTip(link.title);if(!link.url.isEmpty())QDesktopServices::openUrl(QUrl(link.url));
                    });
                    watcher->setFuture(QtConcurrent::run([gpu]{try{return GpuInfo::driverLink(gpu,[](const QUrl &url){return ModRepository::fetch(url);});}catch(...){return GpuInfo::DriverLink{};}}));
                });
                // Laptops and integrated graphics: the computer maker may ship its own customized driver.
                const auto maker=GpuInfo::manufacturerSupport(GpuInfo::computer());
                if(!maker.url.isEmpty())button(maker.title,actions,[url=maker.url]{QDesktopServices::openUrl(QUrl(url));},&d,false,"external-link");
            }
            if(SoftwareGl::available()&&!info["softwareRendering"].toBool())button(crash?text("Probar con gráficos por software","Try software graphics","Tentar gráficos por software"):text("Usar gráficos por software y jugar","Use software graphics and play","Usar gráficos por software e jogar"),actions,[this,dir,&d]{d.accept();useSoftwareRendering(dir);},&d,!crash&&(gpu.virtualAdapter||gpus.isEmpty()),"play");
            break;}
        case CrashReport::Finding::JavaVersion:
            title=text("Hace falta Java %1","Java %1 is needed","É preciso Java %1").arg(f.versions);
            body=text("Un mod o el juego necesita Java %1 o más nuevo. Elegí \"Automático\" en los ajustes de la instancia para que el launcher use el correcto.","A mod or the game needs Java %1 or newer. Choose \"Automatic\" in the instance settings so the launcher uses the right one.","Um mod ou o jogo precisa do Java %1 ou mais novo. Escolha \"Automático\" nas configurações da instância para o launcher usar o correto.").arg(f.versions);
            button(Language::key("Instance settings"),actions,[this,dir,&d]{d.accept();m_selectedDir=dir;editInstance();},&d,false,"settings");break;
        case CrashReport::Finding::OutOfMemory:
            title=text("Minecraft se quedó sin memoria","Minecraft ran out of memory","O Minecraft ficou sem memória");
            body=text("Tiene %1 MB. Subí la memoria en los ajustes de la instancia (6144 MB o más para packs grandes).","It has %1 MB. Raise the memory in the instance settings (6144 MB or more for large packs).","Ele tem %1 MB. Aumente a memória nas configurações da instância (6144 MB ou mais para packs grandes).").arg(info["xmx"].toInt(4096));
            button(Language::key("Instance settings"),actions,[this,dir,&d]{d.accept();m_selectedDir=dir;editInstance();},&d,false,"settings");break;
        }
        auto t=label(title,cl,"",true);t->setStyleSheet("font-weight:800;font-size:15px;");label(body,cl,"muted");if(actions->direction()==QBoxLayout::LeftToRight)actions->addStretch();cl->addWidget(actionsRow);
    }
    lay.addStretch();
    auto bottom=new QHBoxLayout;lay.addLayout(bottom);
    button(text("Ver registro completo","View full log","Ver registro completo"),bottom,[this,dir,&d]{showLog(dir,&d);},&d,false,"file-text");bottom->addStretch();
    button(text("Cerrar","Close","Fechar"),bottom,[&d]{d.reject();},&d);
    Ui::openWindow(d);
}
void MainWindow::changeBackground(const QString &dir){
    if(dir.isEmpty())return;QJsonObject info;try{info=ModRepository::read(dir+"/instance.json");}catch(...){error(exception());return;}
    QString key;QImage custom;if(!InstanceIcons::chooseBackground(this,dir,info["background"].toString(),key,custom))return;
    if(key=="custom"&&!custom.isNull()&&!InstanceIcons::saveBackground(dir,custom)){error(text("No se pudo guardar la imagen.","Could not save the picture.","Não foi possível salvar a imagem."));return;}
    try{info=ModRepository::read(dir+"/instance.json");info["background"]=key;ModRepository::write(dir+"/instance.json",info);}catch(...){error(exception());}
    refreshInstances();
}
void MainWindow::instanceMenu(){
    auto i=selected();if(i.dir.isEmpty())return;QMenu menu(this);
    const bool running=m_mc->isRunning(i.dir);
    menuAction(menu,running?"square":"play",running?Language::key("Stop"):i.ready?Language::key("Play"):Language::key("Install"),this,[this]{play();});
    menuAction(menu,"layout-grid",Language::key("Open"),this,[this]{m_library->setCurrentWidget(m_detail);refreshInstances();});menu.addSeparator();
    menuAction(menu,"settings",Language::key("Instance settings"),this,[this]{editInstance();});
    menuAction(menu,"layers",Language::key("Change group"),this,[this]{changeGroup();});
    menuAction(menu,"folder",Language::key("Open instance folder"),this,[i]{QDesktopServices::openUrl(QUrl::fromLocalFile(i.dir));});
    menuAction(menu,"copy",Language::key("Copy instance"),this,[this]{copyInstance();});
    menuAction(menu,"share-2",Language::key("Export instance"),this,[this]{exportInstance();});menu.addSeparator();
    menuAction(menu,"trash-2",Language::key("Delete instance"),this,[this]{removeInstance();});
    menu.exec(QCursor::pos());
}
void MainWindow::groupMenu(const QString &group){
    QMenu menu(this);auto rename=menu.addAction(Ui::icon("pencil"),Language::key("Rename group"));auto ungroup=menu.addAction(Ui::icon("x"),Language::key("Remove group (keep instances)"));auto chosen=menu.exec(QCursor::pos());if(!chosen)return;Q_UNUSED(ungroup);
    QString name;if(chosen==rename){bool ok;name=QInputDialog::getText(this,Language::key("Rename group"),Language::key("Group name"),QLineEdit::Normal,group,&ok).trimmed();if(!ok||name.isEmpty()||name==group)return;}
    for(const auto &i:m_mc->instances()){try{auto info=ModRepository::read(i.dir+"/instance.json");if(info["group"].toString().trimmed()!=group)continue;info["group"]=name;ModRepository::write(i.dir+"/instance.json",info);}catch(...){error(exception());}}
    auto groups=QSettings().value("ui/groups").toStringList();groups.removeAll(group);if(!name.isEmpty()&&!groups.contains(name))groups<<name;QSettings().setValue("ui/groups",groups);refreshInstances();
}
void MainWindow::changeGroup(){
    auto i=selected();if(i.dir.isEmpty())return;QJsonObject info;try{info=ModRepository::read(i.dir+"/instance.json");}catch(...){error(exception());return;}
    QStringList groups=QSettings().value("ui/groups").toStringList();for(const auto &other:m_mc->instances()){try{auto g=ModRepository::read(other.dir+"/instance.json")["group"].toString().trimmed();if(!g.isEmpty()&&!groups.contains(g))groups<<g;}catch(...){}}
    groups.sort(Qt::CaseInsensitive);groups.prepend(Language::key("No group"));bool ok;auto current=info["group"].toString();
    auto chosen=QInputDialog::getItem(this,Language::key("Change group"),Language::key("Group name"),groups,qMax(0,groups.indexOf(current)),true,&ok).trimmed();if(!ok)return;
    if(chosen==Language::key("No group"))chosen.clear();info["group"]=chosen;try{ModRepository::write(i.dir+"/instance.json",info);}catch(...){error(exception());return;}
    if(!chosen.isEmpty()&&!QSettings().value("ui/groups").toStringList().contains(chosen)){auto all=QSettings().value("ui/groups").toStringList();all<<chosen;QSettings().setValue("ui/groups",all);}
    refreshInstances();
}
void MainWindow::copyInstance(){
    auto i=selected();if(i.dir.isEmpty())return;if(m_mc->isRunning(i.dir)||m_installing.contains(i.dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}
    bool ok;auto name=QInputDialog::getText(this,Language::key("Copy instance"),Language::key("Name of the copy"),QLineEdit::Normal,i.name+" (2)",&ok).trimmed();if(!ok||name.isEmpty())return;
    auto mc=m_mc;auto dir=i.dir;work(Language::key("Copying worlds, mods and settings…"),[mc,dir,name]{return QJsonObject{{"dir",mc->copyInstance(dir,name)}};},[this](QJsonObject r){m_selectedDir=r["dir"].toString();m_library->setCurrentWidget(m_detail);refreshInstances();m_status->setText(Language::key("Instance copied."));});
}
void MainWindow::exportInstance(){
    auto i=selected();if(i.dir.isEmpty())return;if(m_mc->isRunning(i.dir)||m_installing.contains(i.dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}
    auto safe=i.name;safe.replace(QRegularExpression("[^A-Za-z0-9 ._-]"),"_");auto file=QFileDialog::getSaveFileName(this,Language::key("Export instance"),QDir::homePath()+"/"+safe+".zip","ZIP (*.zip)");if(file.isEmpty())return;
    auto mc=m_mc;auto dir=i.dir;work(Language::key("Exporting instance…"),[mc,dir,file]{mc->exportInstance(dir,file);return QJsonObject{};},[this](QJsonObject){m_status->setText(Language::key("Instance exported. Import the ZIP in EBALIA to restore it."));});
}
void MainWindow::showLog(const QString &dir,QWidget *parent){
    if(dir.isEmpty())return;QDialog log(parent);log.setObjectName("logWindow");log.setWindowTitle(text("Registro","Log","Registro"));Ui::fitToScreen(&log,{900,600});QVBoxLayout ll(&log);ll.setContentsMargins(16,16,16,16);QPlainTextEdit view;view.setReadOnly(true);view.setObjectName("logView");
    QFont mono=QFontDatabase::systemFont(QFontDatabase::FixedFont);view.setFont(mono);
    QFile f(dir+"/launcher.log");if(!f.exists())f.setFileName(dir+"/logs/latest.log");
    if(f.open(QIODevice::ReadOnly)){f.seek(qMax<qint64>(0,f.size()-120000));view.setPlainText(QString::fromUtf8(f.readAll()));view.moveCursor(QTextCursor::End);}else view.setPlainText(text("Todavía no hay registro.","No log yet.","Ainda não há registro."));
    ll.addWidget(&view);QDialogButtonBox close(QDialogButtonBox::Close);ll.addWidget(&close);connect(&close,&QDialogButtonBox::rejected,&log,&QDialog::reject);Ui::openWindow(log);
}
void MainWindow::play(){
    auto i=selected();if(i.dir.isEmpty()){createInstance();return;}if(m_installing.contains(i.dir))return;
    if(m_mc->isRunning(i.dir)){if(QMessageBox::question(this,"EBALIA",text("¿Cerrar esta instancia de Minecraft?","Close this Minecraft instance?","Fechar esta instância do Minecraft?"))==QMessageBox::Yes)m_mc->killInstance(i.dir);return;}
    // Like Prism: one press installs what is missing and starts the game; a first game only needs a player name.
    if(m_accounts->active().uuid.isEmpty()){account(false);if(m_accounts->active().uuid.isEmpty())return;}
    if(!i.ready){m_launchAfterInstall.insert(i.dir);m_installing.insert(i.dir);m_mc->installInstance(i.dir);refreshInstances();return;}
    launchInstance(i);
}
void MainWindow::launchInstance(const McInstance &i){
    auto a=m_accounts->active();if(a.uuid.isEmpty())return;
    if(a.type!="msa"){m_mc->launch(i.dir,a.name,a.uuid,{},"legacy");return;}
    auto auth=new MsAuth(this);auto dialog=new QProgressDialog(text("Renovando sesión…","Refreshing session…","Renovando sessão…"),QString(),0,0,this);dialog->setCancelButton(nullptr);dialog->setWindowModality(Qt::ApplicationModal);dialog->show();
    connect(auth,&MsAuth::loginFailed,dialog,[this,auth,dialog](const QString &e){dialog->close();dialog->deleteLater();auth->deleteLater();error(e);});
    connect(auth,&MsAuth::profilePicture,this,[this](const QString &uuid,const QImage &picture){m_accounts->setPicture(uuid,picture);});
    connect(auth,&MsAuth::loginDone,dialog,[this,auth,dialog,i](const QString &token,const QString &refresh,const QString &uuid,const QString &name){m_accounts->addAccount({"msa",name,uuid,token,refresh});dialog->close();dialog->deleteLater();auth->deleteLater();m_mc->launch(i.dir,name,uuid,token,"msa");});auth->refresh(a.refreshToken,!m_accounts->hasPicture(a.uuid));
}
void MainWindow::removeInstance(){
    auto i=selected();if(i.dir.isEmpty())return;
    if(m_mc->isRunning(i.dir)||m_mc->isInstalling(i.dir)||m_installing.contains(i.dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}
    QMessageBox confirm(QMessageBox::Question,Language::key("Delete instance"),Language::key("Move this instance and its worlds to local trash?")+"\n\n"+i.name,QMessageBox::Yes|QMessageBox::No,this);confirm.setObjectName("confirmDeleteInstance");confirm.setDefaultButton(QMessageBox::No);if(confirm.exec()!=QMessageBox::Yes)return;
    try{m_mc->deleteInstance(i.dir);m_selectedDir.clear();m_library->setCurrentWidget(m_grid);refreshInstances();m_status->setText(Language::key("Instance moved to trash. Your worlds are preserved."));}catch(...){error(exception());}
}
void MainWindow::editInstance(){
    auto i=selected();if(i.dir.isEmpty())return;if(m_installing.contains(i.dir)||m_mc->isRunning(i.dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}
    QJsonObject info;try{info=ModRepository::read(i.dir+"/instance.json");}catch(...){error(exception());return;}
    QDialog d(this);d.setObjectName("instanceSettingsWindow");d.setWindowTitle(i.name+" · "+Language::key("Instance settings"));Ui::fitToScreen(&d,{640,600});QVBoxLayout lay(&d);lay.setContentsMargins(22,20,22,18);lay.setSpacing(12);
    auto head=new QHBoxLayout;head->setSpacing(14);lay.addLayout(head);
    // Icon and background are chosen here and saved with the other settings.
    QString iconKey=info["icon"].toString(),backgroundKey=info["background"].toString();QImage customIcon,customBackground;
    auto icon=new QToolButton;icon->setObjectName("settingsInstanceIcon");icon->setFixedSize(64,64);icon->setIconSize({48,48});icon->setIcon(InstanceIcons::icon(iconKey,i.dir));icon->setCursor(Qt::PointingHandCursor);icon->setPopupMode(QToolButton::InstantPopup);
    icon->setToolTip(Language::key("Change icon"));icon->setAccessibleName(icon->toolTip());
    icon->setMenu(InstanceIcons::menu(icon,[&,icon](const QString &key,const QImage &picture){iconKey=key;customIcon=picture;icon->setIcon(key=="custom"?QIcon(QPixmap::fromImage(picture)):InstanceIcons::icon(key));}));head->addWidget(icon);
    auto titles=new QVBoxLayout;titles->setSpacing(2);head->addLayout(titles,1);label(i.name,titles,"sectionTitle",false);label(InstanceText::loader(i.loader)+" "+i.mcVersion,titles,"muted",false);
    QFormLayout form;form.setHorizontalSpacing(14);form.setVerticalSpacing(10);lay.addLayout(&form);
    QLineEdit name(i.name),java(info["javaPath"].toString());java.setObjectName("javaPath");java.setPlaceholderText(text("Detección automática","Automatic detection","Detecção automática"));QSpinBox memory;memory.setRange(512,65536);memory.setSingleStep(512);memory.setValue(i.xmx);memory.setSuffix(" MB");
    form.addRow(text("Nombre","Name","Nome"),&name);form.addRow(text("Memoria","Memory","Memória"),&memory);
    auto backgroundRow=new QHBoxLayout;backgroundRow->setSpacing(10);auto preview=new QLabel;preview->setFixedSize(128,72);backgroundRow->addWidget(preview);
    auto showBackground=[preview](const QString &path,const QImage &picture){if(!picture.isNull()){preview->setPixmap(Ui::cover(QPixmap::fromImage(picture),{128,72},8));return;}Ui::loadArt(path,{256,144},preview,[preview](const QPixmap &p){preview->setPixmap(Ui::cover(p,{128,72},8));});};
    showBackground(InstanceIcons::background(i.dir),{});
    auto changeArt=button(text("Cambiar fondo…","Change background…","Mudar fundo…"),backgroundRow,[&,showBackground]{QString key;QImage picture;if(!InstanceIcons::chooseBackground(&d,i.dir,backgroundKey,key,picture))return;backgroundKey=key;customBackground=picture;showBackground(key.startsWith(":/")?key:key=="custom"?i.dir+"/instance-background.jpg":Ui::artFor(i.dir),picture);},&d,false,"image");changeArt->setObjectName("settingsInstanceBackground");
    backgroundRow->addStretch();form.addRow(text("Fondo","Background","Fundo"),backgroundRow);
    QCheckBox software(text("Gráficos por software (máquinas virtuales o sin driver de gráficos; más lento)","Software graphics (virtual machines or no graphics driver; slower)","Gráficos por software (máquinas virtuais ou sem driver de vídeo; mais lento)"));software.setObjectName("softwareRendering");software.setChecked(info["softwareRendering"].toBool());
    if(SoftwareGl::available())form.addRow(text("Gráficos","Graphics","Gráficos"),&software);else software.hide();
    auto javaRow=new QHBoxLayout;javaRow->setSpacing(8);javaRow->addWidget(&java,1);form.addRow("Java",javaRow);
    auto browse=new QToolButton;browse->setIcon(Ui::icon("folder"));browse->setToolTip(text("Elegir ejecutable de Java","Choose Java executable","Escolher executável Java"));browse->setAccessibleName(browse->toolTip());javaRow->addWidget(browse);
    connect(browse,&QToolButton::clicked,&d,[&]{auto f=QFileDialog::getOpenFileName(&d);if(!f.isEmpty())java.setText(f);});
    auto detected=new QComboBox;detected->setObjectName("detectedJava");detected->addItem(Language::key("Automatic (recommended)"),QString());
    auto rescan=new QToolButton;rescan->setIcon(Ui::icon("refresh-cw"));rescan->setToolTip(Language::key("Scan for Java again"));rescan->setAccessibleName(rescan->toolTip());
    auto detectedRow=new QHBoxLayout;detectedRow->setSpacing(8);detectedRow->addWidget(detected,1);detectedRow->addWidget(rescan);form.addRow(Language::key("Detected Java"),detectedRow);
    auto javaStatus=label(Language::key("Checking installed Java versions…"),&lay,"muted");
    auto watcher=new QFutureWatcher<QList<JavaInstallation>>(&d);
    int required=0;try{required=ModRepository::read(i.dir+"/launch-profile.json")["javaVersion"].toObject()["majorVersion"].toInt(8);}catch(...){}
    auto scan=[watcher,javaStatus]{if(watcher->isRunning())return;javaStatus->setText(Language::key("Checking installed Java versions…"));watcher->setFuture(QtConcurrent::run([]{return JavaRuntime::discover();}));};
    connect(watcher,&QFutureWatcher<QList<JavaInstallation>>::finished,&d,[&,detected,javaStatus,required]{
        QSignalBlocker blocked(detected);detected->clear();detected->addItem(Language::key("Automatic (recommended)"),QString());
        auto installations=watcher->result();QStringList summary;for(auto j:installations){detected->addItem("Java "+j.version+" · "+j.architecture+" · "+j.path,j.path);summary<<QString::number(j.major);}
        summary.removeDuplicates();javaStatus->setText(installations.isEmpty()?Language::key("No working Java installation was found."):Language::key("Detected Java")+": "+summary.join(", ")+(required?" · "+Language::key("Required")+": "+QString::number(required):QString()));
        auto index=detected->findData(java.text());detected->setCurrentIndex(index>=0?index:0);
    });
    connect(detected,qOverload<int>(&QComboBox::activated),&d,[&]{java.setText(detected->currentData().toString());});
    connect(rescan,&QToolButton::clicked,&d,scan);scan();
    lay.addStretch();
    auto tools=new QHBoxLayout;tools->setSpacing(8);lay.addLayout(tools);
    button(text("Abrir carpeta","Open folder","Abrir pasta"),tools,[i]{QDesktopServices::openUrl(QUrl::fromLocalFile(i.dir));},&d,false,"folder");
    button(text("Registro","Log","Registro"),tools,[&,i]{showLog(i.dir,&d);},&d,false,"file-text");
    button(text("Reparar instalación","Repair installation","Reparar instalação"),tools,[&,i]{d.reject();m_installing.insert(i.dir);m_mc->installInstance(i.dir);refreshInstances();},&d,false,"hammer");
    tools->addStretch();auto trash=button({},tools,[&]{d.reject();QTimer::singleShot(0,this,[this]{removeInstance();});},&d,false,"trash-2");trash->setProperty("danger",true);trash->setToolTip(text("Mover instancia a la papelera","Move instance to trash","Mover instância para a lixeira"));trash->setAccessibleName(trash->toolTip());
    QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);lay.addWidget(&buttons);buttons.button(QDialogButtonBox::Save)->setProperty("play",true);
    connect(&buttons,&QDialogButtonBox::accepted,&d,[&]{if(name.text().trimmed().isEmpty())return;info["name"]=name.text().trimmed();info["javaPath"]=java.text().trimmed();info["xmx"]=memory.value();
        if(iconKey=="custom"&&!customIcon.isNull()&&!InstanceIcons::saveCustom(i.dir,customIcon))iconKey=info["icon"].toString();
        if(backgroundKey=="custom"&&!customBackground.isNull()&&!InstanceIcons::saveBackground(i.dir,customBackground))backgroundKey=info["background"].toString();
        info["icon"]=iconKey;info["background"]=backgroundKey;if(SoftwareGl::available())info["softwareRendering"]=software.isChecked();try{ModRepository::write(i.dir+"/instance.json",info);d.accept();const auto moved=m_mc->renameFolder(i.dir);if(m_selectedDir==i.dir)m_selectedDir=moved;refreshInstances();}catch(...){error(exception());}});connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
    Ui::openWindow(d);
}
void MainWindow::manageMods(){
    auto i=selected();if(i.dir.isEmpty())return;if(m_mc->isRunning(i.dir)||m_installing.contains(i.dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}
    QDialog d(this);d.setObjectName("modsWindow");d.setWindowTitle(i.name+" · Mods");Ui::fitToScreen(&d,{820,580});QVBoxLayout lay(&d);lay.setContentsMargins(20,18,20,18);lay.setSpacing(12);
    label(i.name+" · Mods",&lay,"sectionTitle",false);QListWidget list;list.setObjectName("modList");list.setIconSize(QSize(22,22));lay.addWidget(&list,1);
    auto refresh=[&]{list.clear();for(const auto &f:QDir(i.dir+"/mods").entryList({"*.jar","*.jar.disabled"},QDir::Files)){auto item=new QListWidgetItem(Ui::icon("puzzle",f.endsWith(".disabled")?QColor(110,110,118):Ui::accent()),f,&list);if(f.endsWith(".disabled"))item->setForeground(QColor(130,130,138));}};refresh();
    auto row=new QHBoxLayout;row->setSpacing(8);lay.addLayout(row);
    button(text("Activar / desactivar","Enable / disable","Ativar / desativar"),row,[&]{if(!list.currentItem())return;auto name=list.currentItem()->text();auto to=name.endsWith(".disabled")?name.chopped(9):name+".disabled";if(!QFile::rename(i.dir+"/mods/"+name,i.dir+"/mods/"+to))error(text("No se pudo cambiar el archivo.","Could not change the file.","Não foi possível alterar o arquivo."));refresh();},&d,false,"square");
    button(text("Agregar JAR local","Add local JAR","Adicionar JAR local"),row,[&]{for(const auto &f:QFileDialog::getOpenFileNames(&d,{}, {},"Mods (*.jar)")){QDir().mkpath(i.dir+"/mods");if(!QFile::copy(f,i.dir+"/mods/"+QFileInfo(f).fileName()))error(text("El archivo ya existe o no se puede copiar.","The file already exists or cannot be copied.","O arquivo já existe ou não pode ser copiado."));}refresh();},&d,false,"plus");
    auto remove=button(text("Quitar mod","Remove mod","Remover mod"),row,[&]{if(!list.currentItem())return;auto name=list.currentItem()->text();QDir().mkpath(i.dir+"/removed-mods");if(!QFile::rename(i.dir+"/mods/"+name,i.dir+"/removed-mods/"+QString::number(QDateTime::currentMSecsSinceEpoch())+"-"+name))error(text("No se pudo mover el archivo.","Could not move the file.","Não foi possível mover o arquivo."));refresh();},&d,false,"trash-2");remove->setProperty("danger",true);
    row->addStretch();button(text("Explorar mods compatibles","Find compatible mods","Buscar mods compatíveis"),row,[&]{d.accept();showPage(Explore);m_target->setCurrentIndex(m_target->findData(i.dir));searchMods();},&d,true,"compass");
    Ui::openWindow(d);refreshInstances();
}
void MainWindow::savePack(){auto i=selected();if(i.dir.isEmpty())return;if(m_mc->isRunning(i.dir)||m_installing.contains(i.dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}bool ok;auto name=QInputDialog::getText(this,text("Guardar pack","Save pack","Salvar pack"),text("Nombre del pack","Pack name","Nome do pack"),QLineEdit::Normal,i.name,&ok);if(!ok||name.trimmed().isEmpty())return;auto root=m_root;work(text("Identificando tus mods en Modrinth…","Identifying your mods on Modrinth…","Identificando seus mods no Modrinth…"),[root,i,name]{ModRepository repo(root);auto p=repo.capture(i.dir,name);repo.savePack(p);return p;},[this](QJsonObject){refreshPacks();showPage(Packs);});}
void MainWindow::applyPack(){
    int index=m_packs->currentRow();if(index<0||index>=m_packData.size())return;
    QStringList names,dirs;for(const auto &i:m_mc->instances())if(i.loader!="vanilla"&&!m_mc->isRunning(i.dir)&&!m_installing.contains(i.dir)){names<<i.name+" · "+i.mcVersion+" / "+InstanceText::loader(i.loader);dirs<<i.dir;}
    if(names.isEmpty()){error(text("Creá una instancia con cargador de mods primero.","Create an instance with a mod loader first.","Crie uma instância com carregador de mods primeiro."));return;}
    bool ok;auto chosen=QInputDialog::getItem(this,text("Destino del pack","Pack destination","Destino do pack"),Language::key("Instance"),names,0,false,&ok);if(!ok||names.indexOf(chosen)<0)return;
    preview(m_packData[index].toObject()["projects"].toArray(),dirs[names.indexOf(chosen)]);
}
void MainWindow::preview(QJsonArray projects,QString dir){
    if(dir.isEmpty())return;if(m_mc->isRunning(dir)||m_installing.contains(dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}
    QJsonObject info;try{info=ModRepository::read(dir+"/instance.json");}catch(...){error(exception());return;}auto root=m_root;
    work(text("Comprobando versiones y dependencias…","Checking versions and dependencies…","Verificando versões e dependências…"),[root,projects,info,dir]{
        QJsonArray installed;if(QFile::exists(dir+"/mods.json")){for(const auto &v:ModRepository::read(dir+"/mods.json")["mods"].toArray())if(QFile::exists(dir+"/mods/"+v.toObject()["filename"].toString()))installed.append(v);}
        return ModRepository(root).plan(projects,info["mcVersion"].toString(),info["loader"].toString(),installed);
    },[this,dir,root](QJsonObject plan){
        QDialog d(this);d.setObjectName("installReview");d.setWindowTitle(text("Revisar instalación","Review installation","Revisar instalação"));Ui::fitToScreen(&d,{760,560});QVBoxLayout lay(&d);lay.setContentsMargins(20,18,20,18);lay.setSpacing(12);
        label(plan["game"].toString()+" / "+plan["loader"].toString(),&lay,"sectionTitle");QListWidget list;list.setIconSize(QSize(20,20));lay.addWidget(&list,1);
        for(const auto &v:plan["versions"].toArray()){auto o=v.toObject();new QListWidgetItem(Ui::icon("download",Ui::accent()),o["name"].toString()+"\n"+o["file"].toObject()["filename"].toString(),&list);}
        for(const auto &v:plan["missing"].toArray())new QListWidgetItem(Ui::icon("x",QColor(255,140,150)),v.toObject()["name"].toString()+"\n"+Language::message(v.toObject()["reason"].toString()),&list);
        for(const auto &v:plan["missing"].toArray()){
            auto reason=v.toObject()["reason"].toString();auto match=QRegularExpression("https://www\\.curseforge\\.com/[^\\s]+").match(reason);
            if(match.hasMatch()){auto url=match.captured();button(Language::key("Open manual download"),&lay,[url]{QDesktopServices::openUrl(QUrl(url));},&d,false,"external-link");}
        }
        QCheckBox skip(text("Instalar solo los compatibles; omitir los no disponibles","Install compatible mods only; skip unavailable mods","Instalar apenas compatíveis; ignorar indisponíveis"));if(!plan["missing"].toArray().isEmpty())lay.addWidget(&skip);
        label(text("Se incluyen las dependencias obligatorias. Revisá los mods omitidos antes de continuar.","Required dependencies are included. Review skipped mods before continuing.","Dependências obrigatórias estão incluídas. Revise os mods ignorados antes de continuar."),&lay,"muted");
        QDialogButtonBox bb(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);lay.addWidget(&bb);bb.button(QDialogButtonBox::Ok)->setText(text("Instalar mods","Install mods","Instalar mods"));bb.button(QDialogButtonBox::Ok)->setProperty("play",true);
        auto valid=[&]{bb.button(QDialogButtonBox::Ok)->setEnabled(!plan["versions"].toArray().isEmpty()&&(plan["missing"].toArray().isEmpty()||skip.isChecked()));};valid();connect(&skip,&QCheckBox::toggled,&d,valid);connect(&bb,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&bb,&QDialogButtonBox::rejected,&d,&QDialog::reject);
        if(Ui::openWindow(d)!=QDialog::Accepted)return;
        work(text("Descargando y verificando los mods…","Downloading and verifying mods…","Baixando e verificando os mods…"),[root,dir,plan]{ModRepository(root).apply(dir,plan);return QJsonObject{};},[this](QJsonObject){refreshInstances();m_status->setText(text("Mods instalados. Tu instancia está actualizada.","Mods installed. Your instance is up to date.","Mods instalados. Sua instância está atualizada."));});
    });
}
void MainWindow::searchMods(int offset){
    // Another background task is running (an install, an import): search as soon as it ends instead of ignoring Enter.
    if(m_jobs){QTimer::singleShot(400,this,[this,offset]{searchMods(offset);});return;}
    auto dir=m_target->currentData().toString();if(dir.isEmpty()){error(text("Creá una instancia con cargador de mods primero.","Create an instance with a mod loader first.","Crie uma instância com carregador de mods primeiro."));return;}
    QJsonObject info;try{info=ModRepository::read(dir+"/instance.json");}catch(...){error(exception());return;}auto root=m_root,query=m_query->text(),provider=m_modProvider->currentData().toString();
    work(Language::key("Searching provider…"),[root,query,info,offset,provider]{return QJsonObject{{"hits",ModRepository(root).search(query,info["mcVersion"].toString(),info["loader"].toString(),offset,provider)}};},[this,offset](QJsonObject r){
        m_offset=offset;if(offset==0)m_results->clear();auto hits=r["hits"].toArray();m_results->setProperty("lastPage",hits.size()<30);
        for(const auto &v:hits){
            auto p=v.toObject();auto item=new QListWidgetItem(Ui::icon("puzzle",dim),p["title"].toString()+"  ·  "+p["author"].toString()+"\n"+p["description"].toString(),m_results);item->setData(Qt::UserRole,p);item->setToolTip(p["description"].toString());
            const auto url=p["icon_url"].toString();
            loadImage(url,[list=QPointer<QListWidget>(m_results),url](const QPixmap &icon){if(!list)return;for(int n=0;n<list->count();++n)if(list->item(n)->data(Qt::UserRole).toJsonObject()["icon_url"].toString()==url)list->item(n)->setIcon(QIcon(Ui::cover(icon,QSize(88,88),14)));});
        }
        if(hits.isEmpty())m_status->setText(text("No se encontraron más mods compatibles.","No more compatible mods found.","Nenhum outro mod compatível encontrado."));});
}
void MainWindow::account(bool microsoft){
    if(!microsoft){bool ok;const QString suggestion="Player";
        auto name=QInputDialog::getText(this,text("Perfil local","Local profile","Perfil local"),text("Nombre de jugador","Player name","Nome de jogador"),QLineEdit::Normal,suggestion,&ok);if(!ok)return;if(!QRegularExpression("^[A-Za-z0-9_]{3,16}$").match(name).hasMatch()){error(text("Usá entre 3 y 16 letras, números o guiones bajos.","Use 3–16 letters, numbers or underscores.","Use de 3 a 16 letras, números ou sublinhados."));return;}auto hash=QCryptographicHash::hash(("OfflinePlayer:"+name).toUtf8(),QCryptographicHash::Md5);hash[6]=(hash[6]&0x0f)|0x30;hash[8]=(hash[8]&0x3f)|0x80;m_accounts->addAccount({"offline",name,QString::fromLatin1(hash.toHex()),{}, {}});return;}
    QDialog d(this);d.setObjectName("microsoftLogin");d.setWindowTitle("Microsoft");d.resize(600,300);QVBoxLayout lay(&d);lay.setContentsMargins(24,22,24,20);lay.setSpacing(12);auto status=label(text("Conectando…","Connecting…","Conectando…"),&lay);auto code=label("",&lay,"sectionTitle");code->setTextInteractionFlags(Qt::TextSelectableByMouse);lay.addStretch();MsAuth auth;
    QDialogButtonBox cancel(QDialogButtonBox::Cancel);lay.addWidget(&cancel);connect(&cancel,&QDialogButtonBox::rejected,&d,&QDialog::reject);
    connect(&auth,&MsAuth::statusUpdate,&d,[status](const QString &s){status->setText(Language::message(s));});
    connect(&auth,&MsAuth::profilePicture,&d,[this](const QString &uuid,const QImage &picture){m_accounts->setPicture(uuid,picture);});connect(&auth,&MsAuth::loginFailed,&d,[this,&d](const QString &e){d.reject();error(e);});connect(&auth,&MsAuth::loginDone,&d,[this,&d](const QString &token,const QString &refresh,const QString &uuid,const QString &name){m_accounts->addAccount({"msa",name,uuid,token,refresh});d.accept();});QTimer::singleShot(0,&auth,&MsAuth::startLogin);Ui::openWindow(d);auth.cancel();
}
void MainWindow::loadImage(const QString &url,std::function<void(const QPixmap &)> done){
    if(!url.startsWith("https://"))return;if(auto it=m_imageCache.constFind(url);it!=m_imageCache.cend()){done(*it);return;}
    if(qEnvironmentVariableIsSet("EBALIA_NO_NETWORK"))return;
    if(!m_images){m_images=new QNetworkAccessManager(this);m_images->setTransferTimeout(20000);auto cache=new QNetworkDiskCache(m_images);cache->setCacheDirectory(m_root+"/cache/images");cache->setMaximumCacheSize(80*1024*1024);m_images->setCache(cache);}
    QNetworkRequest request{QUrl(url)};request.setHeader(QNetworkRequest::UserAgentHeader,"EBALIA-Launcher/4.0");request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::PreferCache);
    auto reply=m_images->get(request);
    connect(reply,&QNetworkReply::finished,this,[this,reply,url,done]{reply->deleteLater();QPixmap pixmap;if(reply->error()==QNetworkReply::NoError)pixmap.loadFromData(reply->readAll());if(pixmap.isNull())return;if(pixmap.width()>640)pixmap=pixmap.scaledToWidth(640,Qt::SmoothTransformation);m_imageCache.insert(url,pixmap);done(pixmap);});
}
void MainWindow::refreshNews(bool network){
    auto render=[this](QJsonObject o){
        while(auto item=m_homeNews->takeAt(0)){if(item->widget())item->widget()->deleteLater();delete item;}
        auto addCard=[this](const QString &title,const QString &meta,const QString &art,const QString &image,std::function<void()> open){
            auto card=new ContentButton;card->setObjectName("newsCard");card->setCursor(Qt::PointingHandCursor);card->setAccessibleName(title);card->setToolTip(title);card->setMinimumWidth(150);card->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
            auto cl=new QVBoxLayout(card);cl->setContentsMargins(1,1,1,12);cl->setSpacing(8);auto cover=new Cover(11);cover->setObjectName("newsCover");cover->setAspectRatio(16.0/9.0);cover->setArtwork(art);cl->addWidget(cover);
            auto body=new QVBoxLayout;body->setContentsMargins(14,0,14,0);body->setSpacing(4);cl->addLayout(body);
            auto m=elided(body,"newsMeta");m->setText(meta);auto t=label(title,body,"newsTitle");t->setAlignment(Qt::AlignLeft|Qt::AlignTop);body->addStretch();
            for(auto w:{m,t})w->setAttribute(Qt::WA_TransparentForMouseEvents);connect(card,&QPushButton::clicked,this,std::move(open));m_homeNews->addWidget(card,1);
            loadImage(image,[cover=QPointer<Cover>(cover)](const QPixmap &p){if(cover)cover->setPixmap(p);});
        };
        QList<QJsonObject> java,other;for(const auto &v:o["entries"].toArray()){const auto e=v.toObject();if(e["title"].toString().isEmpty())continue;(e["category"].toString().contains("Java")?java:other)<<e;}
        auto cards=java+other;
        for(int n=0;n<qMin(3,int(cards.size()));++n){
            const auto entry=cards[n];const auto link=QUrl(entry["readMoreLink"].toString(entry["link"].toString()));
            const auto image=entry["playPageImage"].toObject()["url"].toString(entry["newsPageImage"].toObject()["url"].toString());
            addCard(entry["title"].toString(),QString(entry["category"].toString()).remove("Minecraft: ").toUpper()+"  ·  "+entry["date"].toString().left(10),Ui::artFor(entry["title"].toString()),image.isEmpty()?QString():"https://launchercontent.mojang.com"+image,[this,link]{if(minecraftLink(link))QDesktopServices::openUrl(link);else showPage(News);});
        }
        if(cards.isEmpty()){ // offline first start: shortcuts to the main sections instead of empty space
            addCard(k("New instance"),"EBALIA",":/art/f2_2.jpg",{},[this]{createInstance();});
            addCard(k("Discover mods"),"MODRINTH  ·  CURSEFORGE",":/art/f2_6.jpg",{},[this]{showPage(Explore);});
            addCard(k("Lost versions"),"EBALIA",":/art/f3_6.jpg",{},[this]{showPage(Lost);});
        }
        m_news->clear();
        for(const auto &e:java+other){
            const auto image=e["newsPageImage"].toObject()["url"].toString(e["playPageImage"].toObject()["url"].toString());const auto url=image.isEmpty()?QString():"https://launchercontent.mojang.com"+image;
            auto item=new QListWidgetItem(QIcon(Ui::art(Ui::artFor(e["title"].toString()),QSize(176,80),8)),e["title"].toString()+"\n"+e["category"].toString()+"  ·  "+e["date"].toString().left(10)+"\n"+e["text"].toString(),m_news);
            item->setData(Qt::UserRole,e["readMoreLink"].toString(e["link"].toString()));item->setData(Qt::UserRole+1,url);item->setToolTip(e["text"].toString());
            loadImage(url,[list=QPointer<QListWidget>(m_news),url](const QPixmap &p){if(!list)return;for(int n=0;n<list->count();++n)if(list->item(n)->data(Qt::UserRole+1).toString()==url)list->item(n)->setIcon(QIcon(Ui::cover(p,QSize(176,80),8)));});
            if(m_news->count()>=30)break;
        }
        if(m_news->count()==0)m_news->addItem(new QListWidgetItem(Ui::icon("newspaper",dim),text("No hay noticias guardadas. Conectate y pulsá Actualizar.","No cached news. Connect and press Refresh.","Sem notícias salvas. Conecte e clique em Atualizar.")));
    };
    auto path=m_root+"/news.json";if(!network){try{render(ModRepository::read(path));}catch(...){render({});}return;}
    auto watcher=new QFutureWatcher<QJsonObject>(this);connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,watcher,render]{auto r=watcher->result();watcher->deleteLater();if(r.contains("entries"))render(r);else if(m_status)m_status->setText(text("Noticias sin conexión: se conserva la última copia.","News offline: keeping the last cached copy.","Notícias offline: mantendo a última cópia salva."));});
    watcher->setFuture(QtConcurrent::run([path]{try{auto o=QJsonDocument::fromJson(ModRepository::fetch(QUrl("https://launchercontent.mojang.com/v2/news.json"))).object();if(o["entries"].toArray().isEmpty())throw std::runtime_error("Empty news");ModRepository::write(path,o);return o;}catch(...){return QJsonObject{};}}));
}
void MainWindow::lostAction(bool launch){
    auto id=filename(m_lost);VersionInfo v;for(const auto &entry:m_versions->getVersions())if(entry.id==id)v=entry;if(v.id.isEmpty())return;
    auto dest=m_versions->getInstallPath(v);
    if(launch){
        if(m_java->isRunning()){if(QMessageBox::question(this,"EBALIA",Language::key("Stop")+" · "+v.name+"?")==QMessageBox::Yes)m_java->stop();return;}
        if(!m_versions->isVersionInstalled(v)){error(text("Primero instalá esta versión.","Install this version first.","Instale esta versão primeiro."));return;}
        // Versions with a native description run with Java on every system; the rest keep the original Windows package (Wine elsewhere).
        const bool native=LostNative::available(v.native);
        const bool windows=!native&&LostInstaller::windowsPackage(v);
#ifndef Q_OS_WIN
        if(windows&&LostInstaller::wine().isEmpty()){error(Language::key("This Windows package requires Wine. Install Wine or run it on Windows."));return;}
#endif
        auto root=m_root;const auto libraries=m_mc->mcDir()+"/libraries";const auto player=m_accounts->active().name;
        work(Language::key("Preparing Java 8 for this version…"),[root,windows,native,v,dest,libraries]{
                QJsonObject r;try{r["java"]=LostInstaller::java(root,windows);}catch(const std::exception &e){r["warning"]=QString::fromUtf8(e.what());}
                if(native)LostNative::prepare(v.native,dest,libraries); // errors reach the user through work()
                return r;},
            [this,v,dest,player,libraries](QJsonObject r){if(r.contains("warning"))m_status->setText(Language::message(r["warning"].toString()));m_java->launch(v,dest,r["java"].toString(),player,libraries);});
        return;
    }
    work(text("Descargando y preparando la versión perdida…","Downloading and preparing the lost version…","Baixando e preparando a versão perdida…"),[v,dest]{
        LostInstaller::install(v,dest);return QJsonObject{};
    },[this](QJsonObject){refreshLost();m_status->setText(text("Versión perdida instalada","Lost version installed","Versão perdida instalada"));});
}
bool MainWindow::canUpdate() const {
    if(m_jobs||!m_installing.isEmpty()||m_java->isRunning()||Ui::openWindows()>0||findChild<QWidget*>("interactiveTour"))return false;
    for(auto page:findChildren<QWidget*>("bedrockLabPage"))if(page->property("bedrockInstalling").toBool())return false;
    for(const auto &instance:m_mc->instances())if(m_mc->isInstalling(instance.dir)||m_mc->isRunning(instance.dir))return false;
    return true;
}
void MainWindow::closeEvent(QCloseEvent *event){
    if(property("launcherUpdating").toBool()){event->ignore();return;}
    if(m_java->isRunning()){event->ignore();error(k("Close running instances before exiting."));return;}
    for(auto page:findChildren<QWidget*>("bedrockLabPage"))
        if(page->property("bedrockInstalling").toBool()) {
            event->ignore();error(text("Esperá a que termine la instalación de Bedrock.","Wait for Bedrock setup to finish.","Aguarde a instalação do Bedrock terminar."));return;
        }

    if(Ui::openWindows()>0){event->ignore();error(k("Close the open windows first."));return;}
    const auto all=m_mc->instances();
    if(m_jobs||!m_installing.isEmpty()||std::any_of(all.begin(),all.end(),[this](const McInstance &i){return m_mc->isInstalling(i.dir);})){error(text("Esperá a que terminen las operaciones antes de cerrar.","Wait for operations to finish before closing.","Aguarde as operações terminarem antes de fechar."));event->ignore();return;}
    for(const auto &i:all)if(m_mc->isRunning(i.dir)){error(text("Cerrá las instancias en ejecución antes de salir.","Close running instances before exiting.","Feche as instâncias em execução antes de sair."));event->ignore();return;}
    event->accept();
}
