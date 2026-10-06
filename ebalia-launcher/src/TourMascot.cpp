#include "TourMascot.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QtMath>
#include <cmath>
namespace {
// Sprite sheet layout: 42×58 frames, three per mood (normal, talking, blinking).
constexpr int frameWidth=42,frameHeight=58,upscale=4;
constexpr double bodyCenter=20.0/frameWidth;
// Regions in sprite pixels.
const QRectF knife{0,24,9,23};
const QPointF knifePivot{9,38};
const QPointF eyeCenters[]{{14.5,13.5},{23.5,13.5}};
// Which eyes glow in each mood (bit 0 left, bit 1 right) and where they look.
const int glowingEyes[]{3,0,3,2,3,0,3};
const QPointF gaze[]{{0,0},{0,0},{0,-0.5},{0,0},{0,0},{0,0},{1,-1}};
}
TourMascot::TourMascot(QWidget *parent):QWidget(parent) {
 // Enlarge the pixel art without blurring; later smooth scaling stays crisp.
 const QImage sheet(QStringLiteral(":/mascot/silence-sheet.png"));
 m_sheet=QPixmap::fromImage(sheet.scaled(sheet.size()*upscale,Qt::IgnoreAspectRatio,Qt::FastTransformation));
 setObjectName("tourMascot");setMinimumSize(78,112);setCursor(Qt::PointingHandCursor);
 setAccessibleName("Silence");
 m_frame.setInterval(16);connect(&m_frame,&QTimer::timeout,this,qOverload<>(&QWidget::update));
 m_clock.start();
}
void TourMascot::react(){m_hop=m_clock.elapsed();}
void TourMascot::lookAt(QPoint target) {
 if(target.isNull()){m_leanTarget=0;return;}
 const double dx=target.x()-mapToGlobal(rect().center()).x();
 m_leanTarget=qBound(-7.0,dx/70.0,7.0);
}
void TourMascot::showEvent(QShowEvent *event){m_shown=m_clock.elapsed();m_frame.start();QWidget::showEvent(event);}
void TourMascot::hideEvent(QHideEvent *event){m_frame.stop();QWidget::hideEvent(event);}
void TourMascot::mousePressEvent(QMouseEvent *event){react();event->accept();}
void TourMascot::paintEvent(QPaintEvent *) {
 if(m_sheet.isNull())return;
 QPainter p(this);p.setRenderHints(QPainter::Antialiasing|QPainter::SmoothPixmapTransform);
 const qint64 now=m_clock.elapsed();const double t=now/1000.0;
 const double appear=qBound(0.0,(now-m_shown)/450.0,1.0),ease=1-std::pow(1-appear,3);
 // Hop: crouch, jump with a stretch, squash on landing.
 const double h=(now-m_hop)/1000.0;double lift=0,squash=0;
 if(h>=0&&h<0.12)squash=std::sin(M_PI*h/0.12)*0.07;
 else if(h>=0.12&&h<0.62){const double k=std::sin(M_PI*(h-0.12)/0.5);lift=k*22;squash=-k*0.05;}
 else if(h>=0.62&&h<0.82)squash=std::sin(M_PI*(h-0.62)/0.2)*0.09;
 const double bob=std::sin(t*2.4)*3;squash+=std::sin(t*2.4+1.2)*0.015;
 m_lean+=(m_leanTarget-m_lean)*0.08;
 // Frame: blink now and then, move the jaw for a moment after each reaction.
 const double cycle=std::fmod(t,4.2);
 const bool blink=cycle<0.13||(int(t/4.2)%3==2&&cycle>0.26&&cycle<0.38);
 const bool talking=h>=0.1&&h<1.5&&int((h-0.1)/0.11)%2==0;
 const int mood=qBound(0,int(m_mood),MoodCount-1);
 const int frame=mood*3+(blink?2:talking?1:0);
 const QRect source(frame*frameWidth*upscale,0,frameWidth*upscale,frameHeight*upscale);
 // Size: leave room for the hop above and the shadow below.
 double sh=height()-34,sw=sh*frameWidth/frameHeight;
 if(sw>width()-8){sw=width()-8;sh=sw*frameHeight/frameWidth;}
 const double unit=sh/frameHeight;
 const QPointF feet(width()/2.0,height()-10.0);
 const double air=qBound(0.0,lift/30.0,1.0);
 p.setPen(Qt::NoPen);p.setBrush(QColor(0,0,0,int(110*(1-air*0.6)*ease)));
 p.drawEllipse(feet,sw*0.3*(1-air*0.35),4.0*(1-air*0.3));
 p.setOpacity(ease);
 p.translate(feet.x(),feet.y()-std::max(0.0,bob)-lift+(1-ease)*24);
 p.rotate(m_lean+std::sin(t*1.3)*1.2);p.scale(1+squash*0.6,1-squash);
 const QRectF body(-sw*bodyCenter,-sh,sw,sh);
 auto map=[&](QPointF f){return QPointF(body.left()+f.x()*unit,body.top()+f.y()*unit);};
 const QRectF knifeRect(map(knife.topLeft()),map(knife.bottomRight()));
 QPainterPath still;still.addRect(body.adjusted(-2,-2,2,2));QPainterPath cut;cut.addRect(knifeRect);
 p.save();p.setClipPath(still.subtracted(cut));p.drawPixmap(body,m_sheet,source);p.restore();
 // The knife sways and waves after each hop.
 double swing=std::sin(t*3.0)*5;
 if(h>=0&&h<1.1)swing+=std::sin(h*16)*11*(1-h/1.1);
 const QPointF pivot=map(knifePivot);
 p.save();p.translate(pivot);p.rotate(-swing);p.translate(-pivot);p.setClipRect(knifeRect);p.drawPixmap(body,m_sheet,source);p.restore();
 if(blink)return;
 for(int eye=0;eye<2;++eye) {
  if(!(glowingEyes[mood]&(1<<eye)))continue;
  const QPointF center=map(eyeCenters[eye]+gaze[mood]);const double radius=unit*3.6;
  QRadialGradient glow(center,radius);
  glow.setColorAt(0,QColor(80,230,255,int(60+40*std::sin(t*3.2))));glow.setColorAt(1,QColor(80,230,255,0));
  p.setBrush(glow);p.drawEllipse(center,radius,radius);
 }
}
