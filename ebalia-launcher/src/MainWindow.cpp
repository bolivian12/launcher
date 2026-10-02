#include "MainWindow.hpp"
#include "CommunityPage.hpp"
#include "PatreonAuth.hpp"
#include "ModRepository.hpp"
#include "Language.hpp"
#include "Archive.hpp"
#include "LostInstaller.hpp"
#include "Loaders.hpp"
#include "SkinsPage.hpp"
#include "VersionManager.hpp"
#include "AccountManager.hpp"
#include "MsAuth.hpp"
#include "JavaRunner.hpp"
#include "JavaRuntime.hpp"
#include "SetupDialog.hpp"
#include "CreateInstanceDialog.hpp"
#include "PackService.hpp"
#include <QtWidgets>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QCryptographicHash>
#include <QTemporaryDir>
#include <stdexcept>
using Language::text;
namespace {
class GameBanner : public QWidget {
public:
    explicit GameBanner(QWidget *parent=nullptr):QWidget(parent),m_image(":/art/f1_2.jpg"){setMinimumHeight(220);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);}
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.drawPixmap((width()-m_scaled.width())/2,(height()-m_scaled.height())/2,m_scaled);
        QLinearGradient shade(0,0,width(),height());shade.setColorAt(0,QColor(7,18,13,150));shade.setColorAt(0.7,QColor(7,18,13,25));shade.setColorAt(1,QColor(7,18,13,160));p.fillRect(rect(),shade);
    }
    void resizeEvent(QResizeEvent *) override {m_scaled=m_image.scaled(size(),Qt::KeepAspectRatioByExpanding,Qt::SmoothTransformation);}
private:
    QPixmap m_image,m_scaled;
};
QPushButton *button(const QString &label,QLayout *layout,std::function<void()> action,QWidget *owner,bool primary=false) {
    auto b=new QPushButton(label); b->setCursor(Qt::PointingHandCursor); if(primary)b->setProperty("primary",true);
    layout->addWidget(b); QObject::connect(b,&QPushButton::clicked,owner,std::move(action));return b;
}
QLabel *label(const QString &value,QLayout *layout,const char *name="") {
    auto l=new QLabel(value);l->setObjectName(name);l->setWordWrap(true);l->setTextFormat(Qt::PlainText);layout->addWidget(l);return l;
}
QWidget *page(QStackedWidget *stack,const QString &title,const QString &description,QVBoxLayout *&layout) {
    auto p=new QWidget;layout=new QVBoxLayout(p);layout->setContentsMargins(28,24,28,24);layout->setSpacing(16);
    label(title,layout,"pageTitle");label(description,layout,"muted");stack->addWidget(p);return p;
}
QString exception() {try{throw;}catch(const std::exception &e){return QString::fromUtf8(e.what());}catch(...){return "Unknown error";}}
QString filename(QListWidget *list) {return list->currentItem()?list->currentItem()->data(Qt::UserRole).toString():QString();}
void writeFile(const QString &path,const QByteArray &data) {QSaveFile f(path);if(!f.open(QIODevice::WriteOnly)||f.write(data)!=data.size()||!f.commit())throw std::runtime_error("Could not write file");}
}
MainWindow::MainWindow(QWidget *parent):QMainWindow(parent) {
    m_root=qEnvironmentVariable("EBALIA_DATA_DIR");if(m_root.isEmpty())m_root=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(m_root);Language::current=QSettings().value("ui/language",QLocale::system().name().left(2)).toString();
    if(qEnvironmentVariableIsSet("EBALIA_LANGUAGE"))Language::current=qEnvironmentVariable("EBALIA_LANGUAGE");
    if(!Language::available().contains(Language::current))Language::current="en";
    m_mc=new McInstanceManager(m_root,this);m_versions=new VersionManager(this);m_versions->loadVersions();
    m_patreon=new PatreonAuth(this);
    m_accounts=new AccountManager(m_mc->mcDir(),this);m_java=new JavaRunner(this);
    build();resize(1200,780);setMinimumSize(960,650);setWindowTitle("EBALIA Launcher");
    connect(m_mc,&McInstanceManager::manifestReady,this,[this](const QList<McVersion> &v){m_manifest=v;m_catalog->setText(text("Catálogo actualizado · ","Catalog updated · ","Catálogo atualizado · ")+QString::number(v.size())+text(" versiones"," versions"," versões"));});
    connect(m_mc,&McInstanceManager::manifestFailed,this,[this](const QString &e){m_catalog->setText(Language::message(e));});
    connect(m_mc,&McInstanceManager::installProgress,this,[this](const QString &,int p,const QString &stage){m_progress->setRange(0,100);m_progress->setValue(p);m_status->setText(Language::message(stage));});
    connect(m_mc,&McInstanceManager::installDone,this,[this](const QString &dir,bool ok,const QString &e){m_installing.remove(dir);m_progress->setValue(ok?100:0);m_status->setText(ok?text("Instancia lista para jugar","Instance ready to play","Instância pronta para jogar"):Language::message(e));refreshInstances();if(!ok)error(e);});
    connect(m_mc,&McInstanceManager::gameStarted,this,[this](const QString &){refreshInstances();m_status->setText(text("Minecraft en ejecución","Minecraft is running","Minecraft em execução"));});
    connect(m_mc,&McInstanceManager::gameEnded,this,[this](const QString &,int code){refreshInstances();m_status->setText(text("Minecraft terminó · código ","Minecraft exited · code ","Minecraft terminou · código ")+QString::number(code));if(code)error(text("Minecraft se cerró con un error. Abrí el registro de la instancia para ver el motivo.","Minecraft exited with an error. Open the instance log for details.","Minecraft fechou com erro. Abra o registro da instância para ver os detalhes."));});
    connect(m_mc,&McInstanceManager::launchFailed,this,[this](const QString &,const QString &e){error(e);refreshInstances();});
    connect(m_accounts,&AccountManager::accountsChanged,this,&MainWindow::refreshAccounts);
    connect(m_java,&JavaRunner::processError,this,&MainWindow::error);
    connect(m_java,&JavaRunner::processStarted,this,[this]{m_status->setText(text("Versión perdida en ejecución","Lost version is running","Versão perdida em execução"));});

    if(!qEnvironmentVariableIsSet("EBALIA_NO_NETWORK")){m_mc->fetchManifest();refreshNews();m_patreon->refreshNews();auto timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{m_mc->fetchManifest();refreshNews();});timer->start(30*60*1000);}
    if(!QSettings().value("ui/tutorialSeen",false).toBool()){showPage(5);QSettings().setValue("ui/tutorialSeen",true);}
}
void MainWindow::build() {
    auto old=takeCentralWidget();if(old)old->deleteLater();
    auto root=new QWidget;setCentralWidget(root);auto outer=new QVBoxLayout(root);outer->setContentsMargins(0,0,0,0);outer->setSpacing(0);
    auto top=new QWidget;top->setObjectName("top");auto header=new QHBoxLayout(top);header->setContentsMargins(24,16,24,16);
    auto logo=new QLabel("E B A L I A");logo->setObjectName("brand");header->addWidget(logo);header->addStretch();
    m_account=new QComboBox;m_account->setMinimumWidth(180);header->addWidget(m_account);
    connect(m_account,qOverload<int>(&QComboBox::activated),this,[this](int){m_accounts->setActive(m_account->currentData().toString());});
    auto language=new QComboBox;language->addItem("Español","es");language->addItem("English","en");language->addItem("Português","pt");language->addItem("Deutsch","de");language->addItem("Français","fr");language->addItem("Italiano","it");language->addItem("Русский","ru");language->addItem("日本語","ja");language->addItem("한국어","ko");language->addItem("简体中文","zh");language->setCurrentIndex(language->findData(Language::current));header->addWidget(language);
    connect(language,qOverload<int>(&QComboBox::activated),this,[this,language]{Language::current=language->currentData().toString();QSettings().setValue("ui/language",Language::current);int current=m_pages->currentIndex();build();showPage(current);});
    outer->addWidget(top);auto body=new QHBoxLayout;body->setSpacing(0);outer->addLayout(body,1);
    m_nav=new QListWidget;m_nav->setObjectName("navigation");m_nav->setFixedWidth(235);
    m_nav->addItems({text("◈  Mis instancias","◈  My instances","◈  Minhas instâncias"),text("◇  Versiones perdidas","◇  Lost versions","◇  Versões perdidas"),text("＋  Explorar mods","＋  Discover mods","＋  Explorar mods"),text("▣  Mis packs","▣  My packs","▣  Meus packs"),text("◎  Noticias","◎  News","◎  Notícias"),text("?  Guía y tutorial","?  Guide & tutorial","?  Guia e tutorial"),text("⚙  Cuentas y ajustes","⚙  Accounts & settings","⚙  Contas e ajustes")});
    m_nav->addItems({"✦  "+Language::key("Fan arts"),"▤  "+Language::key("Servers"),"◇  "+Language::key("Learn"),"♡  "+Language::key("Support & community"),"◉  "+Language::key("EBALIA · Patreon")});
    for(int i=0;i<m_nav->count();++i)m_nav->item(i)->setSizeHint(QSize(180,44));body->addWidget(m_nav);
    m_pages=new QStackedWidget;body->addWidget(m_pages,1);connect(m_nav,&QListWidget::currentRowChanged,m_pages,&QStackedWidget::setCurrentIndex);
    QVBoxLayout *l;
    page(m_pages,text("Tu próxima aventura","Your next adventure","Sua próxima aventura"),text("CLIENTE MINECRAFT · Mundos, mods y ajustes separados para cada instancia.","MINECRAFT CLIENT · Separate worlds, mods and settings for every instance.","CLIENTE MINECRAFT · Mundos, mods e ajustes separados em cada instância."),l);
    m_clientTabs=new QTabWidget;l->addWidget(m_clientTabs,1);
    auto home=new QWidget;auto homeLayout=new QVBoxLayout(home);homeLayout->setContentsMargins(0,14,0,0);homeLayout->setSpacing(16);
    auto banner=new GameBanner;auto heroLayout=new QVBoxLayout(banner);heroLayout->setContentsMargins(34,30,34,30);
    auto wordmark=label("MINECRAFT",heroLayout);wordmark->setStyleSheet("background:transparent;color:white;font-size:42px;font-weight:900;");
    auto edition=label("JAVA EDITION",heroLayout);edition->setStyleSheet("background:transparent;color:white;font-size:18px;font-weight:700;");heroLayout->addStretch();
    auto welcome=label(text("Tu próxima aventura","Your next adventure","Sua próxima aventura"),heroLayout);welcome->setStyleSheet("background:transparent;color:white;font-size:27px;font-weight:700;");homeLayout->addWidget(banner,1);
    auto launchBar=new QHBoxLayout;homeLayout->addLayout(launchBar);m_playInstance=new QComboBox;m_playInstance->setMinimumWidth(250);m_playInstance->setPlaceholderText(text("Crear mi primera instancia","Create my first instance","Criar minha primeira instância"));launchBar->addWidget(m_playInstance,1);
    auto folder=button("",launchBar,[this]{auto i=selected();if(!i.dir.isEmpty())QDesktopServices::openUrl(QUrl::fromLocalFile(i.dir));},this);
    folder->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));folder->setToolTip(text("📂 Abrir carpeta de instancia","📂 Open instance folder","📂 Abrir pasta da instância"));folder->setAccessibleName(folder->toolTip());
    m_playButton=button(text("▶ Jugar","▶ Play","▶ Jogar"),launchBar,[this]{play();},this,true);m_playButton->setMinimumSize(220,54);
    connect(m_playInstance,qOverload<int>(&QComboBox::activated),this,[this]{auto dir=m_playInstance->currentData().toString();m_instanceFilter->clear();for(int n=0;n<m_instances->count();++n)if(m_instances->item(n)->data(Qt::UserRole).toString()==dir)m_instances->setCurrentRow(n);});
    auto shortcuts=new QHBoxLayout;homeLayout->addLayout(shortcuts);
    button(text("＋ Nueva instancia","＋ New instance","＋ Nova instância"),shortcuts,[this]{createInstance();},this);
    button(text("Administrar mods","Manage mods","Gerenciar mods"),shortcuts,[this]{manageMods();},this);
    button(text("Guardar mods como pack","Save mods as a pack","Salvar mods como pack"),shortcuts,[this]{savePack();},this);
    m_homeNews=new QHBoxLayout;homeLayout->addLayout(m_homeNews);
    m_clientTabs->addTab(home,text("▶ Jugar","▶ Play","▶ Jogar"));
    auto library=new QWidget;auto libraryLayout=new QVBoxLayout(library);libraryLayout->setContentsMargins(0,14,0,0);libraryLayout->setSpacing(14);
    m_clientTabs->addTab(library,text("◈  Mis instancias","◈  My instances","◈  Minhas instâncias"));l=libraryLayout;
    auto actions=new QHBoxLayout;l->addLayout(actions);m_instanceFilter=new QLineEdit;m_instanceFilter->setPlaceholderText(text("Buscar instancias…","Find an instance…","Buscar instâncias…"));actions->addWidget(m_instanceFilter,1);
    button(text("＋ Nueva instancia","＋ New instance","＋ Nova instância"),actions,[this]{createInstance();},this,true);
    connect(m_instanceFilter,&QLineEdit::textChanged,this,[this]{refreshInstances();});
    auto split=new QHBoxLayout;l->addLayout(split,1);m_instances=new QListWidget;m_instances->setObjectName("instanceList");m_instances->setMinimumWidth(280);split->addWidget(m_instances,3);
    auto detail=new QFrame;detail->setObjectName("card");auto dl=new QVBoxLayout(detail);dl->setContentsMargins(22,22,22,22);dl->setSpacing(12);split->addWidget(detail,2);
    m_title=label("",dl,"sectionTitle");m_details=label("",dl,"muted");dl->addStretch();
    button(text("▶  Jugar / Instalar","▶  Play / Install","▶  Jogar / Instalar"),dl,[this]{play();},this,true);
    button(text("📂 Abrir carpeta de instancia","📂 Open instance folder","📂 Abrir pasta da instância"),dl,[this]{auto i=selected();if(!i.dir.isEmpty())QDesktopServices::openUrl(QUrl::fromLocalFile(i.dir));},this);
    button(text("Administrar mods","Manage mods","Gerenciar mods"),dl,[this]{manageMods();},this);
    button(text("Guardar mods como pack","Save mods as a pack","Salvar mods como pack"),dl,[this]{savePack();},this);
    button(text("Otra instancia con estos mods","New instance with these mods","Nova instância com estes mods"),dl,[this]{createInstance(true);},this);
    button(text("Ajustes de instancia","Instance settings","Ajustes da instância"),dl,[this]{editInstance();},this);
    auto remove=button(Language::key("Delete instance"),dl,[this]{removeInstance();},this);remove->setObjectName("deleteInstance");
    m_instances->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_instances,&QListWidget::customContextMenuRequested,this,[this](QPoint pos){auto item=m_instances->itemAt(pos);if(!item)return;m_instances->setCurrentItem(item);QMenu menu(this);auto settings=menu.addAction(text("Ajustes de instancia","Instance settings","Ajustes da instância"));auto trash=menu.addAction(Language::key("Delete instance"));auto action=menu.exec(m_instances->viewport()->mapToGlobal(pos));if(action==trash)removeInstance();else if(action==settings)editInstance();});
    auto deleteShortcut=new QShortcut(QKeySequence::Delete,m_instances);deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);connect(deleteShortcut,&QShortcut::activated,this,[this]{removeInstance();});
    connect(m_instances,&QListWidget::currentRowChanged,this,[this]{selection();});
    m_catalog=label(text("Consultando versiones…","Checking versions…","Consultando versões…"),l,"muted");
    m_clientTabs->addTab(new SkinsPage(m_root,m_accounts,m_mc,[this](const QString &title,std::function<QJsonObject()> job,std::function<void(QJsonObject)> done){work(title,job,done);},this),Language::key("Skins"));
    page(m_pages,text("Versiones perdidas","Lost versions","Versões perdidas"),text("El archivo de EBALIA. Cada versión conserva su instalación y su forma de inicio original.","The EBALIA archive. Each version keeps its own installation and original launch method.","O arquivo da EBALIA. Cada versão mantém sua instalação e sua forma original de iniciar."),l);
    m_lostFilter=new QLineEdit;m_lostFilter->setPlaceholderText(text("Buscar por nombre o categoría…","Search by name or category…","Buscar por nome ou categoria…"));l->addWidget(m_lostFilter);
    m_lost=new QListWidget;l->addWidget(m_lost,1);connect(m_lostFilter,&QLineEdit::textChanged,this,[this]{refreshLost();});
    auto lostActions=new QHBoxLayout;l->addLayout(lostActions);button(text("Instalar","Install","Instalar"),lostActions,[this]{lostAction(false);},this);button(text("▶ Jugar","▶ Play","▶ Jogar"),lostActions,[this]{lostAction(true);},this,true);

    button(Language::key("Setup & diagnostics"),lostActions,[this]{SetupDialog dialog(this);dialog.exec();},this);
    label(text("Los paquetes de Windows necesitan Windows o Wine. La disponibilidad depende del archivo original.","Windows packages require Windows or Wine. Availability depends on the original archive.","Pacotes do Windows precisam de Windows ou Wine. A disponibilidade depende do arquivo original."),l,"muted");
    page(m_pages,text("Encontrá tus próximos mods","Find your next mods","Encontre seus próximos mods"),Language::key("Mods for your Minecraft version and loader. Select one or more mods."),l);
    m_modProvider=new QComboBox;m_modProvider->addItem("Modrinth","modrinth");m_modProvider->addItem("CurseForge","curseforge");l->addWidget(m_modProvider);
    connect(m_modProvider,qOverload<int>(&QComboBox::activated),this,[this]{m_results->clear();m_offset=0;});
    m_target=new QComboBox;l->addWidget(m_target);connect(m_target,qOverload<int>(&QComboBox::activated),this,[this]{m_results->clear();m_hits={};});
    auto search=new QHBoxLayout;l->addLayout(search);m_query=new QLineEdit;m_query->setPlaceholderText("Sodium, Dynamic Lights, FallingTree, Veinminer…");search->addWidget(m_query,1);button(text("Buscar","Search","Buscar"),search,[this]{searchMods();},this,true);connect(m_query,&QLineEdit::returnPressed,this,[this]{searchMods();});
    m_results=new QListWidget;m_results->setSelectionMode(QAbstractItemView::ExtendedSelection);l->addWidget(m_results,1);auto browse=new QHBoxLayout;l->addLayout(browse);
    button(text("Ver instalación y dependencias","Preview installation & dependencies","Ver instalação e dependências"),browse,[this]{QJsonArray projects;for(auto item:m_results->selectedItems()){auto p=item->data(Qt::UserRole).toJsonObject();projects.append(QJsonObject{{"project_id",p["project_id"]},{"name",p["title"]}});}if(!projects.isEmpty())preview(projects,m_target->currentData().toString());},this,true);
    button(text("Más resultados","More results","Mais resultados"),browse,[this]{searchMods(m_offset+30);},this);
    page(m_pages,text("Tus mods, listos para volver","Your mods, ready to reuse","Seus mods, prontos para reutilizar"),text("Un pack recuerda tus mods favoritos y busca sus versiones compatibles en la instancia de destino.","A pack remembers your favorite mods and finds compatible versions for the target instance.","Um pack lembra seus mods favoritos e encontra versões compatíveis para a instância de destino."),l);
    m_packs=new QListWidget;l->addWidget(m_packs,1);m_packDetails=label("",l,"muted");connect(m_packs,&QListWidget::currentRowChanged,this,[this]{int i=m_packs->currentRow();if(i<0||i>=m_packData.size()){m_packDetails->clear();return;}QStringList names;for(const auto &p:m_packData[i].toObject()["projects"].toArray())names<<p.toObject()["name"].toString();m_packDetails->setText(names.join(" · "));});
    auto packs=new QHBoxLayout;l->addLayout(packs);button(text("Aplicar a una instancia","Apply to an instance","Aplicar a uma instância"),packs,[this]{applyPack();},this,true);
    button(text("Importar pack","Import pack","Importar pack"),packs,[this]{auto file=QFileDialog::getOpenFileName(this,{}, {},"EBALIA pack (*.json)");if(file.isEmpty())return;try{ModRepository(m_root).savePack(ModRepository::read(file));refreshPacks();}catch(...){error(exception());}},this);
    button(text("Exportar pack","Export pack","Exportar pack"),packs,[this]{int i=m_packs->currentRow();if(i<0)return;auto file=QFileDialog::getSaveFileName(this,{},"pack.ebalia.json","EBALIA pack (*.json)");if(file.isEmpty())return;try{auto pack=m_packData[i].toObject();pack.remove("path");ModRepository::write(file,pack);}catch(...){error(exception());}},this);
    label(text("Para crear un pack: Mis instancias → Guardar mods como pack. No necesitás un RAR ni copiar carpetas.","To create a pack: My instances → Save mods as a pack. No archives or folder copying needed.","Para criar um pack: Minhas instâncias → Salvar mods como pack. Sem RAR nem cópia de pastas."),l,"muted");
    page(m_pages,text("Lo nuevo en Minecraft","What's new in Minecraft","Novidades do Minecraft"),text("Noticias oficiales. Abrí un artículo para leerlo en Minecraft.net.","Official news. Open an article to read it on Minecraft.net.","Notícias oficiais. Abra um artigo para ler no Minecraft.net."),l);
    m_news=new QListWidget;l->addWidget(m_news,1);connect(m_news,&QListWidget::itemDoubleClicked,this,[this](QListWidgetItem *i){auto u=QUrl(i->data(Qt::UserRole).toString());if(u.scheme()=="https"&&(u.host()=="minecraft.net"||u.host().endsWith(".minecraft.net")))QDesktopServices::openUrl(u);});
    auto newsActions=new QHBoxLayout;l->addLayout(newsActions);button(text("Leer artículo","Read article","Ler artigo"),newsActions,[this]{if(auto i=m_news->currentItem())emit m_news->itemDoubleClicked(i);},this,true);button(text("Actualizar","Refresh","Atualizar"),newsActions,[this]{refreshNews();},this);refreshNews(false);
    page(m_pages,text("Empezá a jugar, paso a paso","Start playing, step by step","Comece a jogar, passo a passo"),text("Esta guía siempre está disponible en la barra lateral.","This guide is always available in the sidebar.","Este guia está sempre disponível na barra lateral."),l);
    auto guide=new QTextBrowser;guide->setOpenExternalLinks(false);guide->setHtml(text(
        "<h2>1. Elegí tu modo</h2><p><b>Mis instancias</b> es el cliente normal de Minecraft. <b>Versiones perdidas</b> abre el archivo de EBALIA.</p><h2>2. Creá una instancia</h2><p>Elegí un nombre, una versión y un cargador. Vanilla es el juego original; Fabric, Quilt, Forge o NeoForge permiten mods. Los mundos y ajustes quedan separados. El catálogo se actualiza al abrir y cada 30 minutos. Las instancias existentes conservan su versión para cuidar tus mundos.</p><h2>3. Agregá mods</h2><p>En <b>Explorar mods</b>, seleccioná tu instancia y buscá Sodium, luces dinámicas, FallingTree o Veinminer. Revisá las dependencias y confirmá la instalación. La compatibilidad publicada no garantiza que todos los mods funcionen juntos.</p><h2>4. Guardá tus favoritos como pack</h2><p>En tu instancia, usá <b>Guardar mods como pack</b>. Luego aplicalo desde <b>Mis packs</b> a otra instancia. Buscamos una edición para su versión y cargador; si no existe, te lo mostramos antes de descargar. No copiamos un JAR incompatible. Los mods locales deben poder identificarse en Modrinth; los desactivados no se incluyen.</p><h2>5. Jugá</h2><p>Agregá una cuenta, instalá Java y pulsá <b>Jugar / Instalar</b>. Una vez terminada la instalación, pulsá Jugar. Si falta Java, el launcher indica qué versión requiere el juego. Podés elegir su ruta y memoria en los ajustes de instancia.</p><h2>Si algo falla</h2><p>Abrí <b>Ajustes de instancia → Registro</b>. Reparar vuelve a comprobar las descargas del juego. Quitar una instancia la mueve a la papelera local con sus mundos. Forge y NeoForge se instalan con sus instaladores oficiales y necesitan Java antes de comenzar. Las versiones perdidas pueden requerir Wine, Java antiguo o paquetes originales que ya no estén disponibles.</p>",
        "<h2>1. Choose your mode</h2><p><b>My instances</b> is the regular Minecraft client. <b>Lost versions</b> opens the EBALIA archive.</p><h2>2. Create an instance</h2><p>Choose a name, game version and loader. Vanilla is the original game; Fabric, Quilt, Forge or NeoForge support mods. Worlds and settings stay separate. The catalog refreshes at startup and every 30 minutes. Existing instances keep their game version to protect your worlds.</p><h2>3. Add mods</h2><p>In <b>Discover mods</b>, choose your instance and search for Sodium, dynamic lights, FallingTree or Veinminer. Review dependencies and confirm installation. Published compatibility does not guarantee that all mods work together.</p><h2>4. Save your favorites as a pack</h2><p>Use <b>Save mods as a pack</b> on your instance. Apply it to another instance from <b>My packs</b>. We look up builds for its game version and loader; unavailable mods are listed before downloading. Incompatible JARs are never copied. Local mods must be identifiable on Modrinth; disabled mods are excluded.</p><h2>5. Play</h2><p>Add an account, install Java and press <b>Play / Install</b>. After installation finishes, press Play. If Java is missing, the launcher tells you which version the game requires. Choose its path and memory in instance settings.</p><h2>Troubleshooting</h2><p>Open <b>Instance settings → Log</b>. Repair verifies game downloads again. Removing an instance moves it to local trash with its worlds. Forge and NeoForge use their official installers and require Java before installation. Lost versions may require Wine, older Java or original packages that are no longer available.</p>",
        "<h2>1. Escolha seu modo</h2><p><b>Minhas instâncias</b> é o cliente normal do Minecraft. <b>Versões perdidas</b> abre o arquivo da EBALIA.</p><h2>2. Crie uma instância</h2><p>Escolha um nome, uma versão e um carregador. Vanilla é o jogo original; Fabric, Quilt, Forge ou NeoForge permitem mods. Mundos e ajustes ficam separados. O catálogo é atualizado ao abrir e a cada 30 minutos. Instâncias existentes mantêm sua versão para preservar seus mundos.</p><h2>3. Adicione mods</h2><p>Em <b>Explorar mods</b>, escolha a instância e busque Sodium, luzes dinâmicas, FallingTree ou Veinminer. Revise as dependências e confirme a instalação. A compatibilidade publicada não garante que todos os mods funcionem juntos.</p><h2>4. Salve seus favoritos como pack</h2><p>Use <b>Salvar mods como pack</b> na instância. Depois aplique em outra instância por <b>Meus packs</b>. Buscamos edições para a versão e o carregador de destino; mods indisponíveis são mostrados antes do download. Não copiamos JARs incompatíveis. Mods locais precisam ser identificados no Modrinth; mods desativados não entram no pack.</p><h2>5. Jogue</h2><p>Adicione uma conta, instale Java e clique em <b>Jogar / Instalar</b>. Ao terminar a instalação, clique em Jogar. Se faltar Java, o launcher indica a versão necessária. Escolha o caminho e a memória nos ajustes da instância.</p><h2>Se algo falhar</h2><p>Abra <b>Ajustes da instância → Registro</b>. Reparar verifica novamente os downloads do jogo. Remover uma instância move seus mundos para a lixeira local. Forge e NeoForge usam seus instaladores oficiais e precisam de Java antes da instalação. Versões perdidas podem precisar de Wine, Java antigo ou pacotes originais indisponíveis.</p>"));l->addWidget(guide,1);button(text("Crear mi primera instancia","Create my first instance","Criar minha primeira instância"),l,[this]{showPage(0);createInstance();},this,true);
    page(m_pages,text("A tu manera","Make it yours","Do seu jeito"),text("Cuentas, almacenamiento y preferencias del launcher.","Accounts, storage and launcher preferences.","Contas, armazenamento e preferências do launcher."),l);
    button(Language::key("Setup & diagnostics"),l,[this]{SetupDialog dialog(this);dialog.exec();},this);
    button(Language::key("Provider settings"),l,[this]{
        QDialog d(this);d.setWindowTitle(Language::key("Provider settings"));QFormLayout form(&d);
        QLineEdit curse(QSettings().value("integrations/curseforgeKey").toString()),microsoft(QSettings().value("auth/microsoftClientId").toString());curse.setEchoMode(QLineEdit::Password);
        form.addRow("CurseForge API key",&curse);form.addRow("Microsoft OAuth client ID",&microsoft);
        auto note=new QLabel(Language::key("Use your EBALIA application credentials. No passwords or keys from another launcher are required."));note->setWordWrap(true);form.addRow(note);
        QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);form.addRow(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);
        if(d.exec()==QDialog::Accepted){QSettings settings;settings.setValue("integrations/curseforgeKey",curse.text().trimmed());settings.setValue("auth/microsoftClientId",microsoft.text().trimmed());}
    },this);
    button(text("Agregar cuenta Microsoft","Add Microsoft account","Adicionar conta Microsoft"),l,[this]{account(true);},this,true);
    button(text("Agregar perfil local","Add local profile","Adicionar perfil local"),l,[this]{account(false);},this);
    button(text("Quitar cuenta activa","Remove active account","Remover conta ativa"),l,[this]{auto a=m_accounts->active();if(!a.uuid.isEmpty()&&QMessageBox::question(this,"EBALIA",text("¿Quitar la cuenta del launcher?","Remove this account from the launcher?","Remover esta conta do launcher?"))==QMessageBox::Yes)m_accounts->removeAccount(a.uuid);},this);
    label(text("Microsoft permite usar servidores autenticados con tu cuenta. El perfil local sirve para jugar sin conexión.","Microsoft lets you use authenticated servers with your account. A local profile is for offline play.","A conta Microsoft permite acessar servidores autenticados. O perfil local serve para jogar offline."),l,"muted");
    button(text("Abrir carpeta de datos","Open data folder","Abrir pasta de dados"),l,[this]{QDesktopServices::openUrl(QUrl::fromLocalFile(m_root));},this);
    button(text("Abrir papelera de instancias","Open instance trash","Abrir lixeira de instâncias"),l,[this]{QDir().mkpath(m_mc->mcDir()+"/trash");QDesktopServices::openUrl(QUrl::fromLocalFile(m_mc->mcDir()+"/trash"));},this);
    button(text("Actualizar catálogo de versiones","Refresh version catalog","Atualizar catálogo de versões"),l,[this]{m_mc->fetchManifest();},this);
    label(text("EBALIA 4.0 · La actualización automática es del catálogo y las noticias. No cambia los mundos, mods ni la versión de tus instancias.","EBALIA 4.0 · Automatic refresh updates the catalog and news. It does not change worlds, mods or existing instance versions.","EBALIA 4.0 · A atualização automática é do catálogo e das notícias. Não altera mundos, mods ou versões das instâncias."),l,"muted");l->addStretch();
    for(int section=0;section<5;++section)m_pages->addWidget(new CommunityPage(section,m_root,m_patreon));
    auto footer=new QHBoxLayout;footer->setContentsMargins(22,10,22,10);m_status=new QLabel(text("Todo listo","Ready","Tudo pronto"));m_status->setWordWrap(true);footer->addWidget(m_status,1);m_progress=new QProgressBar;m_progress->setFixedWidth(180);m_progress->setValue(0);footer->addWidget(m_progress);outer->addLayout(footer);
    setStyleSheet(R"(
QMainWindow,QWidget{background:#11151b;color:#e8edf5;font-family:'DejaVu Sans';font-size:13px;}
#top{background:#171d26;border-bottom:1px solid #2c3543;}#brand{font-size:22px;font-weight:800;color:#98edc6;background:transparent;}
#navigation{background:#141a22;border:0;border-right:1px solid #293340;padding:16px 8px;}#navigation::item{padding:0 10px;border-radius:8px;margin:3px 0;}#navigation::item:selected{background:#28433b;color:#a5f6ce;}
#pageTitle{font-size:29px;font-weight:700;}#sectionTitle{font-size:21px;font-weight:700;}#muted{color:#9daabc;line-height:1.6;}#card{background:#1a222c;border:1px solid #303e4c;border-radius:12px;}#card QLabel{background:transparent;}
QToolButton{background:#1a222c;border:1px solid #303e4c;border-radius:9px;padding:8px;}QToolButton:hover{border-color:#92ebbe;}
QPushButton{background:#252f3c;border:1px solid #3a4859;border-radius:7px;padding:11px 15px;font-weight:600;}#newsCard{background:#1b2630;text-align:left;}#newsCard:hover{border-color:#92ebbe;}QPushButton:hover{background:#334257;border-color:#6b8098;}QPushButton[primary=true]{background:#92ebbe;color:#12291f;border-color:#92ebbe;}QPushButton[primary=true]:hover{background:#b5f6d4;}QPushButton:disabled{color:#77818d;background:#222b35;}
QLineEdit,QComboBox,QSpinBox{background:#1c2530;border:1px solid #364454;border-radius:6px;padding:10px;selection-background-color:#396650;}QComboBox QAbstractItemView{background:#1c2530;selection-background-color:#396650;}
QListWidget,QTextBrowser,QPlainTextEdit{background:#161e28;border:1px solid #303d4c;border-radius:9px;padding:8px;}QListWidget::item{padding:14px;border-bottom:1px solid #273340;}QListWidget::item:selected{background:#2b463e;color:#c0f7db;border-radius:6px;}QListWidget::item:hover{background:#23332f;}
QProgressBar{border:0;background:#283342;border-radius:5px;min-height:8px;max-height:16px;text-align:center;}QProgressBar::chunk{background:#7edfb1;border-radius:5px;}QScrollBar:vertical{background:#151c25;width:10px;}QScrollBar::handle:vertical{background:#405065;min-height:25px;border-radius:4px;}
QTabWidget::pane{border:0;}QTabBar::tab{background:#11151b;padding:12px 20px;border-bottom:3px solid transparent;}QTabBar::tab:selected{border-bottom:3px solid #92ebbe;color:#a5f6ce;}
QToolTip{background:#293542;color:white;border:1px solid #58687a;}QDialog{background:#161d26;}QTextBrowser{padding:18px;font-size:14px;}
)");
    refreshAccounts();refreshInstances();refreshPacks();refreshLost();m_nav->setCurrentRow(0);
}
void MainWindow::showPage(int i){m_nav->setCurrentRow(i);}
void MainWindow::error(const QString &e){QMessageBox::warning(this,"EBALIA",Language::message(e));}
void MainWindow::refreshAccounts(){m_account->blockSignals(true);m_account->clear();for(const auto &a:m_accounts->accounts())m_account->addItem(a.name+(a.type=="msa"?" · Microsoft":" · "+Language::key("Local profile")),a.uuid);if(m_account->count()==0)m_account->addItem(text("Sin cuenta","No account","Sem conta"));m_account->setCurrentIndex(qMax(0,m_account->findData(m_accounts->active().uuid)));m_account->blockSignals(false);}
McInstance MainWindow::selected() const {auto dir=filename(m_instances);for(const auto &i:m_mc->instances())if(i.dir==dir)return i;return {};}
void MainWindow::refreshInstances(){
    auto selectedDir=filename(m_instances),target=m_target->currentData().toString();m_instances->clear();m_target->clear();m_playInstance->blockSignals(true);m_playInstance->clear();
    for(const auto &i:m_mc->instances()){
        m_playInstance->addItem(i.name+" · "+i.mcVersion+" / "+i.loader,i.dir);
        if(i.loader!="vanilla")m_target->addItem(i.name+" · "+i.mcVersion+" / "+i.loader,i.dir);
        if(!i.name.contains(m_instanceFilter->text(),Qt::CaseInsensitive))continue;
        QString state=m_mc->isRunning(i.dir)?text("En ejecución","Running","Em execução"):m_installing.contains(i.dir)?text("Instalando…","Installing…","Instalando…"):i.ready?text("Lista para jugar","Ready to play","Pronta para jogar"):text("Pendiente de instalación","Not installed yet","Ainda não instalada");
        QJsonObject meta;try{meta=ModRepository::read(i.dir+"/instance.json");}catch(...){}
        auto item=new QListWidgetItem(i.name+(meta["group"].toString().isEmpty()?QString():" · "+meta["group"].toString())+"\n"+i.mcVersion+"  ·  "+i.loader.toUpper()+"  ·  "+state,m_instances);item->setData(Qt::UserRole,i.dir);item->setSizeHint(QSize(280,88));if(i.dir==selectedDir)m_instances->setCurrentItem(item);
    }
    if(m_instances->currentRow()<0&&m_instances->count())m_instances->setCurrentRow(0);
    int idx=m_target->findData(target);if(idx>=0)m_target->setCurrentIndex(idx);m_playInstance->blockSignals(false);selection();
}
void MainWindow::selection(){auto i=selected();m_playInstance->setCurrentIndex(m_playInstance->findData(i.dir));m_playButton->setText(i.dir.isEmpty()?text("Crear instancia","Create instance","Criar instância"):m_mc->isRunning(i.dir)?Language::standard("Close"):i.ready?text("▶ Jugar","▶ Play","▶ Jogar"):text("Instalar","Install","Instalar"));m_playButton->setEnabled(!m_installing.contains(i.dir));if(i.dir.isEmpty()){m_title->setText(text("Un lugar para cada mundo","A place for every world","Um lugar para cada mundo"));m_details->setText(text("Creá tu primera instancia. Podés tener varias de la misma versión con mods diferentes.","Create your first instance. You can have multiple instances of the same version with different mods.","Crie sua primeira instância. Você pode ter várias da mesma versão com mods diferentes."));return;}m_title->setText(i.name);int count=QDir(i.dir+"/mods").entryList({"*.jar"},QDir::Files).size();m_details->setText("Minecraft "+i.mcVersion+"\n"+i.loader.toUpper()+" · "+QString::number(i.xmx)+" MB\n"+QString::number(count)+text(" mods activos"," active mods"," mods ativos"));}
void MainWindow::refreshPacks(){m_packs->clear();try{m_packData=ModRepository(m_root).packs();for(const auto &v:m_packData){auto p=v.toObject();m_packs->addItem(p["name"].toString()+"\n"+QString::number(p["projects"].toArray().size())+" mods");}if(m_packs->count())m_packs->setCurrentRow(0);else m_packDetails->setText(text("Todavía no hay packs. Guardá los mods de una instancia para empezar.","No packs yet. Save an instance's mods to get started.","Ainda não há packs. Salve os mods de uma instância para começar."));}catch(...){m_packDetails->setText(exception());}}
void MainWindow::refreshLost(){m_lost->clear();for(const auto &v:m_versions->getVersions()){if(!(v.name+v.category).contains(m_lostFilter->text(),Qt::CaseInsensitive))continue;auto item=new QListWidgetItem(v.name+"\n"+v.category+(m_versions->isVersionInstalled(v)?text(" · Instalada"," · Installed"," · Instalada"):""),m_lost);item->setData(Qt::UserRole,v.id);}if(m_lost->count())m_lost->setCurrentRow(0);}
void MainWindow::work(const QString &title,std::function<QJsonObject()> job,std::function<void(QJsonObject)> done){
    if(m_jobs)return;++m_jobs;auto dialog=new QProgressDialog(title,QString(),0,0,this);dialog->setCancelButton(nullptr);dialog->setWindowModality(Qt::ApplicationModal);dialog->setMinimumDuration(0);dialog->show();
    auto watcher=new QFutureWatcher<QJsonObject>(this);connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,dialog,watcher,done]{auto result=watcher->result();watcher->deleteLater();dialog->close();dialog->deleteLater();--m_jobs;if(result.contains("_error"))error(result["_error"].toString());else done(result);});
    watcher->setFuture(QtConcurrent::run([job]{try{return job();}catch(...){return QJsonObject{{"_error",exception()}};}}));
}
void MainWindow::createInstance(bool copy){
    auto source=selected();if(copy&&source.dir.isEmpty())return;
    if(copy&&(m_mc->isRunning(source.dir)||m_installing.contains(source.dir))){error(text("Cerrá el juego antes de copiar sus mods.","Close the game before copying its mods.","Feche o jogo antes de copiar seus mods."));return;}
    if(m_manifest.isEmpty()){m_mc->fetchManifest();error(text("El catálogo todavía no está disponible. Revisá tu conexión y volvé a intentar.","The catalog is not available yet. Check your connection and try again.","O catálogo ainda não está disponível. Verifique a conexão e tente novamente."));return;}
    CreateInstanceDialog dialog(m_mc,m_manifest,m_packData,source,copy,this);
    if(dialog.exec()!=QDialog::Accepted)return;auto c=dialog.configuration();
    if(!c["providerPack"].toObject().isEmpty()){auto root=m_root;work(Language::key("Installing modpack…"),[root,c]{auto dir=PackService(root).install(c["providerPack"].toObject(),c["providerVersion"].toObject(),c["name"].toString(),c["group"].toString(),c["xmx"].toInt(4096));return QJsonObject{{"dir",dir}};},[this](QJsonObject result){m_instanceFilter->clear();refreshInstances();showPage(0);for(int n=0;n<m_instances->count();++n)if(m_instances->item(n)->data(Qt::UserRole).toString()==result["dir"].toString())m_instances->setCurrentRow(n);m_status->setText(Language::key("Modpack ready. Press Install to prepare Minecraft."));});return;}
    try{auto dir=m_mc->createInstance(c["name"].toString(),c["mcVersion"].toString(),c["loader"].toString(),c["loaderVersion"].toString());auto info=ModRepository::read(dir+"/instance.json");for(auto key:{"xmx","group","icon"})info[key]=c[key];ModRepository::write(dir+"/instance.json",info);m_instanceFilter->clear();refreshInstances();showPage(0);for(int i=0;i<m_instances->count();++i)if(m_instances->item(i)->data(Qt::UserRole).toString()==dir)m_instances->setCurrentRow(i);
        if(copy){auto root=m_root;work(text("Identificando los mods…","Identifying mods…","Identificando mods…"),[root,source]{return ModRepository(root).capture(source.dir,source.name);},[this,dir](QJsonObject p){preview(p["projects"].toArray(),dir);});}
        else if(!c["pack"].toObject().isEmpty())preview(c["pack"].toObject()["projects"].toArray(),dir);
    }catch(...){error(exception());}
}

void MainWindow::play(){
    auto i=selected();if(i.dir.isEmpty()){createInstance();return;}if(m_installing.contains(i.dir))return;
    if(m_mc->isRunning(i.dir)){if(QMessageBox::question(this,"EBALIA",text("¿Cerrar esta instancia de Minecraft?","Close this Minecraft instance?","Fechar esta instância do Minecraft?"))==QMessageBox::Yes)m_mc->killInstance(i.dir);return;}
    if(!i.ready){m_installing.insert(i.dir);m_mc->installInstance(i.dir);refreshInstances();return;}
    auto a=m_accounts->active();if(a.uuid.isEmpty()){showPage(6);error(text("Agregá una cuenta o perfil local para jugar.","Add an account or local profile to play.","Adicione uma conta ou perfil local para jogar."));return;}
    if(a.type!="msa"){m_mc->launch(i.dir,a.name,a.uuid,{},"legacy");return;}
    auto auth=new MsAuth(this);auto dialog=new QProgressDialog(text("Renovando sesión…","Refreshing session…","Renovando sessão…"),QString(),0,0,this);dialog->setCancelButton(nullptr);dialog->setWindowModality(Qt::ApplicationModal);dialog->show();
    connect(auth,&MsAuth::loginFailed,dialog,[this,auth,dialog](const QString &e){dialog->close();dialog->deleteLater();auth->deleteLater();error(e);});
    connect(auth,&MsAuth::loginDone,dialog,[this,auth,dialog,i](const QString &token,const QString &refresh,const QString &uuid,const QString &name){m_accounts->addAccount({"msa",name,uuid,token,refresh});dialog->close();dialog->deleteLater();auth->deleteLater();m_mc->launch(i.dir,name,uuid,token,"msa");});auth->refresh(a.refreshToken);
}
void MainWindow::removeInstance(){
    auto i=selected();if(i.dir.isEmpty())return;
    if(m_mc->isRunning(i.dir)||m_mc->isInstalling(i.dir)||m_installing.contains(i.dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}
    QMessageBox confirm(QMessageBox::Question,Language::key("Delete instance"),Language::key("Move this instance and its worlds to local trash?")+"\n\n"+i.name,QMessageBox::Yes|QMessageBox::No,this);confirm.setObjectName("confirmDeleteInstance");confirm.setDefaultButton(QMessageBox::No);if(confirm.exec()!=QMessageBox::Yes)return;
    try{m_mc->deleteInstance(i.dir);refreshInstances();m_status->setText(Language::key("Instance moved to trash. Your worlds are preserved."));}catch(...){error(exception());}
}
void MainWindow::editInstance(){
    auto i=selected();if(i.dir.isEmpty())return;if(m_installing.contains(i.dir)||m_mc->isRunning(i.dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}
    QDialog d(this);d.setWindowTitle(i.name);d.resize(560,400);QVBoxLayout lay(&d);QFormLayout form;lay.addLayout(&form);
    QJsonObject info;try{info=ModRepository::read(i.dir+"/instance.json");}catch(...){error(exception());return;}
    QLineEdit name(i.name),java(info["javaPath"].toString());java.setObjectName("javaPath");java.setPlaceholderText(text("Detección automática","Automatic detection","Detecção automática"));QSpinBox memory;memory.setRange(512,65536);memory.setValue(i.xmx);memory.setSuffix(" MB");form.addRow(text("Nombre","Name","Nome"),&name);form.addRow("Java",&java);form.addRow(text("Memoria","Memory","Memória"),&memory);
    auto javaStatus=label(Language::key("Checking installed Java versions…"),&lay,"muted");
    auto detected=new QComboBox;detected->setObjectName("detectedJava");lay.addWidget(detected);detected->addItem(Language::key("Automatic (recommended)"),QString());
    auto watcher=new QFutureWatcher<QList<JavaInstallation>>(&d);
    int required=0;try{required=ModRepository::read(i.dir+"/launch-profile.json")["javaVersion"].toObject()["majorVersion"].toInt(8);}catch(...){}
    auto scan=[watcher,javaStatus]{if(watcher->isRunning())return;javaStatus->setText(Language::key("Checking installed Java versions…"));watcher->setFuture(QtConcurrent::run([]{return JavaRuntime::discover();}));};
    connect(watcher,&QFutureWatcher<QList<JavaInstallation>>::finished,&d,[&,detected,javaStatus,required]{
        QSignalBlocker blocked(detected);detected->clear();detected->addItem(Language::key("Automatic (recommended)"),QString());
        auto installations=watcher->result();QStringList summary;for(auto j:installations){detected->addItem("Java "+j.version+" · "+j.architecture+" · "+j.path,j.path);summary<<QString::number(j.major);}
        summary.removeDuplicates();javaStatus->setText(installations.isEmpty()?Language::key("No working Java installation was found."):Language::key("Detected Java")+": "+summary.join(", ")+(required?" · "+Language::key("Required")+": "+QString::number(required):QString()));
        auto index=detected->findData(java.text());detected->setCurrentIndex(index>=0?index:0);
    });
    connect(detected,qOverload<int>(&QComboBox::activated),&d,[&]{java.setText(detected->currentData().toString());});
    button(Language::key("Scan for Java again"),&lay,scan,&d);scan();
    button(text("Elegir ejecutable de Java","Choose Java executable","Escolher executável Java"),&lay,[&]{auto f=QFileDialog::getOpenFileName(&d);if(!f.isEmpty())java.setText(f);},&d);
    button(text("📂 Abrir carpeta de instancia","📂 Open instance folder","📂 Abrir pasta da instância"),&lay,[i]{QDesktopServices::openUrl(QUrl::fromLocalFile(i.dir));},&d);
    button(text("Registro","Log","Registro"),&lay,[&,i]{QDialog log(&d);log.resize(800,520);QVBoxLayout ll(&log);QPlainTextEdit view;view.setReadOnly(true);QFile f(i.dir+"/launcher.log");if(!f.exists())f.setFileName(i.dir+"/logs/latest.log");if(f.open(QIODevice::ReadOnly)){f.seek(qMax<qint64>(0,f.size()-120000));view.setPlainText(QString::fromUtf8(f.readAll()));}else view.setPlainText(text("Todavía no hay registro.","No log yet.","Ainda não há registro."));ll.addWidget(&view);log.exec();},&d);
    button(text("Reparar instalación","Repair installation","Reparar instalação"),&lay,[&,i]{d.reject();m_installing.insert(i.dir);m_mc->installInstance(i.dir);refreshInstances();},&d);
    button(text("Mover instancia a la papelera","Move instance to trash","Mover instância para a lixeira"),&lay,[&]{d.reject();removeInstance();},&d);
    QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);lay.addWidget(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&d,[&]{if(name.text().trimmed().isEmpty())return;info["name"]=name.text().trimmed();info["javaPath"]=java.text().trimmed();info["xmx"]=memory.value();try{ModRepository::write(i.dir+"/instance.json",info);d.accept();refreshInstances();}catch(...){error(exception());}});connect(&buttons,&QDialogButtonBox::rejected,&d,&QDialog::reject);d.exec();
}
void MainWindow::manageMods(){
    auto i=selected();if(i.dir.isEmpty())return;if(m_mc->isRunning(i.dir)||m_installing.contains(i.dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}
    QDialog d(this);d.setWindowTitle(i.name+" · Mods");d.resize(780,550);QVBoxLayout lay(&d);QListWidget list;lay.addWidget(&list,1);
    auto refresh=[&]{list.clear();for(const auto &f:QDir(i.dir+"/mods").entryList({"*.jar","*.jar.disabled"},QDir::Files))list.addItem(f);};refresh();
    auto row=new QHBoxLayout;lay.addLayout(row);button(text("Activar / desactivar","Enable / disable","Ativar / desativar"),row,[&]{if(!list.currentItem())return;auto name=list.currentItem()->text();auto to=name.endsWith(".disabled")?name.chopped(9):name+".disabled";if(!QFile::rename(i.dir+"/mods/"+name,i.dir+"/mods/"+to))error(text("No se pudo cambiar el archivo.","Could not change the file.","Não foi possível alterar o arquivo."));refresh();},&d);
    button(text("Quitar mod","Remove mod","Remover mod"),row,[&]{if(!list.currentItem())return;auto name=list.currentItem()->text();QDir().mkpath(i.dir+"/removed-mods");if(!QFile::rename(i.dir+"/mods/"+name,i.dir+"/removed-mods/"+QString::number(QDateTime::currentMSecsSinceEpoch())+"-"+name))error(text("No se pudo mover el archivo.","Could not move the file.","Não foi possível mover o arquivo."));refresh();},&d);
    button(text("Agregar JAR local","Add local JAR","Adicionar JAR local"),row,[&]{for(const auto &f:QFileDialog::getOpenFileNames(&d,{}, {},"Mods (*.jar)")){QDir().mkpath(i.dir+"/mods");if(!QFile::copy(f,i.dir+"/mods/"+QFileInfo(f).fileName()))error(text("El archivo ya existe o no se puede copiar.","The file already exists or cannot be copied.","O arquivo já existe ou não pode ser copiado."));}refresh();},&d);
    button(text("Explorar mods compatibles","Find compatible mods","Buscar mods compatíveis"),&lay,[&]{d.accept();showPage(2);m_target->setCurrentIndex(m_target->findData(i.dir));searchMods();},&d,true);d.exec();selection();
}
void MainWindow::savePack(){auto i=selected();if(i.dir.isEmpty())return;if(m_mc->isRunning(i.dir)||m_installing.contains(i.dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}bool ok;auto name=QInputDialog::getText(this,text("Guardar pack","Save pack","Salvar pack"),text("Nombre del pack","Pack name","Nome do pack"),QLineEdit::Normal,i.name,&ok);if(!ok||name.trimmed().isEmpty())return;auto root=m_root;work(text("Identificando tus mods en Modrinth…","Identifying your mods on Modrinth…","Identificando seus mods no Modrinth…"),[root,i,name]{ModRepository repo(root);auto p=repo.capture(i.dir,name);repo.savePack(p);return p;},[this](QJsonObject){refreshPacks();showPage(3);});}
void MainWindow::applyPack(){int index=m_packs->currentRow();if(index<0)return;QDialog d(this);d.setWindowTitle(text("Destino del pack","Pack destination","Destino do pack"));QVBoxLayout lay(&d);QComboBox instances;for(const auto &i:m_mc->instances())if(i.loader!="vanilla"&&!m_mc->isRunning(i.dir)&&!m_installing.contains(i.dir))instances.addItem(i.name+" · "+i.mcVersion+" / "+i.loader,i.dir);lay.addWidget(&instances);QDialogButtonBox bb(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);lay.addWidget(&bb);connect(&bb,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&bb,&QDialogButtonBox::rejected,&d,&QDialog::reject);if(!instances.count()){error(text("Creá una instancia con cargador de mods primero.","Create an instance with a mod loader first.","Crie uma instância com carregador de mods primeiro."));return;}if(d.exec()==QDialog::Accepted)preview(m_packData[index].toObject()["projects"].toArray(),instances.currentData().toString());}
void MainWindow::preview(QJsonArray projects,QString dir){
    if(dir.isEmpty())return;if(m_mc->isRunning(dir)||m_installing.contains(dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}
    QJsonObject info;try{info=ModRepository::read(dir+"/instance.json");}catch(...){error(exception());return;}auto root=m_root;
    work(text("Comprobando versiones y dependencias…","Checking versions and dependencies…","Verificando versões e dependências…"),[root,projects,info,dir]{
        QJsonArray installed;if(QFile::exists(dir+"/mods.json")){for(const auto &v:ModRepository::read(dir+"/mods.json")["mods"].toArray())if(QFile::exists(dir+"/mods/"+v.toObject()["filename"].toString()))installed.append(v);}
        return ModRepository(root).plan(projects,info["mcVersion"].toString(),info["loader"].toString(),installed);
    },[this,dir,root](QJsonObject plan){
        QDialog d(this);d.setWindowTitle(text("Revisar instalación","Review installation","Revisar instalação"));d.resize(720,540);QVBoxLayout lay(&d);
        label(plan["game"].toString()+" / "+plan["loader"].toString(),&lay,"sectionTitle");QListWidget list;lay.addWidget(&list,1);
        for(const auto &v:plan["versions"].toArray()){auto o=v.toObject();list.addItem("✓ "+o["name"].toString()+"\n"+o["file"].toObject()["filename"].toString());}
        for(const auto &v:plan["missing"].toArray())list.addItem("⚠ "+v.toObject()["name"].toString()+"\n"+Language::message(v.toObject()["reason"].toString()));
        for(const auto &v:plan["missing"].toArray()){
            auto reason=v.toObject()["reason"].toString();auto match=QRegularExpression("https://www\\.curseforge\\.com/[^\\s]+").match(reason);
            if(match.hasMatch()){auto url=match.captured();button(Language::key("Open manual download"),&lay,[url]{QDesktopServices::openUrl(QUrl(url));},&d);}
        }
        QCheckBox skip(text("Instalar solo los compatibles; omitir los no disponibles","Install compatible mods only; skip unavailable mods","Instalar apenas compatíveis; ignorar indisponíveis"));if(!plan["missing"].toArray().isEmpty())lay.addWidget(&skip);
        label(text("Se incluyen las dependencias obligatorias. Revisá los mods omitidos antes de continuar.","Required dependencies are included. Review skipped mods before continuing.","Dependências obrigatórias estão incluídas. Revise os mods ignorados antes de continuar."),&lay,"muted");
        QDialogButtonBox bb(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);lay.addWidget(&bb);bb.button(QDialogButtonBox::Ok)->setText(text("Instalar mods","Install mods","Instalar mods"));auto valid=[&]{bb.button(QDialogButtonBox::Ok)->setEnabled(!plan["versions"].toArray().isEmpty()&&(plan["missing"].toArray().isEmpty()||skip.isChecked()));};valid();connect(&skip,&QCheckBox::toggled,&d,valid);connect(&bb,&QDialogButtonBox::accepted,&d,&QDialog::accept);connect(&bb,&QDialogButtonBox::rejected,&d,&QDialog::reject);
        if(d.exec()!=QDialog::Accepted)return;
        work(text("Descargando y verificando los mods…","Downloading and verifying mods…","Baixando e verificando os mods…"),[root,dir,plan]{ModRepository(root).apply(dir,plan);return QJsonObject{};},[this](QJsonObject){refreshInstances();m_status->setText(text("Mods instalados. Tu instancia está actualizada.","Mods installed. Your instance is up to date.","Mods instalados. Sua instância está atualizada."));});
    });
}
void MainWindow::searchMods(int offset){auto dir=m_target->currentData().toString();if(dir.isEmpty()){error(text("Creá una instancia con cargador de mods primero.","Create an instance with a mod loader first.","Crie uma instância com carregador de mods primeiro."));return;}QJsonObject info;try{info=ModRepository::read(dir+"/instance.json");}catch(...){error(exception());return;}auto root=m_root,query=m_query->text(),provider=m_modProvider->currentData().toString();work(Language::key("Searching provider…"),[root,query,info,offset,provider]{return QJsonObject{{"hits",ModRepository(root).search(query,info["mcVersion"].toString(),info["loader"].toString(),offset,provider)}};},[this,offset](QJsonObject r){m_offset=offset;if(offset==0)m_results->clear();auto hits=r["hits"].toArray();for(const auto &v:hits){auto p=v.toObject();auto item=new QListWidgetItem(p["title"].toString()+" · "+p["author"].toString()+"\n"+p["description"].toString(),m_results);item->setData(Qt::UserRole,p);item->setToolTip(p["description"].toString());}if(hits.isEmpty())m_status->setText(text("No se encontraron más mods compatibles.","No more compatible mods found.","Nenhum outro mod compatível encontrado."));});}
void MainWindow::account(bool microsoft){
    if(!microsoft){bool ok;auto name=QInputDialog::getText(this,text("Perfil local","Local profile","Perfil local"),text("Nombre de jugador","Player name","Nome de jogador"),QLineEdit::Normal,{},&ok);if(!ok)return;if(!QRegularExpression("^[A-Za-z0-9_]{3,16}$").match(name).hasMatch()){error(text("Usá entre 3 y 16 letras, números o guiones bajos.","Use 3–16 letters, numbers or underscores.","Use de 3 a 16 letras, números ou sublinhados."));return;}auto hash=QCryptographicHash::hash(("OfflinePlayer:"+name).toUtf8(),QCryptographicHash::Md5);hash[6]=(hash[6]&0x0f)|0x30;hash[8]=(hash[8]&0x3f)|0x80;m_accounts->addAccount({"offline",name,QString::fromLatin1(hash.toHex()),{}, {}});return;}
    QDialog d(this);d.setWindowTitle("Microsoft");d.resize(580,280);QVBoxLayout lay(&d);auto status=label(text("Conectando…","Connecting…","Conectando…"),&lay);auto code=label("",&lay,"sectionTitle");code->setTextInteractionFlags(Qt::TextSelectableByMouse);MsAuth auth;
    connect(&auth,&MsAuth::statusUpdate,&d,[status](const QString &s){status->setText(Language::message(s));});connect(&auth,&MsAuth::showCode,&d,[code](const QString &c,const QString &url){code->setText(c+"\n"+url);QDesktopServices::openUrl(QUrl(url));});
    connect(&auth,&MsAuth::loginFailed,&d,[this,&d](const QString &e){d.reject();error(e);});connect(&auth,&MsAuth::loginDone,&d,[this,&d](const QString &token,const QString &refresh,const QString &uuid,const QString &name){m_accounts->addAccount({"msa",name,uuid,token,refresh});d.accept();});QTimer::singleShot(0,&auth,&MsAuth::startLogin);d.exec();auth.cancel();
}
void MainWindow::refreshNews(bool network){
    auto render=[this](QJsonObject o){
        while(auto item=m_homeNews->takeAt(0)){if(item->widget())item->widget()->deleteLater();delete item;}
        auto entries=o["entries"].toArray();
        for(int index=0;index<qMin(2,entries.size());++index){auto entry=entries[index].toObject();
            auto card=new QPushButton;card->setObjectName("newsCard");card->setMinimumHeight(76);card->setCursor(Qt::PointingHandCursor);auto cardLayout=new QVBoxLayout(card);cardLayout->setContentsMargins(14,10,14,10);cardLayout->setSpacing(4);
            auto meta=label(entry["date"].toString().left(10)+"  ·  MINECRAFT",cardLayout);meta->setStyleSheet("background:transparent;color:#92ebbe;font-size:10px;");
            auto title=label(entry["title"].toString(),cardLayout);title->setStyleSheet("background:transparent;font-size:12px;font-weight:600;");title->setAttribute(Qt::WA_TransparentForMouseEvents);meta->setAttribute(Qt::WA_TransparentForMouseEvents);
            card->setAccessibleName(entry["title"].toString());m_homeNews->addWidget(card,1);
            connect(card,&QPushButton::clicked,this,[this,entry]{auto url=QUrl(entry["readMoreLink"].toString(entry["link"].toString()));if(url.scheme()=="https"&&(url.host()=="minecraft.net"||url.host().endsWith(".minecraft.net")))QDesktopServices::openUrl(url);else showPage(4);});
        }
        if(entries.isEmpty())button(text("Lo nuevo en Minecraft","What's new in Minecraft","Novidades do Minecraft"),m_homeNews,[this]{showPage(4);},this);
        m_news->clear();for(const auto &v:o["entries"].toArray()){auto e=v.toObject();if(e["title"].toString().isEmpty())continue;auto item=new QListWidgetItem(e["title"].toString()+"\n"+e["date"].toString().left(10)+"  ·  "+e["text"].toString(),m_news);item->setData(Qt::UserRole,e["readMoreLink"].toString(e["link"].toString()));item->setToolTip(e["text"].toString());if(m_news->count()>=30)break;}if(m_news->count()==0)m_news->addItem(text("No hay noticias guardadas. Conectate y pulsá Actualizar.","No cached news. Connect and press Refresh.","Sem notícias salvas. Conecte e clique em Atualizar."));};
    auto path=m_root+"/news.json";if(!network){try{render(ModRepository::read(path));}catch(...){render({});}return;}
    auto watcher=new QFutureWatcher<QJsonObject>(this);connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,watcher,render]{auto r=watcher->result();watcher->deleteLater();if(r.contains("entries"))render(r);else if(m_status)m_status->setText(text("Noticias sin conexión: se conserva la última copia.","News offline: keeping the last cached copy.","Notícias offline: mantendo a última cópia salva."));});
    watcher->setFuture(QtConcurrent::run([path]{try{auto o=QJsonDocument::fromJson(ModRepository::fetch(QUrl("https://launchercontent.mojang.com/v2/news.json"))).object();if(o["entries"].toArray().isEmpty())throw std::runtime_error("Empty news");ModRepository::write(path,o);return o;}catch(...){return QJsonObject{};}}));
}
void MainWindow::lostAction(bool launch){
    auto id=filename(m_lost);VersionInfo v;for(const auto &entry:m_versions->getVersions())if(entry.id==id)v=entry;if(v.id.isEmpty())return;

    auto dest=m_versions->getInstallPath(v);
    if(launch){if(!m_versions->isVersionInstalled(v)){error(text("Primero instalá esta versión.","Install this version first.","Instale esta versão primeiro."));return;}m_java->launch(v,dest);return;}
    work(text("Descargando y preparando la versión perdida…","Downloading and preparing the lost version…","Baixando e preparando a versão perdida…"),[v,dest]{
        LostInstaller::install(v,dest);return QJsonObject{};
    },[this](QJsonObject){refreshLost();m_status->setText(text("Versión perdida instalada","Lost version installed","Versão perdida instalada"));});
}
void MainWindow::closeEvent(QCloseEvent *event){if(m_jobs||!m_installing.isEmpty()){error(text("Esperá a que terminen las operaciones antes de cerrar.","Wait for operations to finish before closing.","Aguarde as operações terminarem antes de fechar."));event->ignore();return;}for(const auto &i:m_mc->instances())if(m_mc->isRunning(i.dir)){error(text("Cerrá las instancias en ejecución antes de salir.","Close running instances before exiting.","Feche as instâncias em execução antes de sair."));event->ignore();return;}event->accept();}
