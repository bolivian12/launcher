#include "HomeBanner.hpp"
#include "Language.hpp"
#include "Ui.hpp"
#include <QtWidgets>

namespace {
struct Slide { const char *file; const char *title; };
constexpr Slide slides[]{
    {"chase-skies","Chase the Skies"},
    {"trails-tales","Trails & Tales"},
    {"spring-life","Spring to Life"},
    {"tricky-trials","Tricky Trials"}
};
QString key(const char *s){return Language::key(QString::fromUtf8(s));}
QString artwork(int index){return ":/art/backgrounds/"+QString::fromLatin1(slides[index].file)+".png";}
}

HomeBanner::HomeBanner(QWidget *parent):QWidget(parent),m_rotation(this),m_fade(this){
    setObjectName("hero");setMinimumHeight(200);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(32,26,32,24);layout->setSpacing(12);
    auto top=new QHBoxLayout;layout->addLayout(top);
    auto edition=new QLabel("MINECRAFT  /  JAVA EDITION");edition->setObjectName("heroBadge");top->addWidget(edition);top->addStretch();
    layout->addStretch();
    auto footer=new QHBoxLayout;footer->setSpacing(20);layout->addLayout(footer);
    auto copy=new QVBoxLayout;copy->setSpacing(5);footer->addLayout(copy,1);
    m_theme=new QLabel;m_theme->setObjectName("heroTheme");copy->addWidget(m_theme);
    m_title=new QLabel(key("Your next adventure"));m_title->setObjectName("heroTagline");m_title->setWordWrap(true);copy->addWidget(m_title);
    auto controls=new QWidget;controls->setObjectName("heroControls");auto row=new QHBoxLayout(controls);row->setContentsMargins(8,6,8,6);row->setSpacing(4);footer->addWidget(controls,0,Qt::AlignBottom);
    auto control=[this,row](const char *name,const char *label,const QString &icon){
        auto button=new QToolButton;button->setObjectName(name);button->setProperty("heroControl",true);button->setFixedSize(32,32);button->setIconSize(QSize(16,16));
        button->setIcon(Ui::icon(icon));button->setToolTip(key(label));button->setAccessibleName(button->toolTip());button->setCursor(Qt::PointingHandCursor);row->addWidget(button);return button;
    };
    auto previous=control("heroPrevious","Previous background","chevron-left");
    connect(previous,&QToolButton::clicked,this,[this]{selectSlide((m_index+slideCount()-1)%slideCount());});
    for(int i=0;i<slideCount();++i){
        auto dot=new QToolButton;dot->setObjectName("heroDot");dot->setCheckable(true);dot->setFixedSize(18,22);dot->setCursor(Qt::PointingHandCursor);
        dot->setAccessibleName(QString::fromLatin1(slides[i].title));dot->setToolTip(dot->accessibleName());row->addWidget(dot);m_dots<<dot;
        connect(dot,&QToolButton::clicked,this,[this,i]{selectSlide(i);});
    }
    auto next=control("heroNext","Next background","chevron-right");connect(next,&QToolButton::clicked,this,[this]{selectSlide((m_index+1)%slideCount());});
    m_pause=control("heroPause","Pause backgrounds","pause");m_pause->setCheckable(true);
    m_paused=!QSettings().value("ui/rotateBackgrounds",true).toBool();m_pause->setChecked(m_paused);
    connect(m_pause,&QToolButton::toggled,this,[this](bool paused){m_paused=paused;QSettings().setValue("ui/rotateBackgrounds",!paused);updateControls();updateRotation();});
    m_rotation.setObjectName("heroRotation");m_rotation.setInterval(9000);
    connect(&m_rotation,&QTimer::timeout,this,[this]{if(!window()->isMinimized())selectSlide((m_index+1)%slideCount());});
    m_fade.setDuration(650);m_fade.setStartValue(0.0);m_fade.setEndValue(1.0);m_fade.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&m_fade,&QVariantAnimation::valueChanged,this,[this](const QVariant &value){m_opacity=value.toReal();update();});
    connect(&m_fade,&QVariantAnimation::finished,this,[this]{m_previous={};m_previousScaled={};});
    loadSlide(m_index);updateControls();
    for(int i=1;i<slideCount();++i)Ui::loadArt(artwork(i),{1920,1080},this,[](const QPixmap &){});
}
int HomeBanner::slideCount()const{return int(std::size(slides));}
void HomeBanner::selectSlide(int index){
    if(index<0||index>=slideCount())return;
    if(index==m_index){updateControls();return;}
    m_index=index;updateControls();loadSlide(index);updateRotation();
}
void HomeBanner::loadSlide(int index){
    Ui::loadArt(artwork(index),{1920,1080},this,[this,index](const QPixmap &image){
        if(index!=m_index||image.isNull())return;m_fade.stop();m_previous=m_image;m_previousIndex=m_loadedIndex;m_loadedIndex=index;m_image=image;
        m_opacity=m_previous.isNull()||!isVisible()?1:0;cacheImages();if(m_opacity==0)m_fade.start();update();
    });
}
void HomeBanner::updateControls(){
    m_theme->setText(QString::fromLatin1(slides[m_index].title));setProperty("currentSlide",m_index);
    for(int i=0;i<m_dots.size();++i){QSignalBlocker block(m_dots[i]);m_dots[i]->setChecked(i==m_index);}
    m_pause->setIcon(Ui::icon(m_paused?"play":"pause"));m_pause->setToolTip(key(m_paused?"Resume backgrounds":"Pause backgrounds"));m_pause->setAccessibleName(m_pause->toolTip());
}
void HomeBanner::updateRotation(){if(isVisible()&&!m_paused)m_rotation.start();else m_rotation.stop();}
void HomeBanner::cacheImages(){
    const auto target=size()*devicePixelRatioF();
    auto scale=[&](const QPixmap &image){if(image.isNull())return QPixmap();auto p=image.scaled(target,Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);p.setDevicePixelRatio(devicePixelRatioF());return p;};
    m_scaled=scale(m_image);m_previousScaled=m_previous.isNull()?QPixmap():scale(m_previous);
}
void HomeBanner::paintEvent(QPaintEvent *){
    if(m_scaled.isNull())cacheImages();QPainter p(this);p.fillRect(rect(),QColor(24,24,29));
    auto draw=[&](const QPixmap &image,int index){auto s=image.deviceIndependentSize();p.drawPixmap(QPointF((width()-s.width())/2,(height()-s.height())*(index==1?0.0:0.2)),image);};
    if(!m_previousScaled.isNull())draw(m_previousScaled,m_previousIndex);p.setOpacity(m_opacity);draw(m_scaled,m_index);p.setOpacity(1);
    QLinearGradient top(0,0,0,100);top.setColorAt(0,QColor(10,12,20,90));top.setColorAt(1,Qt::transparent);p.fillRect(rect(),top);
    QLinearGradient bottom(0,height()*0.45,0,height());bottom.setColorAt(0,Qt::transparent);bottom.setColorAt(1,QColor(14,16,23,225));p.fillRect(rect(),bottom);
}
void HomeBanner::resizeEvent(QResizeEvent *event){QWidget::resizeEvent(event);cacheImages();m_title->setStyleSheet(width()<800?"font-size:22px;":"font-size:30px;");}
void HomeBanner::showEvent(QShowEvent *event){QWidget::showEvent(event);updateRotation();}
void HomeBanner::hideEvent(QHideEvent *event){m_rotation.stop();m_fade.stop();m_opacity=1;m_previous={};m_previousScaled={};QWidget::hideEvent(event);}
