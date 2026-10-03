#include "Ui.hpp"
#include <QtWidgets>
#include <QtConcurrent>
namespace {
int windowsOpen=0;
QString artKey(const QString &path,QSize bounds){return path+QString(":%1x%2").arg(bounds.width()).arg(bounds.height());}
struct ArtRequest {QPointer<QObject> receiver;std::function<void(const QPixmap &)> ready;};
class ArtCache:public QObject {
public:
    ArtCache():QObject(qApp),small(24*1024),large(64*1024){pool.setMaxThreadCount(2);pool.setExpiryTimeout(10000);}
    QCache<QString,QPixmap> small,large;
    QHash<QString,QList<ArtRequest>> pending;
    QThreadPool pool;
    QCache<QString,QPixmap> &cache(QSize bounds){return bounds.width()<=768?small:large;}
};
ArtCache &artCache(){static QPointer<ArtCache> cache;if(!cache)cache=new ArtCache;return *cache;}
}
QPixmap Ui::cachedArt(const QString &resource,QSize bounds){auto p=artCache().cache(bounds).object(artKey(resource,bounds));return p?*p:QPixmap();}
void Ui::loadArt(const QString &resource,QSize bounds,QObject *receiver,std::function<void(const QPixmap &)> ready){
    if(!receiver||resource.isEmpty()||bounds.isEmpty())return;
    auto &cache=artCache();const auto key=artKey(resource,bounds);
    if(auto p=cache.cache(bounds).object(key)){ready(*p);return;}
    const bool loading=cache.pending.contains(key);cache.pending[key].append({receiver,std::move(ready)});if(loading)return;
    auto watcher=new QFutureWatcher<QImage>(&cache);
    QObject::connect(watcher,&QFutureWatcher<QImage>::finished,&cache,[watcher,key,bounds]{
        const auto result=watcher->result();watcher->deleteLater();auto pixmap=QPixmap::fromImage(result);auto &cache=artCache();
        if(!pixmap.isNull())cache.cache(bounds).insert(key,new QPixmap(pixmap),qMax(1,int(result.sizeInBytes()/1024)));
        const auto requests=cache.pending.take(key);for(const auto &request:requests)if(request.receiver)request.ready(pixmap);
    });
    watcher->setFuture(QtConcurrent::run(&cache.pool,[resource,bounds]{
        QImageReader reader(resource);reader.setAutoTransform(true);const auto original=reader.size();
        if(!original.isEmpty())reader.setScaledSize(original.scaled(bounds,Qt::KeepAspectRatio).boundedTo(original));
        return reader.read().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }));
}
QPixmap Ui::pixmap(const QString &name,int size,const QColor &color){
    static QHash<QString,QPixmap> cache;const auto key=name+"|"+QString::number(size)+"|"+color.name(QColor::HexArgb);
    if(auto it=cache.constFind(key);it!=cache.cend())return *it;
    QImage source(":/icons/ui/"+name+".png");if(source.isNull())return {};
    // The PNGs are white; recolor them so one file serves every state.
    QImage tinted=source.convertToFormat(QImage::Format_ARGB32_Premultiplied);QPainter p(&tinted);p.setCompositionMode(QPainter::CompositionMode_SourceIn);p.fillRect(tinted.rect(),color);p.end();
    auto result=QPixmap::fromImage(tinted.scaled(size,size,Qt::KeepAspectRatio,Qt::SmoothTransformation));cache.insert(key,result);return result;
}
QIcon Ui::icon(const QString &name,const QColor &color){QIcon result;for(int size:{16,20,24,32,48})result.addPixmap(pixmap(name,size,color));result.addPixmap(pixmap(name,24,QColor(120,120,128)),QIcon::Disabled);return result;}
QIcon Ui::navigationIcon(const QString &name){
    QIcon result;const QPixmap source(":/icons/navigation/"+name+".png");
    for(int size:{24,28,32,48,64}){auto p=source.scaled(size,size,Qt::KeepAspectRatio,Qt::SmoothTransformation);result.addPixmap(p);result.addPixmap(p,QIcon::Selected);result.addPixmap(p,QIcon::Active);}
    return result;
}
QString Ui::lostArtFor(const QString &id,const QString &category){
    static const QHash<QString,QString> covers{
        {"a1.2.6_06","mineshaft"},{"alpha_1.2.7","adventure"},{"alpha_1.2.6_04","village"},
        {"alpha_herobrine_test","mangroves"},{"minecraft_a1.1.1","bees"},{"alpha_1.0.16.05_20","island"},{"alpha_1.3","ocean"},
        {"alpha_d3ath_exe","pale-garden"},{"alpha_0.0.0_remastered","warden"},{"ghost","night"},
        {"minecraft_1_li02","nether"},{"nsss","caves"},{"undef_554e","wild"},
        {"1.1_patch_1","colors"},{"minecraft_1.7.10","aquatic"}
    };
    if(covers.contains(id))return ":/art/backgrounds/"+covers.value(id)+".png";
    QString name;
    if(category=="horror")name="pale-garden";
    else if(category=="release")name="tricky-trials";
    else {uint hash=0;for(auto c:id)hash=hash*31+c.unicode();name=hash%2?"island":"mineshaft";}
    return ":/art/backgrounds/"+name+".png";
}
QString Ui::artFor(const QString &key){
    static const QStringList banners{":/art/f1_2.jpg",":/art/f1_6.jpg",":/art/f2_2.jpg",":/art/f2_6.jpg",":/art/f2_10.jpg",":/art/f3_6.jpg"};
    return banners[int(qHash(key)%uint(banners.size()))];
}
QPixmap Ui::cover(const QPixmap &source,QSize size,int radius){
    QPixmap out(size);out.fill(Qt::transparent);if(source.isNull()||size.isEmpty())return out;
    auto scaled=source.scaled(size,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);
    QPainter p(&out);p.setRenderHint(QPainter::Antialiasing);if(radius>0){QPainterPath clip;clip.addRoundedRect(QRectF(QPointF(0,0),QSizeF(size)),radius,radius);p.setClipPath(clip);}
    p.drawPixmap(0,0,scaled,(scaled.width()-size.width())/2,0,size.width(),size.height()); // keep the sky, drop the HUD at the bottom of some screenshots
    return out;
}
QPixmap Ui::art(const QString &resource,QSize size,int radius){return cover(QPixmap(resource),size,radius);}
QPixmap Ui::avatar(const QString &name,int size){
    QPixmap out(size,size);out.fill(Qt::transparent);QPainter p(&out);p.setRenderHint(QPainter::Antialiasing);p.setPen(Qt::NoPen);
    static const QList<QColor> colors{QColor(60,133,39),QColor(45,99,214),QColor(181,98,36),QColor(132,63,178),QColor(32,140,140),QColor(176,52,74)};
    p.setBrush(name.isEmpty()?QColor(52,52,58):colors[int(qHash(name)%uint(colors.size()))]);p.drawRoundedRect(QRectF(0,0,size,size),size*0.28,size*0.28);
    if(name.isEmpty()){const int inner=size*3/5;p.drawPixmap((size-inner)/2,(size-inner)/2,pixmap("user-round",inner,QColor(200,200,206)));return out;}
    QFont font=QApplication::font();font.setBold(true);font.setPixelSize(size*9/20);p.setFont(font);p.setPen(Qt::white);p.drawText(QRect(0,0,size,size),Qt::AlignCenter,name.left(1).toUpper());return out;
}
int Ui::openWindows(){return windowsOpen;}
Ui::ResponsiveRow::ResponsiveRow(int breakpoint,QWidget *parent):QWidget(parent),m_breakpoint(breakpoint){
    m_box=new QBoxLayout(QBoxLayout::TopToBottom,this);m_box->setContentsMargins(0,0,0,0);m_box->setSpacing(12);
    m_box->setSizeConstraint(QLayout::SetNoConstraint);
}
QSize Ui::ResponsiveRow::minimumSizeHint() const {return {0,m_box->minimumSize().height()};}
void Ui::ResponsiveRow::resizeEvent(QResizeEvent *event){
    QWidget::resizeEvent(event);const auto direction=width()<m_breakpoint?QBoxLayout::TopToBottom:QBoxLayout::LeftToRight;
    if(m_box->direction()!=direction){m_box->setDirection(direction);updateGeometry();}
}
void Ui::fitToScreen(QWidget *window,QSize preferred){
    auto screen=window->screen()?window->screen():QGuiApplication::primaryScreen();if(!screen){window->resize(preferred);return;}
    auto available=screen->availableGeometry().size()*0.92;window->resize(preferred.boundedTo(available));
}
int Ui::openWindow(QDialog &dialog){
    QList<QPointer<QWidget>> blocked;
    if(auto owner=dialog.parentWidget()?dialog.parentWidget()->window():nullptr){
        if(auto main=qobject_cast<QMainWindow*>(owner))blocked<<main->centralWidget();
        else for(auto child:owner->findChildren<QWidget*>(Qt::FindDirectChildrenOnly))if(!child->isWindow()&&child->isEnabled())blocked<<child;
    }
    for(auto &w:blocked)if(w)w->setEnabled(false);
    dialog.setWindowModality(Qt::NonModal);dialog.setSizeGripEnabled(true);
    QEventLoop loop;QObject::connect(&dialog,&QDialog::finished,&loop,&QEventLoop::exit);
    ++windowsOpen;dialog.show();dialog.raise();dialog.activateWindow();const int result=loop.exec();--windowsOpen;
    for(auto &w:blocked)if(w)w->setEnabled(true);
    return dialog.isVisible()?(dialog.hide(),QDialog::Rejected):result;
}
QString Ui::styleSheet(){return QStringLiteral(R"(
QWidget{background:transparent;color:#f1f1f3;font-size:13px;}
QMainWindow,QDialog,#content{background:#141416;}
QMenu{background:#1e1e22;border:1px solid #34343a;border-radius:10px;padding:6px;}QMenu::item{padding:8px 26px 8px 10px;border-radius:7px;}QMenu::item:selected{background:#2b3a27;}QMenu::separator{height:1px;background:#2e2e33;margin:5px 8px;}QMenu::icon{padding-left:8px;}
QToolTip{background:#26262b;color:#ffffff;border:1px solid #3a3a41;padding:5px;}
#sidebar{background:#1b1b1e;border-right:1px solid #26262a;}
#accountButton{background:#232327;border:1px solid #2e2e33;border-radius:12px;padding:0;text-align:left;}#accountButton:hover{background:#2a2a2f;border-color:#3d3d44;}
#brand{font-size:17px;font-weight:900;letter-spacing:3px;color:#ffffff;}#accountName{font-weight:800;font-size:14px;}#accountType,#sidebarVersion{color:#9a9aa3;font-size:11px;}
#navigation{background:transparent;border:0;outline:0;padding:0;}
#navigation::item{padding:0 12px;border-radius:10px;margin:2px 0;color:#cacad0;border:0;font-weight:600;}
#navigation::item:hover{background:#25252a;color:#ffffff;}
#navigation::item:selected{background:#2b3a27;color:#ffffff;}
#navSettings{background:transparent;border:0;border-radius:10px;padding:11px 12px;text-align:left;color:#cacad0;font-weight:600;}#navSettings:hover{background:#25252a;color:#ffffff;}#navSettings:checked{background:#2b3a27;color:#ffffff;}
#pageTitle{font-size:28px;font-weight:800;color:#ffffff;}#pageSubtitle,#muted{color:#a4a4ad;}#sectionTitle{font-size:18px;font-weight:800;}
#card,#settingsCard{background:#1e1e22;border:1px solid #2b2b30;border-radius:14px;}
QPushButton{background:#2a2a2f;border:1px solid #36363c;border-radius:8px;padding:9px 16px;font-weight:700;color:#f1f1f3;}
QPushButton:hover{background:#333339;border-color:#4a4a52;}QPushButton:pressed{background:#222226;}QPushButton:disabled{color:#6f6f78;background:#202023;border-color:#2b2b30;}
QPushButton[play=true]{background:#3c8527;border:0;border-bottom:4px solid #285c1a;border-radius:6px;color:#ffffff;font-weight:900;}
QPushButton[play=true]:hover{background:#469a2e;}QPushButton[play=true]:pressed{background:#327020;border-bottom-width:1px;}QPushButton[play=true]:disabled{background:#2a4423;color:#9db896;border-bottom-color:#203519;}
QPushButton[secondary=true]{background:#2d63d6;border:0;border-bottom:4px solid #1d438f;border-radius:6px;color:#ffffff;font-weight:900;}QPushButton[secondary=true]:hover{background:#3a72e8;}
QPushButton[danger=true]{background:#3a1d22;border-color:#6b2a35;color:#ffa3ae;}QPushButton[danger=true]:hover{background:#4b2229;}
QPushButton[link=true]{background:transparent;border:0;color:#6fd15b;font-weight:800;padding:3px 0;text-align:left;}QPushButton[link=true]:hover{color:#97ec85;}
QPushButton[tab=true]{background:#1e1e22;border:1px solid #2b2b30;border-radius:9px;padding:8px 16px;color:#c9c9cf;}QPushButton[tab=true]:hover{background:#26262b;}QPushButton[tab=true]:checked{background:#303036;color:#ffffff;border-color:#4b4b53;}
QToolButton{background:#26262b;border:1px solid #34343a;border-radius:9px;padding:8px;}QToolButton:hover{background:#303036;border-color:#4a4a52;}QToolButton::menu-indicator{image:none;}
QLineEdit,QComboBox,QSpinBox{background:#1e1e22;border:1px solid #303036;border-radius:9px;padding:9px 12px;selection-background-color:#3c8527;}
QLineEdit:focus,QComboBox:focus,QSpinBox:focus{border-color:#4a9e31;}QSpinBox{padding-right:26px;}QSpinBox::up-button,QSpinBox::down-button{subcontrol-origin:border;width:24px;border:0;background:transparent;}QSpinBox::up-button{subcontrol-position:top right;}QSpinBox::down-button{subcontrol-position:bottom right;}QSpinBox::up-button:hover,QSpinBox::down-button:hover{background:#2a2a2f;}QSpinBox::up-arrow{image:url(:/icons/ui/chevron-up.png);width:12px;height:12px;}QSpinBox::down-arrow{image:url(:/icons/ui/chevron-down.png);width:12px;height:12px;}QComboBox::down-arrow{image:url(:/icons/ui/chevron-down.png);width:12px;height:12px;}QComboBox::drop-down{border:0;width:26px;}QComboBox QAbstractItemView{background:#1e1e22;border:1px solid #34343a;selection-background-color:#2b3a27;padding:4px;}
QListWidget,QTreeWidget,QTextBrowser,QPlainTextEdit{background:#1a1a1d;border:1px solid #2a2a2f;border-radius:12px;padding:6px;}
QListWidget::item{padding:12px;border-radius:9px;margin:2px;}QListWidget::item:hover{background:#25252a;}QListWidget::item:selected{background:#2b3a27;color:#ffffff;}
QTreeWidget::item{padding:6px 4px;}QTreeWidget::item:selected{background:#2b3a27;color:#ffffff;}
QHeaderView::section{background:#1e1e22;color:#a4a4ad;border:0;border-bottom:1px solid #2a2a2f;padding:7px 6px;font-weight:700;}
QTabWidget::pane{border:0;}QTabBar::tab{background:transparent;color:#b5b5bc;padding:10px 14px;border-bottom:3px solid transparent;font-weight:700;}QTabBar::tab:selected{color:#ffffff;border-bottom:3px solid #4a9e31;}QTabBar::tab:hover{color:#ffffff;}
QScrollArea{border:0;}QScrollBar:vertical{background:transparent;width:10px;margin:2px;}QScrollBar::handle:vertical{background:#3a3a41;border-radius:4px;min-height:30px;}QScrollBar::handle:vertical:hover{background:#4a4a52;}
QScrollBar:horizontal{background:transparent;height:10px;margin:2px;}QScrollBar::handle:horizontal{background:#3a3a41;border-radius:4px;min-width:30px;}QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0;}QScrollBar::add-page,QScrollBar::sub-page{background:transparent;}
QProgressBar{border:0;background:#2a2a2f;border-radius:5px;min-height:8px;max-height:14px;text-align:center;font-size:10px;}QProgressBar::chunk{background:#4a9e31;border-radius:5px;}
QCheckBox::indicator,QRadioButton::indicator{width:15px;height:15px;border:1px solid #55555e;background:#1e1e22;}QCheckBox::indicator{border-radius:4px;}QRadioButton::indicator{border-radius:8px;}
QCheckBox::indicator:checked,QRadioButton::indicator:checked{background:#4a9e31;border-color:#6fd15b;}QCheckBox::indicator:hover,QRadioButton::indicator:hover{border-color:#6fd15b;}
QSplitter::handle{background:transparent;}QSizeGrip{background:transparent;}
#hero QLabel{background:transparent;color:#ffffff;}#heroTitle{font-size:50px;font-weight:900;letter-spacing:3px;}#heroEdition{font-size:16px;font-weight:800;letter-spacing:6px;color:#e6e6e6;}#heroTagline{font-size:24px;font-weight:800;}
#hero #heroBadge{background:rgba(16,20,29,155);border:1px solid rgba(255,255,255,45);border-radius:8px;padding:8px 12px;font-size:11px;font-weight:800;letter-spacing:2px;}
#hero #heroTheme{color:#e6eaf0;font-size:12px;font-weight:700;letter-spacing:1px;}
#heroControls{background:rgba(15,19,27,190);border:1px solid rgba(255,255,255,35);border-radius:12px;}
QToolButton[heroControl=true]{background:transparent;border:0;border-radius:6px;padding:0;}QToolButton[heroControl=true]:hover{background:rgba(255,255,255,35);}QToolButton[heroControl=true]:focus{border:1px solid #b9efd2;}
#heroDot{background:rgba(255,255,255,75);border:0;border-radius:3px;padding:0;margin:8px 2px;}#heroDot:checked{background:#a9e58c;}#heroDot:hover{background:#ffffff;}
#archiveEyebrow{color:#d0e7df;font-size:10px;font-weight:800;letter-spacing:2px;}#lostCover QLabel{background:transparent;}
#creatorModCard{background:#202126;border:1px solid #353740;border-radius:14px;}#creatorModCard QLabel{background:transparent;}
#creatorPatreonCard{background:#30232c;border:1px solid #684354;border-radius:14px;}#creatorPatreonCard QLabel{background:transparent;}
QPushButton[patreon=true]{background:#ed6957;border:1px solid #f58978;color:#ffffff;font-weight:800;}QPushButton[patreon=true]:hover{background:#ff8874;}
#playBar{background:#1b1b1e;border-top:1px solid #26262a;}
#instancePicker{background:#232327;border:1px solid #303036;border-radius:10px;padding:0;text-align:left;}#instancePicker:hover{background:#2a2a2f;border-color:#46464e;}
#pickerName,#playerName{font-weight:800;font-size:14px;}#pickerSub,#playerType{color:#9a9aa3;font-size:12px;}
#homePlay{font-size:21px;letter-spacing:1px;}#detailPlay{font-size:17px;}
#newsCard{background:#1e1e22;border:1px solid #2b2b30;border-radius:12px;padding:0;text-align:left;}#newsCard:hover{border-color:#4a9e31;background:#232327;}#newsCard QLabel{background:transparent;}
#newsTitle{font-weight:800;font-size:13px;}#newsMeta{color:#6fd15b;font-size:11px;font-weight:700;}
#chip{background:rgba(26,26,30,220);border:1px solid #3a3a41;border-radius:7px;padding:6px 11px;font-weight:700;}#chip[accent=true]{background:#3c8527;border-color:#4a9e31;color:#ffffff;}
#instanceCard{background:#1e1e22;border:1px solid #2b2b30;border-radius:14px;}#instanceCard:hover{border-color:#4a9e31;background:#222227;}#instanceCard[selected=true]{border:2px solid #4a9e31;}
#instanceCard QLabel,#instanceCards,#instanceGrid,#instanceDetail,#instanceDetail QLabel{background:transparent;}
#cardTitle{font-size:15px;font-weight:800;}#cardFooter{color:#8c8c95;font-size:11px;}
#cardSettings,#instanceDetail QToolButton{background:rgba(38,38,43,230);border:1px solid #3a3a41;border-radius:9px;padding:9px;}#cardSettings:hover,#instanceDetail QToolButton:hover{border-color:#6fd15b;}
#detailTitle{font-size:30px;font-weight:900;color:#ffffff;}#detailIcon{background:rgba(20,20,22,220);border:1px solid #3a3a41;border-radius:16px;}
#statCard{background:rgba(30,30,34,235);border:1px solid #34343a;border-radius:14px;}#statCard:hover{border-color:#4a9e31;}#statCard QLabel{background:transparent;}#statTitle{font-size:16px;font-weight:800;}#statIcon{background:rgba(60,133,39,60);border-radius:10px;}
#groupHeader{background:transparent;border:0;text-align:left;font-size:15px;font-weight:800;padding:6px 2px;}#groupHeader:hover{color:#97ec85;background:transparent;}
#lostList{padding:8px;}#lostList::item{padding:0;margin:4px 2px;border-radius:12px;border:2px solid transparent;}#lostList::item:selected{background:transparent;border:2px solid #4a9e31;}#lostList::item:hover{background:transparent;border:2px solid #3a3a41;}
#lostDetail{background:#1e1e22;border:1px solid #2b2b30;border-radius:14px;}#lostDetail QLabel{background:transparent;}#skinPreview{background:#1e1e22;border:1px solid #2b2b30;border-radius:14px;}#statusBar{background:#18181b;border-top:1px solid #26262a;}#statusText{color:#a4a4ad;}
#instanceSources::item{padding:6px 8px;}
)");}
