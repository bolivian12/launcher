#include "SplashScreen.hpp"

#include <QApplication>
#include <QScreen>
#include <QLabel>
#include <QProgressBar>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <QPixmap>
#include <QImage>
#include <QVector>
#include <QtMath>
#include <random>

// Startup splash: EBALIA letters drop in, and the lost-version mobs walk
// across a pixel grass field — Silence included.
SplashScreen::SplashScreen(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::SplashScreen | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(520, 340);

    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("splashRoot"));
    root->setGeometry(0, 0, width(), height());
    root->setStyleSheet(QStringLiteral(R"(
        QWidget#splashRoot {
            background-color: #0d0f0c;
            border: 1px solid #2a3324;
            border-radius: 14px;
        }
        QLabel#splashLetter {
            color: #ffffff;
            font-size: 32px;
            font-weight: bold;
        }
        QLabel#splashSub {
            color: #7fa860;
            font-size: 10px;
            letter-spacing: 7px;
        }
        QLabel#splashVersion { color: #4a5544; font-size: 9px; }
        QProgressBar#splashProgress {
            background-color: #1c211a;
            border: none;
        }
        QProgressBar#splashProgress::chunk { background-color: #57a83b; }
    )"));

    // ── Logo glyph, soft pulse ──
    auto *logo = new QLabel(root);
    logo->setPixmap(QPixmap(QStringLiteral(":/ebalia.png"))
                        .scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignCenter);
    const QRect logoFinal((width() - 44) / 2, 26, 44, 44);
    logo->setGeometry(logoFinal);
    auto *logoFx = new QGraphicsOpacityEffect(logo);
    logoFx->setOpacity(0.0);
    logo->setGraphicsEffect(logoFx);
    logo->show();

    auto *logoFade = new QPropertyAnimation(logoFx, "opacity", this);
    logoFade->setDuration(600);
    logoFade->setStartValue(0.0);
    logoFade->setEndValue(1.0);
    logoFade->start(QAbstractAnimation::DeleteWhenStopped);

    auto *pulse = new QPropertyAnimation(logo, "geometry", this);
    pulse->setDuration(1400);
    pulse->setStartValue(logoFinal);
    pulse->setKeyValueAt(0.5, logoFinal.adjusted(-2, -2, 2, 2));
    pulse->setEndValue(logoFinal);
    pulse->setEasingCurve(QEasingCurve::InOutSine);
    pulse->setLoopCount(-1);
    pulse->start();

    // ── Letters drop with a small bounce ──
    const QString name = QStringLiteral("EBALIA");
    const int lw = 34, lh = 44;
    const int startX = (width() - name.size() * lw) / 2;
    const int finalY = 82;

    for (int i = 0; i < name.size(); ++i) {
        auto *letter = new QLabel(QString(name[i]), root);
        letter->setObjectName(QStringLiteral("splashLetter"));
        letter->setAlignment(Qt::AlignCenter);
        letter->setGeometry(startX + i * lw, finalY - 46, lw, lh);
        auto *fx = new QGraphicsOpacityEffect(letter);
        fx->setOpacity(0.0);
        letter->setGraphicsEffect(fx);
        letter->show();

        QTimer::singleShot(250 + i * 80, this,
                           [this, letter, fx, startX, finalY, i, lw, lh] {
            auto *fade = new QPropertyAnimation(fx, "opacity", this);
            fade->setDuration(380);
            fade->setStartValue(0.0);
            fade->setEndValue(1.0);
            fade->start(QAbstractAnimation::DeleteWhenStopped);

            auto *drop = new QPropertyAnimation(letter, "geometry", this);
            drop->setDuration(500);
            drop->setStartValue(QRect(startX + i * lw, finalY - 46, lw, lh));
            drop->setEndValue(QRect(startX + i * lw, finalY, lw, lh));
            drop->setEasingCurve(QEasingCurve::OutBack);
            drop->start(QAbstractAnimation::DeleteWhenStopped);
        });
    }

    auto *sub = new QLabel(QStringLiteral("LOST VERSIONS"), root);
    sub->setObjectName(QStringLiteral("splashSub"));
    sub->setAlignment(Qt::AlignCenter);
    sub->setGeometry(0, 134, width(), 16);
    auto *subFx = new QGraphicsOpacityEffect(sub);
    subFx->setOpacity(0.0);
    sub->setGraphicsEffect(subFx);

    QTimer::singleShot(850, this, [this, subFx] {
        auto *fade = new QPropertyAnimation(subFx, "opacity", this);
        fade->setDuration(500);
        fade->setStartValue(0.0);
        fade->setEndValue(1.0);
        fade->start(QAbstractAnimation::DeleteWhenStopped);
    });

    // ── Pixel grass field across the bottom ──
    const int groundH = 30;
    const int groundY = height() - groundH;
    {
        QImage ground(width(), groundH, QImage::Format_RGB32);
        std::mt19937 rng(0xEBA1);
        static const QVector<QRgb> grass = { 0x6aae3f, 0x7cbd4b, 0x5e9c3c, 0x86c957 };
        static const QVector<QRgb> dirt  = { 0x7a5a34, 0x8a683c, 0x6b4d2b, 0x5d4125 };
        for (int y = 0; y < groundH; ++y)
            for (int x = 0; x < width(); ++x) {
                const bool isGrass = y < 4 + int(rng() % 3);
                ground.setPixel(x, y, isGrass ? grass[rng() % grass.size()]
                                              : dirt[rng() % dirt.size()]);
            }
        auto *groundLbl = new QLabel(root);
        groundLbl->setGeometry(1, groundY, width() - 2, groundH);
        groundLbl->setPixmap(QPixmap::fromImage(ground));
        groundLbl->show();
    }

    // ── Mobs walking the field ──
    struct Mob {
        QLabel *lbl;
        double x, speed, phase;
        int w, h;
    };
    auto *mobs = new QVector<Mob>();

    const struct { const char *res; double speed; int h; } mobDefs[] = {
        { ":/art/creeper.png",  1.7, 86 },
        { ":/art/zombie.png",   1.2, 90 },
        { ":/mascot.png",       0.9, 96 },   // The Silence itself
        { ":/art/skeleton.png", 2.2, 82 },
        { ":/art/enderman.png", 0.7, 94 },
    };
    int idx = 0;
    for (const auto &md : mobDefs) {
        QPixmap p(QString::fromLatin1(md.res));
        if (p.isNull()) { ++idx; continue; }
        const int h = md.h;
        const int w = p.width() * h / p.height();

        auto *lbl = new QLabel(root);
        lbl->setPixmap(p.scaledToHeight(h, Qt::SmoothTransformation));
        lbl->setFixedSize(w, h);
        lbl->show();

        mobs->append({ lbl, 14.0 - idx * 95.0, md.speed, idx * 1.7, w, h });
        ++idx;
    }

    auto *walker = new QTimer(this);
    connect(walker, &QTimer::timeout, this, [this, mobs, groundY] {
        for (auto &m : *mobs) {
            m.x += m.speed;
            if (m.x > width() + 70)
                m.x = -m.w - 50;
            const int bob = int(qAbs(qSin(m.phase)) * 5.0);
            m.phase += 0.18;
            m.lbl->move(int(m.x), groundY - m.h - bob + 4); // feet on the grass
        }
    });
    walker->start(33);

    // ── Progress just above the field ──
    m_progress = new QProgressBar(root);
    m_progress->setObjectName(QStringLiteral("splashProgress"));
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setGeometry(70, 172, width() - 140, 3);

    auto *ver = new QLabel(QStringLiteral("v3.0"), root);
    ver->setObjectName(QStringLiteral("splashVersion"));
    ver->setAlignment(Qt::AlignCenter);
    ver->setGeometry(0, 184, width(), 14);

    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect g = screen->availableGeometry();
        move(g.center() - rect().center());
    }

    auto *fill = new QPropertyAnimation(m_progress, "value", this);
    fill->setDuration(2500);
    fill->setStartValue(0);
    fill->setEndValue(100);
    fill->setEasingCurve(QEasingCurve::InOutQuad);
    fill->start(QAbstractAnimation::DeleteWhenStopped);

    connect(fill, &QPropertyAnimation::finished, this, [this] {
        auto *out = new QPropertyAnimation(this, "windowOpacity", this);
        out->setDuration(350);
        out->setStartValue(1.0);
        out->setEndValue(0.0);
        out->setEasingCurve(QEasingCurve::InOutQuad);
        out->start(QAbstractAnimation::DeleteWhenStopped);
        connect(out, &QPropertyAnimation::finished, this, [this] {
            emit finished();
            close();
        });
    });
}
