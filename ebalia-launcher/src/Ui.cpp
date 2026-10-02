#include "Ui.hpp"
#include <QtWidgets>
namespace {
int windowsOpen=0;
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
QLineEdit:focus,QComboBox:focus,QSpinBox:focus{border-color:#4a9e31;}QComboBox::drop-down{border:0;width:26px;}QComboBox QAbstractItemView{background:#1e1e22;border:1px solid #34343a;selection-background-color:#2b3a27;padding:4px;}
QListWidget,QTreeWidget,QTextBrowser,QPlainTextEdit{background:#1a1a1d;border:1px solid #2a2a2f;border-radius:12px;padding:6px;}
QListWidget::item{padding:12px;border-radius:9px;margin:2px;}QListWidget::item:hover{background:#25252a;}QListWidget::item:selected{background:#2b3a27;color:#ffffff;}
QTreeWidget::item{padding:6px 4px;}QTreeWidget::item:selected{background:#2b3a27;color:#ffffff;}
QHeaderView::section{background:#1e1e22;color:#a4a4ad;border:0;border-bottom:1px solid #2a2a2f;padding:7px 6px;font-weight:700;}
QTabWidget::pane{border:0;}QTabBar::tab{background:transparent;color:#b5b5bc;padding:10px 18px;border-bottom:3px solid transparent;font-weight:700;}QTabBar::tab:selected{color:#ffffff;border-bottom:3px solid #4a9e31;}QTabBar::tab:hover{color:#ffffff;}
QScrollArea{border:0;}QScrollBar:vertical{background:transparent;width:10px;margin:2px;}QScrollBar::handle:vertical{background:#3a3a41;border-radius:4px;min-height:30px;}QScrollBar::handle:vertical:hover{background:#4a4a52;}
QScrollBar:horizontal{background:transparent;height:10px;margin:2px;}QScrollBar::handle:horizontal{background:#3a3a41;border-radius:4px;min-width:30px;}QScrollBar::add-line,QScrollBar::sub-line{width:0;height:0;}QScrollBar::add-page,QScrollBar::sub-page{background:transparent;}
QProgressBar{border:0;background:#2a2a2f;border-radius:5px;min-height:8px;max-height:14px;text-align:center;font-size:10px;}QProgressBar::chunk{background:#4a9e31;border-radius:5px;}
QCheckBox::indicator,QRadioButton::indicator{width:15px;height:15px;border:1px solid #55555e;background:#1e1e22;}QCheckBox::indicator{border-radius:4px;}QRadioButton::indicator{border-radius:8px;}
QCheckBox::indicator:checked,QRadioButton::indicator:checked{background:#4a9e31;border-color:#6fd15b;}QCheckBox::indicator:hover,QRadioButton::indicator:hover{border-color:#6fd15b;}
QSplitter::handle{background:transparent;}QSizeGrip{background:transparent;}
#hero QLabel{background:transparent;color:#ffffff;}#heroTitle{font-size:50px;font-weight:900;letter-spacing:3px;}#heroEdition{font-size:16px;font-weight:800;letter-spacing:6px;color:#e6e6e6;}#heroTagline{font-size:24px;font-weight:800;}
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
