#include "CreatorDialog.hpp"
#include "Language.hpp"
#include "Ui.hpp"
#include <QtWidgets>

namespace {
QString t(const char *text){return Language::key(QString::fromUtf8(text));}
QLabel *copy(const QString &text,QVBoxLayout *layout,const char *name){auto label=new QLabel(text);label->setWordWrap(true);label->setObjectName(name);layout->addWidget(label);return label;}
QPushButton *link(const QString &text,const QString &url,QBoxLayout *layout,const char *name,bool patreon=false){
    auto button=new QPushButton(text);button->setObjectName(name);button->setIcon(Ui::icon("external-link",Qt::white));button->setCursor(Qt::PointingHandCursor);button->setProperty("destination",url);if(patreon)button->setProperty("patreon",true);layout->addWidget(button);
    QObject::connect(button,&QPushButton::clicked,button,[url]{QDesktopServices::openUrl(QUrl(url));});return button;
}
}
QString CreatorMod::url() const { return "https://www.curseforge.com/minecraft/mc-mods/" + slug; }
QList<CreatorMod> creatorMods(){
    // Dark artwork that suits horror mods.
    return {{"ooo.jar","ooo-jar","An unsettling adventure hidden in your Minecraft world.",false,":/art/backgrounds/nether.png"},
            {"In Your World","in-your-world","Something in your world is not quite right.",false,":/art/backgrounds/warden.png"},
            {"Secret 01","secret-01-proyect","A new mystery by EBALIA. Coming soon.",true,":/art/backgrounds/mineshaft.png"}};
}
CreatorDialog::CreatorDialog(bool patreonOnly,QWidget *parent):QDialog(parent){
    setObjectName(patreonOnly?"patreonInvite":"myModsDialog");setWindowTitle(patreonOnly?"EBALIA · Patreon":QString("EBALIA Mods"));setMinimumSize(480,400);
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(20,20,20,16);layout->setSpacing(14);
    auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);layout->addWidget(scroll,1);
    auto content=new QWidget;auto body=new QVBoxLayout(content);body->setContentsMargins(0,0,10,0);body->setSpacing(14);scroll->setWidget(content);
    copy("EBALIA / CREATOR COLLECTION",body,"archiveEyebrow");copy(patreonOnly?t("Enter the world of EBALIA"):QString("EBALIA Mods"),body,"pageTitle");
    copy(t(patreonOnly?"Support new creations and discover exclusive content on Patreon.":"Stories, strange worlds and creations by EBALIA."),body,"pageSubtitle");
    if(!patreonOnly){
        for(const auto &mod:creatorMods()){
            auto card=new QFrame;card->setObjectName("creatorModCard");body->addWidget(card);auto row=new QHBoxLayout(card);row->setContentsMargins(14,14,14,14);row->setSpacing(16);
            auto icon=new QLabel;icon->setFixedSize(76,76);icon->setAlignment(Qt::AlignCenter);row->addWidget(icon,0,Qt::AlignTop);
            Ui::loadArt(":/creations/"+mod.slug+".png",{96,96},icon,[icon](const QPixmap &p){icon->setPixmap(p.scaled(76,76,Qt::KeepAspectRatio,Qt::SmoothTransformation));});
            auto details=new QVBoxLayout;details->setSpacing(8);row->addLayout(details,1);copy(mod.name+(mod.soon?"  ·  SOON":QString()),details,"sectionTitle");copy(t(mod.description.toUtf8().constData()),details,"muted");
            auto action=link(t(mod.soon?"View project":"Open on CurseForge"),mod.url(),details,"creatorModLink");action->setProperty("soon",mod.soon);action->setProperty("project",mod.slug);
        }
    }
    auto patron=new QFrame;patron->setObjectName("creatorPatreonCard");body->addWidget(patron);auto pl=new QVBoxLayout(patron);pl->setContentsMargins(20,18,20,18);pl->setSpacing(12);
    copy("EBALIA · PATREON",pl,"archiveEyebrow");copy(t("Be part of the next creation"),pl,"sectionTitle");
    copy(t("Exclusive creations, mod previews and early access, according to your Patreon membership."),pl,"pageSubtitle");
    link(t("Join EBALIA on Patreon"),"https://www.patreon.com/EBALIA",pl,"joinPatreon",true);
    auto member=link(t("Already a member? View exclusive posts"),"https://www.patreon.com/c/ebalia/posts",pl,"patreonContent");member->setProperty("link",true);
    body->addStretch();
    auto footer=new QHBoxLayout;layout->addLayout(footer);
    if(patreonOnly){auto never=new QCheckBox(t("Do not show this invitation again"));never->setObjectName("hidePatreonInvite");never->setChecked(QSettings().value("ui/hidePatreonInvite",false).toBool());footer->addWidget(never);connect(never,&QCheckBox::toggled,this,[](bool checked){QSettings().setValue("ui/hidePatreonInvite",checked);});}
    footer->addStretch();auto close=new QPushButton(t("Close"));close->setObjectName("closeCreatorDialog");footer->addWidget(close);connect(close,&QPushButton::clicked,this,&QDialog::accept);
    Ui::fitToScreen(this,patreonOnly?QSize(580,510):QSize(700,730));
}
