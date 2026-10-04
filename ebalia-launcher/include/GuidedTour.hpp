#pragma once
#include <QWidget>
#include <functional>
class QLabel;
class QPushButton;
class QProgressBar;
class QFrame;
class GuidedTour:public QWidget {
public:
 GuidedTour(QWidget *parent,std::function<void(int)> navigate);
protected:
 bool eventFilter(QObject *watched,QEvent *event) override;
 void paintEvent(QPaintEvent *event) override;
private:
 void display();
 void place();
 void finish();
 std::function<void(int)> m_navigate;
 int m_step=0;
 QFrame *m_card;
 QLabel *m_title,*m_body,*m_count;
 QPushButton *m_previous,*m_next;
 QProgressBar *m_progress;
 QRect m_highlight;
};
