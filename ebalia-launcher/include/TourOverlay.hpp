#pragma once

#include <QWidget>
#include <QStringList>

class QFrame;
class QLabel;
class QPushButton;

// First-run guided tour: a dim overlay with The Silence as the mascot,
// a card per step, NEXT / SKIP. Marks itself done in QSettings.
class TourOverlay : public QWidget {
    Q_OBJECT
public:
    explicit TourOverlay(QWidget *parent = nullptr);

    void start();

signals:
    void stepChanged(int index);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void showStep();
    void finish();

    QFrame *m_card;
    QLabel *m_title;
    QLabel *m_body;
    QLabel *m_counter;
    QPushButton *m_nextBtn;
    QPushButton *m_skipBtn;
    QString m_lang;
    int m_idx = 0;

    QList<QPair<QString, QString>> m_steps;
};
