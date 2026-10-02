#include "TourOverlay.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QSettings>
#include <QResizeEvent>

// Tour steps per language (Settings → Language applies here too)
static QList<QPair<QString, QString>> stepsFor(const QString &lang)
{
    if (lang == QStringLiteral("es")) return {
        { QStringLiteral("Bienvenido a EBALIA"),
          QStringLiteral("Soy Silence — te enseño el launcher. Veinte segundos, no más.") },
        { QStringLiteral("La biblioteca"),
          QStringLiteral("Todas las versiones perdidas viven en la barra lateral: Alpha, Terror y Release. "
                         "Haz clic en una y se carga aquí.") },
        { QStringLiteral("Jugar"),
          QStringLiteral("Elige una versión abajo, pulsa INSTALAR una vez, y luego JUGAR. "
                         "Java y demás se instalan con un clic en SETUP.") },
        { QStringLiteral("Comunidad"),
          QStringLiteral("Mods con sus trailers, servidores, cursos gratis y fan arts — "
                         "todo en la barra lateral.") },
        { QStringLiteral("Membresía"),
          QStringLiteral("Los miembros de Patreon leen el feed en vivo: filtraciones y builds tempranos. "
                         "Vincúlalo cuando quieras desde el chip de arriba a la izquierda. Disfruta.") },
    };
    if (lang == QStringLiteral("pt")) return {
        { QStringLiteral("Bem-vindo ao EBALIA"),
          QStringLiteral("Eu sou o Silence — vou te mostrar o launcher. Vinte segundos, no máximo.") },
        { QStringLiteral("A biblioteca"),
          QStringLiteral("Todas as versões perdidas ficam na barra lateral: Alpha, Terror e Release. "
                         "Clique em uma e ela carrega aqui.") },
        { QStringLiteral("Jogar"),
          QStringLiteral("Escolha uma versão abaixo, aperte INSTALAR uma vez, depois JOGAR. "
                         "Java e o resto se instalam com um clique em SETUP.") },
        { QStringLiteral("Comunidade"),
          QStringLiteral("Mods com trailers, servidores, cursos grátis e fan arts — "
                         "tudo na barra lateral.") },
        { QStringLiteral("Assinatura"),
          QStringLiteral("Membros do Patreon leem o feed ao vivo: vazamentos e builds antecipados. "
                         "Vincule quando quiser pelo chip no canto superior esquerdo. Aproveite.") },
    };
    if (lang == QStringLiteral("fr")) return {
        { QStringLiteral("Bienvenue sur EBALIA"),
          QStringLiteral("Je suis Silence — je te fais visiter. Vingt secondes, pas plus.") },
        { QStringLiteral("La bibliothèque"),
          QStringLiteral("Toutes les versions perdues sont dans la barre latérale : Alpha, Horreur et Release. "
                         "Clique sur l'une d'elles et elle se charge ici.") },
        { QStringLiteral("Jouer"),
          QStringLiteral("Choisis une version en bas, appuie une fois sur INSTALLER, puis JOUER. "
                         "Java et le reste s'installent en un clic dans SETUP.") },
        { QStringLiteral("Communauté"),
          QStringLiteral("Mods avec leurs trailers, serveurs, cours gratuits et fan arts — "
                         "tout est dans la barre latérale.") },
        { QStringLiteral("Abonnement"),
          QStringLiteral("Les membres Patreon lisent le flux en direct : fuites et builds anticipés. "
                         "Lie ton compte via le chip en haut à gauche. Profite bien.") },
    };
    if (lang == QStringLiteral("de")) return {
        { QStringLiteral("Willkommen bei EBALIA"),
          QStringLiteral("Ich bin Silence — ich zeige dir alles. Zwanzig Sekunden, versprochen.") },
        { QStringLiteral("Die Bibliothek"),
          QStringLiteral("Alle verlorenen Versionen leben in der Seitenleiste: Alpha, Horror und Release. "
                         "Klick eine an und sie lädt hier.") },
        { QStringLiteral("Spielen"),
          QStringLiteral("Wähl unten eine Version, drück einmal INSTALLIEREN, dann SPIELEN. "
                         "Java und Co. installierst du mit einem Klick unter SETUP.") },
        { QStringLiteral("Community"),
          QStringLiteral("Mods mit Trailern, Server, Gratiskurse und Fan Arts — "
                         "alles in der Seitenleiste.") },
        { QStringLiteral("Mitgliedschaft"),
          QStringLiteral("Patreon-Mitglieder lesen den Live-Feed: Leaks und frühe Builds. "
                         "Verknüpfe dein Konto über den Chip oben links. Viel Spaß.") },
    };
    return {
        { QStringLiteral("Welcome to EBALIA"),
          QStringLiteral("I'm Silence — I'll show you around. Twenty seconds, tops.") },
        { QStringLiteral("The Library"),
          QStringLiteral("Every lost version lives in the sidebar: Alpha, Horror and Release. "
                         "Click one and it loads here.") },
        { QStringLiteral("Play"),
          QStringLiteral("Pick a version down below, press INSTALL once, then PLAY. "
                         "Java and friends are one click away in SETUP.") },
        { QStringLiteral("Community"),
          QStringLiteral("Mods with their trailers, servers, free courses and fan arts — "
                         "all down the sidebar.") },
        { QStringLiteral("Membership"),
          QStringLiteral("Patreon members read the live news feed: dev leaks and early builds. "
                         "Link it anytime from the top-left chip. Enjoy the depth.") },
    };
}

TourOverlay::TourOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(QStringLiteral(R"(
        TourOverlay { background-color: rgba(8, 8, 8, 170); }
        QFrame#tourCard {
            background-color: #1e1e1e;
            border: 1px solid #3c8527;
            border-radius: 12px;
        }
        QLabel#tourTitle { color: #6fcf5a; font-size: 15px; font-weight: bold; }
        QLabel#tourBody { color: #d0d0d0; font-size: 12px; }
        QLabel#tourCounter { color: #707070; font-size: 10px; }
        QPushButton#tourNext {
            background-color: #44a02d; color: #fff; border: none;
            border-radius: 6px; padding: 8px 22px; font-weight: bold;
        }
        QPushButton#tourNext:hover { background-color: #52b83c; }
        QPushButton#tourSkip {
            background-color: transparent; color: #909090; border: none;
            padding: 8px 14px;
        }
        QPushButton#tourSkip:hover { color: #ffffff; }
    )"));

    QSettings langSettings(QStringLiteral("EBALIA"), QStringLiteral("EBALIA Launcher"));
    m_lang = langSettings.value(QStringLiteral("language"),
                                QStringLiteral("en")).toString();
    m_steps = stepsFor(m_lang);

    // Card
    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("tourCard"));
    m_card->setFixedSize(430, 210);

    auto *lay = new QHBoxLayout(m_card);
    lay->setContentsMargins(16, 14, 16, 14);
    lay->setSpacing(14);

    auto *mascot = new QLabel(m_card);
    mascot->setPixmap(QPixmap(QStringLiteral(":/mascot.png"))
                          .scaled(96, 144, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    mascot->setFixedWidth(96);
    mascot->setAlignment(Qt::AlignCenter);
    lay->addWidget(mascot, 0, Qt::AlignVCenter);

    auto *textCol = new QVBoxLayout;
    textCol->setSpacing(6);

    m_title = new QLabel(m_card);
    m_title->setObjectName(QStringLiteral("tourTitle"));
    textCol->addWidget(m_title);

    m_body = new QLabel(m_card);
    m_body->setObjectName(QStringLiteral("tourBody"));
    m_body->setWordWrap(true);
    textCol->addWidget(m_body, 1);

    auto *btnRow = new QHBoxLayout;
    m_counter = new QLabel(m_card);
    m_counter->setObjectName(QStringLiteral("tourCounter"));
    btnRow->addWidget(m_counter);
    btnRow->addStretch(1);

    auto *skip = new QPushButton(QStringLiteral("Skip"), m_card);
    skip->setObjectName(QStringLiteral("tourSkip"));
    skip->setCursor(Qt::PointingHandCursor);
    m_skipBtn = skip;
    btnRow->addWidget(skip);

    m_nextBtn = new QPushButton(QStringLiteral("Next"), m_card);
    m_nextBtn->setObjectName(QStringLiteral("tourNext"));
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    btnRow->addWidget(m_nextBtn);
    textCol->addLayout(btnRow);

    lay->addLayout(textCol, 1);

    connect(skip, &QPushButton::clicked, this, &TourOverlay::finish);
    connect(m_nextBtn, &QPushButton::clicked, this, [this] {
        ++m_idx;
        if (m_idx >= m_steps.size()) finish();
        else showStep();
    });

    parent->installEventFilter(this);
    hide();
}

void TourOverlay::start()
{
    m_idx = 0;
    if (parentWidget())
        setGeometry(parentWidget()->rect());
    show();
    raise();
    showStep(); // after geometry — card positions depend on width/height
}

void TourOverlay::showStep()
{
    m_title->setText(m_steps[m_idx].first);
    m_body->setText(m_steps[m_idx].second);
    m_counter->setText(QStringLiteral("%1 / %2").arg(m_idx + 1).arg(m_steps.size()));

    QString skip = QStringLiteral("Skip"), next = QStringLiteral("Next"),
            done = QStringLiteral("Done");
    if (m_lang == QStringLiteral("es"))      { skip = QStringLiteral("Omitir");       next = QStringLiteral("Siguiente"); done = QStringLiteral("Listo"); }
    else if (m_lang == QStringLiteral("pt")) { skip = QStringLiteral("Pular");        next = QStringLiteral("Próximo");   done = QStringLiteral("Concluir"); }
    else if (m_lang == QStringLiteral("fr")) { skip = QStringLiteral("Passer");       next = QStringLiteral("Suivant");   done = QStringLiteral("Terminé"); }
    else if (m_lang == QStringLiteral("de")) { skip = QStringLiteral("Überspringen"); next = QStringLiteral("Weiter");    done = QStringLiteral("Fertig"); }

    m_skipBtn->setText(skip);
    m_nextBtn->setText(m_idx == m_steps.size() - 1 ? done : next);
    emit stepChanged(m_idx);

    // Card position varies per step so it never covers what it describes —
    // always fully inside the overlay
    const int maxX = qMax(12, width() - m_card->width() - 12);
    const int maxY = qMax(12, height() - m_card->height() - 12);
    const int cx = (width() - m_card->width()) / 2;
    const int cy = (height() - m_card->height()) / 2;

    int x = cx, y = cy;
    if (m_idx == 1 || m_idx == 3)
        x = qMin(maxX, 260);                      // sidebar steps → shift right
    else if (m_idx == 2)
        y = qMax(12, height() - m_card->height() - 170); // above bottom controls

    m_card->move(qBound(12, x, maxX), qBound(12, y, maxY));
}

void TourOverlay::finish()
{
    QSettings s(QStringLiteral("EBALIA"), QStringLiteral("EBALIA Launcher"));
    s.setValue(QStringLiteral("tour/done"), true);
    hide();
    deleteLater();
}

bool TourOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parentWidget() && event->type() == QEvent::Resize)
        setGeometry(parentWidget()->rect());
    return QWidget::eventFilter(watched, event);
}
