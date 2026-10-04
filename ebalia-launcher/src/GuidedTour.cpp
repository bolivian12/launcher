#include "GuidedTour.hpp"
#include "MainWindow.hpp"
#include "Language.hpp"
#include <QtWidgets>
namespace {
struct Step {int page;const char *target,*esTitle,*enTitle,*esBody,*enBody;};
const QList<Step> steps{
 {MainWindow::Home,"homePlay","Bienvenido a EBALIA","Welcome to EBALIA","Vamos a recorrer el launcher paso a paso. Usá Siguiente y Atrás; podés omitir la guía y volver a abrirla cuando quieras.","Let's explore the launcher step by step. Use Next and Back; you can skip and reopen this tour whenever you like."},
 {MainWindow::Home,"accountButton","Tu perfil y tus cuentas","Your profile and accounts","Desde tu avatar podés agregar una cuenta Microsoft o un perfil local y elegir la cuenta activa. Las licencias de cada edición del juego se verifican por separado.","Use your avatar to add a Microsoft account or a local profile and choose the active account. Each game edition has its own license requirements."},
 {MainWindow::Home,"instancePicker","Creá tu primera instancia","Create your first instance","Este selector permite crear una instancia y elegir cuál jugar. Una instancia mantiene su propia versión de Minecraft, mods y ajustes.","This selector lets you create an instance and choose which one to play. Each instance keeps its own Minecraft version, mods and settings."},
 {MainWindow::Instances,"navigation","Tu biblioteca","Your library","Acá se organizan las instancias. Abrí una para jugar, administrar sus mods, revisar registros o cambiar su configuración. Tus mundos permanecen en su instancia.","Organize instances here. Open one to play, manage mods, inspect logs or change its settings. Worlds remain inside their instance."},
 {MainWindow::Explore,"modResults","Descubrí mods y modpacks","Discover mods and modpacks","Buscá contenido en las plataformas disponibles. Elegí la instancia de destino y revisá la versión de Minecraft, el loader y las dependencias antes de instalar.","Search available platforms. Select the target instance and review its Minecraft version, loader and dependencies before installing."},
 {MainWindow::Lost,"lostDetail","Lost Versions","Lost Versions","Cada versión perdida tiene una instalación independiente. Podés instalar, jugar, abrir su carpeta o desinstalarla. Desinstalar mueve esa carpeta a la papelera, incluidos sus mundos: revisá la confirmación.","Each lost version has a separate installation. Install, play, open its folder or uninstall it. Uninstalling moves that folder, including its worlds, to the trash: review the confirmation."},
 {MainWindow::Packs,"navigation","Tus packs reutilizables","Reusable packs","Guardá y aplicá selecciones de mods para reutilizarlas en otras instancias compatibles.","Save and apply mod selections to reuse them in other compatible instances."},
 {MainWindow::Skins,"navigation","Personalizá tu aspecto","Customize your appearance","Administrá tus skins desde esta sección. Su uso dentro del juego depende del tipo de cuenta y de la compatibilidad del servidor.","Manage skins in this section. Their appearance in-game depends on your account type and server support."},
 {MainWindow::News,"navigation","Noticias y Patreon","News and Patreon","Alterná entre noticias de Minecraft y EBALIA. Las publicaciones públicas están disponibles para todos; el contenido de pago requiere vincular una suscripción válida de Patreon.","Switch between Minecraft and EBALIA news. Public posts are available to everyone; paid content requires linking a valid Patreon membership."},
 {MainWindow::Community,"navigation","La comunidad y Mis Mods","Community and My Mods","Encontrá los enlaces de EBALIA y sus creaciones. Desde Inicio también podés abrir Mis Mods para visitar las páginas de los proyectos.","Find EBALIA links and creations. You can also open My Mods from Home to visit project pages."},
 {MainWindow::Bedrock,"bedrockProviderCard","Bedrock · Beta","Bedrock · Beta","El botón instala un gestor externo. En Windows usa la licencia de Minecraft para Windows; en Linux y macOS usa Android y requiere Google Play. La compra de Windows no habilita Android.","The button installs an external manager. Windows uses a Minecraft for Windows license; Linux and macOS use Android and require Google Play. A Windows purchase does not unlock Android."},
 {MainWindow::Settings,"navSettings","Ajustalo a tu gusto","Make it yours","Configurá el idioma, Java y los recursos del juego. Si una instancia no abre, revisá su registro antes de cambiar los ajustes.","Configure language, Java and game resources. If an instance won't open, check its log before changing settings."},
 {MainWindow::Home,"launcherUpdateAvailable","Actualizaciones sin salir a la web","Updates without visiting the website","Cuando haya una versión nueva verás Actualizar y reiniciar. Primero cerrá las partidas y esperá las instalaciones. Se reemplazan los archivos del launcher; se conservan instancias, Lost Versions y descargas.","When a new release is available, choose Update and restart. Close games and wait for installations first. Launcher files are replaced; instances, Lost Versions and downloads are preserved."},
 {MainWindow::Guide,"navigation","Todo listo","You're ready","Esta guía siempre se puede repetir desde Guía y tutorial. ¡Ahora creá tu primera instancia y empezá a explorar!","You can repeat this tour from Guide & tutorial at any time. Create your first instance and start exploring!"}
};
QString words(const char *es,const char *en){return QString::fromUtf8(Language::current=="es"?es:en);}
}
GuidedTour::GuidedTour(QWidget *parent,std::function<void(int)> navigate):QWidget(parent),m_navigate(std::move(navigate)) {
 setObjectName("interactiveTour");setAttribute(Qt::WA_DeleteOnClose);setFocusPolicy(Qt::StrongFocus);
 parent->installEventFilter(this);setGeometry(parent->rect());
 m_card=new QFrame(this);m_card->setObjectName("tourCard");m_card->setStyleSheet("#tourCard{background:#20251f;border:1px solid #9ad56c;border-radius:16px;} QLabel{color:#f4f6f0;} QPushButton{padding:9px 14px;}");
 auto layout=new QVBoxLayout(m_card);layout->setContentsMargins(22,18,22,18);layout->setSpacing(12);
 m_count=new QLabel; m_count->setObjectName("tourStepCount");layout->addWidget(m_count);
 m_title=new QLabel;m_title->setObjectName("tourTitle");m_title->setWordWrap(true);m_title->setStyleSheet("font-size:22px;font-weight:700;");layout->addWidget(m_title);
 m_body=new QLabel;m_body->setObjectName("tourBody");m_body->setWordWrap(true);m_body->setTextFormat(Qt::PlainText);layout->addWidget(m_body);
 m_progress=new QProgressBar;m_progress->setRange(1,steps.size());m_progress->setTextVisible(false);m_progress->setFixedHeight(6);layout->addWidget(m_progress);
 auto row=new QHBoxLayout;layout->addLayout(row);
 auto skip=new QPushButton(words("Omitir","Skip"));skip->setObjectName("tourSkip");row->addWidget(skip);row->addStretch();
 m_previous=new QPushButton(words("Atrás","Back"));m_previous->setObjectName("tourBack");row->addWidget(m_previous);
 m_next=new QPushButton;m_next->setObjectName("tourNext");m_next->setProperty("primary",true);row->addWidget(m_next);
 connect(skip,&QPushButton::clicked,this,[this]{finish();});
 connect(m_previous,&QPushButton::clicked,this,[this]{if(m_step>0){--m_step;display();}});
 connect(m_next,&QPushButton::clicked,this,[this]{if(m_step+1==steps.size())finish();else{++m_step;display();}});
 auto escape=new QShortcut(QKeySequence(Qt::Key_Escape),this);escape->setContext(Qt::WidgetWithChildrenShortcut);connect(escape,&QShortcut::activated,this,[this]{finish();});
 display();show();raise();m_next->setFocus();
}
void GuidedTour::display() {
 const auto &step=steps[m_step];m_navigate(step.page);
 m_title->setText(words(step.esTitle,step.enTitle));m_body->setText(words(step.esBody,step.enBody));
 m_count->setText(words("DESCUBRÍ EBALIA · %1 de %2","DISCOVER EBALIA · %1 of %2").arg(m_step+1).arg(steps.size()));
 m_previous->setEnabled(m_step>0);m_next->setText(m_step+1==steps.size()?words("Empezar","Get started"):words("Siguiente","Next"));m_progress->setValue(m_step+1);
 place();QTimer::singleShot(0,this,[this]{place();});
}
void GuidedTour::place() {
 setGeometry(parentWidget()->rect());m_highlight={};
 if(auto target=parentWidget()->findChild<QWidget*>(steps[m_step].target);target&&target->isVisible()) {
  m_highlight=QRect(target->mapTo(this,QPoint(0,0)),target->size()).intersected(rect()).adjusted(-4,-4,4,4);
 }
 m_card->setFixedWidth(qMin(530,width()-32));m_card->adjustSize();
 int y=height()-m_card->height()-24;
 if(m_highlight.isValid()&&m_highlight.center().y()>height()/2)y=24;
 m_card->move((width()-m_card->width())/2,qMax(12,y));raise();update();
}
bool GuidedTour::eventFilter(QObject *watched,QEvent *event){if(watched==parentWidget()&&event->type()==QEvent::Resize)place();return QWidget::eventFilter(watched,event);}
void GuidedTour::paintEvent(QPaintEvent *) {
 QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);QPainterPath shade;shade.addRect(rect());
 if(m_highlight.isValid()){QPainterPath hole;hole.addRoundedRect(m_highlight,8,8);shade=shade.subtracted(hole);}
 painter.fillPath(shade,QColor(0,0,0,175));
 if(m_highlight.isValid()){painter.setPen(QPen(QColor("#b4ed80"),2));painter.setBrush(Qt::NoBrush);painter.drawRoundedRect(m_highlight,8,8);}
}
void GuidedTour::finish(){QSettings().setValue("ui/tutorialSeen",true);m_navigate(MainWindow::Home);close();}
