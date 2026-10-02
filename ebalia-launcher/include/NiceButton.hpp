#pragma once

#include <QPushButton>
#include <QColor>

// PushButton with animated hover/press color transitions (Qt style sheets
// can't animate — this paints itself and tweens a hover factor).
class NiceButton : public QPushButton {
    Q_OBJECT
    Q_PROPERTY(qreal hover READ hover WRITE setHover)
public:
    enum Role { Primary, Secondary, Outline };

    explicit NiceButton(const QString &text, Role role = Secondary,
                        QWidget *parent = nullptr);

    qreal hover() const { return m_hover; }
    void setHover(qreal h);

    void setRole(Role r) { m_role = r; update(); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void animateTo(qreal target);

    qreal m_hover = 0.0;
    Role m_role;
};
