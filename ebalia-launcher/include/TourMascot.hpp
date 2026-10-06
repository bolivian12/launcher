#pragma once
#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <QElapsedTimer>
// Animated Silence, the guide's mascot, drawn from a pixel-art sprite sheet
// (resources/mascot/silence-sheet.png): one pose per mood, each with talking
// and blinking frames. Idle float and breathing, glowing eyes, a swaying
// knife, a hop and a few words on every new step, and a lean toward what the
// guide is pointing at.
class TourMascot:public QWidget {
public:
 enum Mood{Idle,Happy,Surprised,Wink,Point,Cheer,Think,MoodCount};
 explicit TourMascot(QWidget *parent=nullptr);
 // Hop, wave and talk, e.g. when the guide moves to another step.
 void react();
 void react(Mood mood){setMood(mood);react();}
 void setMood(Mood mood){m_mood=mood;update();}
 Mood mood() const {return m_mood;}
 // Lean toward a point in global coordinates; a null point stands upright.
 void lookAt(QPoint target);
 QSize sizeHint() const override {return {118,170};}
protected:
 void paintEvent(QPaintEvent *event) override;
 void showEvent(QShowEvent *event) override;
 void hideEvent(QHideEvent *event) override;
 void mousePressEvent(QMouseEvent *event) override;
private:
 QPixmap m_sheet;
 Mood m_mood=Idle;
 QTimer m_frame;
 QElapsedTimer m_clock;
 qint64 m_hop=-100000,m_shown=0;
 double m_lean=0,m_leanTarget=0;
};
