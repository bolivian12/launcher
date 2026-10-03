#pragma once
#include <QWidget>
#include <QPixmap>
#include <QTimer>
#include <QVariantAnimation>

class QLabel;
class QToolButton;

class HomeBanner : public QWidget {
    Q_OBJECT
public:
    explicit HomeBanner(QWidget *parent=nullptr);
    int currentSlide() const { return m_index; }
    int slideCount() const;
    void selectSlide(int index);
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void showEvent(QShowEvent *) override;
    void hideEvent(QHideEvent *) override;
private:
    void updateControls();
    void updateRotation();
    void cacheImages();
    void loadSlide(int index);
    QTimer m_rotation;
    QVariantAnimation m_fade;
    QPixmap m_image,m_previous,m_scaled,m_previousScaled;
    QLabel *m_theme,*m_title;
    QToolButton *m_pause;
    QList<QToolButton*> m_dots;
    int m_index=0,m_previousIndex=0,m_loadedIndex=0;
    qreal m_opacity=1;
    bool m_paused=false;
};
