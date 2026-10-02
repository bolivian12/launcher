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
#include "InstanceViews.hpp"
#include "InstanceIcons.hpp"
#include "Icons.hpp"
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
    connect(m_java,&JavaRunner::processStarted,this,[this]{m_status->setText(text("Versión perdida en ejecución","Lost version is running","Versão perdida em execução"));m_lostPlay->setText("■  "+Language::key("Stop"));});
    connect(m_java,&JavaRunner::processFinished,this,[this]{m_lostPlay->setText(text("▶ Jugar","▶ Play","▶ Jogar"));m_status->setText(text("Todo listo","Ready","Tudo pronto"));});

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
    for(int n=0;n<2;++n)if(auto w=l->itemAt(n)->widget())w->hide(); // the hero banner already carries the title
    l->setContentsMargins(28,12,28,16);m_clientTabs=new QTabWidget;l->addWidget(m_clientTabs,1);
    auto home=new QWidget;auto homeLayout=new QVBoxLayout(home);homeLayout->setContentsMargins(0,14,0,0);homeLayout->setSpacing(16);
    auto banner=new GameBanner;auto heroLayout=new QVBoxLayout(banner);heroLayout->setContentsMargins(34,30,34,30);
    auto wordmark=label("MINECRAFT",heroLayout);wordmark->setStyleSheet("background:transparent;color:white;font-size:42px;font-weight:900;");
    auto edition=label("JAVA EDITION",heroLayout);edition->setStyleSheet("background:transparent;color:white;font-size:18px;font-weight:700;");heroLayout->addStretch();
    auto welcome=label(text("Tu próxima aventura","Your next adventure","Sua próxima aventura"),heroLayout);welcome->setStyleSheet("background:transparent;color:white;font-size:27px;font-weight:700;");homeLayout->addWidget(banner,1);
    auto launchBar=new QHBoxLayout;homeLayout->addLayout(launchBar);m_playInstance=new QComboBox;m_playInstance->setMinimumWidth(250);m_playInstance->setPlaceholderText(text("Crear mi primera instancia","Create my first instance","Criar minha primeira instância"));m_playInstance->setIconSize(QSize(24,24));m_playInstance->setFixedWidth(330);launchBar->addWidget(m_playInstance);
    auto folder=button("",launchBar,[this]{auto i=selected();if(!i.dir.isEmpty())QDesktopServices::openUrl(QUrl::fromLocalFile(i.dir));},this);
    folder->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));folder->setToolTip(text("📂 Abrir carpeta de instancia","📂 Open instance folder","📂 Abrir pasta da instância"));folder->setAccessibleName(folder->toolTip());
    launchBar->addStretch();m_playButton=button(text("▶ Jugar","▶ Play","▶ Jogar"),launchBar,[this]{play();},this);m_playButton->setObjectName("homePlay");m_playButton->setProperty("play",true);m_playButton->setMinimumSize(280,60);launchBar->addStretch();
    m_playerName=new QLabel;m_playerName->setObjectName("playerName");m_playerName->setMinimumWidth(150);m_playerName->setAlignment(Qt::AlignRight|Qt::AlignVCenter);launchBar->addWidget(m_playerName);
    connect(m_playInstance,qOverload<int>(&QComboBox::activated),this,[this]{m_selectedDir=m_playInstance->currentData().toString();refreshInstances();});
    auto shortcuts=new QHBoxLayout;homeLayout->addLayout(shortcuts);
    button(text("＋ Nueva instancia","＋ New instance","＋ Nova instância"),shortcuts,[this]{createInstance();},this);
    button(text("Administrar mods","Manage mods","Gerenciar mods"),shortcuts,[this]{manageMods();},this);
    button(text("Guardar mods como pack","Save mods as a pack","Salvar mods como pack"),shortcuts,[this]{savePack();},this);
    m_homeNews=new QHBoxLayout;homeLayout->addLayout(m_homeNews);
    m_clientTabs->addTab(home,text("▶ Jugar","▶ Play","▶ Jogar"));
    auto library=new QWidget;auto libraryLayout=new QVBoxLayout(library);libraryLayout->setContentsMargins(0,0,0,0);libraryLayout->setSpacing(8);
    m_clientTabs->addTab(library,text("◈  Mis instancias","◈  My instances","◈  Minhas instâncias"));l=libraryLayout;
    m_library=new QStackedWidget;l->addWidget(m_library,1);m_grid=new InstanceGrid;m_detail=new InstanceDetail;m_library->addWidget(m_grid);m_library->addWidget(m_detail);
    auto choose=[this](const QString &dir){m_selectedDir=dir;selection();};
    m_grid->open=[this,choose](const QString &dir){choose(dir);m_library->setCurrentWidget(m_detail);refreshInstances();};
    m_grid->play=[this,choose](const QString &dir){choose(dir);play();};
    m_grid->settings=[this,choose](const QString &dir){choose(dir);editInstance();};
    m_grid->menu=[this,choose](const QString &dir){choose(dir);instanceMenu();};
    m_grid->create=[this]{createInstance();};m_grid->importPack=[this]{createInstance(false,CreateInstanceDialog::Import);};
    m_grid->newGroup=[this]{bool ok;auto name=QInputDialog::getText(this,Language::key("New group"),Language::key("Group name"),QLineEdit::Normal,{},&ok).trimmed();if(!ok||name.isEmpty())return;auto groups=QSettings().value("ui/groups").toStringList();if(!groups.contains(name))groups<<name;QSettings().setValue("ui/groups",groups);refreshInstances();};
    m_grid->groupMenu=[this](const QString &group){groupMenu(group);};
    m_detail->back=[this]{m_library->setCurrentWidget(m_grid);refreshInstances();};
    m_detail->play=[this]{play();};m_detail->settings=[this]{editInstance();};m_detail->log=[this]{showLog(selected().dir,this);};
    m_detail->exportZip=[this]{exportInstance();};m_detail->copy=[this]{copyInstance();};m_detail->remove=[this]{removeInstance();};
    m_detail->mods=[this]{manageMods();};m_detail->savePack=[this]{savePack();};m_detail->changeGroup=[this]{changeGroup();};
    m_detail->findMods=[this]{auto i=selected();if(i.dir.isEmpty())return;if(i.loader=="vanilla"){error(text("Esta instancia es Vanilla. Creá una instancia con Fabric, Quilt, Forge o NeoForge para usar mods.","This is a Vanilla instance. Create an instance with Fabric, Quilt, Forge or NeoForge to use mods.","Esta instância é Vanilla. Crie uma instância com Fabric, Quilt, Forge ou NeoForge para usar mods."));return;}showPage(2);m_target->setCurrentIndex(m_target->findData(i.dir));};
    m_detail->openFolder=[this](const QString &folder){auto i=selected();if(i.dir.isEmpty())return;auto path=folder.isEmpty()?i.dir:i.dir+"/"+folder;QDir().mkpath(path);QDesktopServices::openUrl(QUrl::fromLocalFile(path));};
    auto deleteShortcut=new QShortcut(QKeySequence::Delete,m_library);deleteShortcut->setContext(Qt::WidgetWithChildrenShortcut);connect(deleteShortcut,&QShortcut::activated,this,[this]{removeInstance();});
    m_catalog=label(text("Consultando versiones…","Checking versions…","Consultando versões…"),l,"muted");
    m_clientTabs->addTab(new SkinsPage(m_root,m_accounts,m_mc,[this](const QString &title,std::function<QJsonObject()> job,std::function<void(QJsonObject)> done){work(title,job,done);},this),Language::key("Skins"));
    page(m_pages,text("Versiones perdidas","Lost versions","Versões perdidas"),text("El archivo de EBALIA. Cada versión conserva su instalación y su forma de inicio original.","The EBALIA archive. Each version keeps its own installation and original launch method.","O arquivo da EBALIA. Cada versão mantém sua instalação e sua forma original de iniciar."),l);
    m_lostFilter=new QLineEdit;m_lostFilter->setPlaceholderText(text("Buscar por nombre o categoría…","Search by name or category…","Buscar por nome ou categoria…"));m_lostFilter->setClearButtonEnabled(true);l->addWidget(m_lostFilter);
    auto lostSplit=new QHBoxLayout;lostSplit->setSpacing(16);l->addLayout(lostSplit,1);
    m_lost=new QListWidget;m_lost->setObjectName("lostList");m_lost->setIconSize(QSize(36,36));m_lost->setMinimumWidth(300);lostSplit->addWidget(m_lost,2);connect(m_lostFilter,&QLineEdit::textChanged,this,[this]{refreshLost();});
    auto lostCard=new QFrame;lostCard->setObjectName("card");auto lc=new QVBoxLayout(lostCard);lc->setContentsMargins(24,22,24,22);lc->setSpacing(10);lostSplit->addWidget(lostCard,3);
    m_lostTitle=label("",lc,"detailTitle");m_lostInfo=label("",lc,"muted");lc->addStretch();
    auto lostActions=new QHBoxLayout;lc->addLayout(lostActions);
    m_lostPlay=button(text("▶ Jugar","▶ Play","▶ Jogar"),lostActions,[this]{lostAction(true);},this);m_lostPlay->setProperty("play",true);m_lostPlay->setMinimumSize(180,50);
    m_lostInstall=button(text("Instalar","Install","Instalar"),lostActions,[this]{lostAction(false);},this);
    auto lostFolder=button("",lostActions,[this]{auto id=filename(m_lost);for(const auto &v:m_versions->getVersions())if(v.id==id&&m_versions->isVersionInstalled(v))QDesktopServices::openUrl(QUrl::fromLocalFile(m_versions->getInstallPath(v)));},this);
    lostFolder->setIcon(QIcon(icons::folder(22)));lostFolder->setToolTip(Language::key("Open instance folder"));lostFolder->setAccessibleName(lostFolder->toolTip());lostActions->addStretch();
    button(Language::key("Setup & diagnostics"),lostActions,[this]{SetupDialog dialog(this);dialog.exec();},this);
    connect(m_lost,&QListWidget::currentRowChanged,this,[this]{lostSelection();});
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
QToolTip{background:#293542;color:white;border:1px solid #58687a;}
QPushButton[play=true]{background:#3c8527;color:white;border:1px solid #52a535;border-bottom:3px solid #27591b;font-weight:800;}QPushButton[play=true]:hover{background:#4a9e31;}QPushButton[play=true]:disabled{background:#2d4a28;color:#a9c3a3;}
#homePlay{font-size:17px;}#detailPlay{font-size:16px;}QPushButton[danger=true]{background:#3a1f24;border-color:#7a3340;color:#ffb0b8;}QPushButton[danger=true]:hover{background:#552a31;}
QPushButton[tab=true]{background:#161e28;border:1px solid #2c3543;border-radius:8px;padding:8px 14px;}QPushButton[tab=true]:checked{background:#24382f;color:#a5f6ce;border-color:#3f7a63;}
QPushButton[link=true]{background:transparent;border:0;color:#5fd38d;font-weight:800;text-align:left;padding:4px 0;}QPushButton[link=true]:hover{color:#8ff0b5;background:transparent;}
#groupHeader{background:transparent;border:0;text-align:left;font-size:15px;font-weight:700;padding:6px 2px;}#groupHeader:hover{color:#a5f6ce;background:transparent;}
#instanceCard{background:#1a222c;border:1px solid #2c3846;border-radius:12px;}#instanceCard:hover{border-color:#4a9e31;}#instanceCard[selected=true]{border:2px solid #5fbf7f;}#instanceCard QLabel{background:transparent;}
#cardTitle{font-size:15px;font-weight:700;}#cardFooter{color:#7f8b9b;font-size:11px;}#instanceCards,#instanceGrid,#instanceDetail{background:transparent;}
#cardSettings,#instanceDetail QToolButton{background:#222c38;border:1px solid #364454;border-radius:8px;padding:9px;}#cardSettings:hover,#instanceDetail QToolButton:hover{border-color:#92ebbe;}
#detailTitle{font-size:26px;font-weight:800;}#detailIcon{background:#141b23;border:1px solid #303e4c;border-radius:14px;}
#chip{background:#1f2a35;border:1px solid #364454;border-radius:7px;padding:5px 10px;font-weight:600;}#chip[accent=true]{background:#2f6b2a;border-color:#4a9e31;color:white;}
#statCard{background:#1a222c;border:1px solid #303e4c;border-radius:12px;}#statCard QLabel{background:transparent;}#statTitle{font-size:16px;font-weight:700;}
QScrollArea{background:transparent;border:0;}QDialog{background:#11151b;}QLabel{background:transparent;}#playerName{font-weight:800;font-size:14px;color:#cfd8e3;}
QCheckBox::indicator,QRadioButton::indicator{width:15px;height:15px;border:1px solid #5d6f84;background:#1c2530;}QCheckBox::indicator{border-radius:4px;}QRadioButton::indicator{border-radius:8px;}
QCheckBox::indicator:checked,QRadioButton::indicator:checked{background:#5fbf7f;border-color:#8fe0ad;}QCheckBox::indicator:hover,QRadioButton::indicator:hover{border-color:#92ebbe;}
QTreeWidget{background:#161e28;border:1px solid #303d4c;border-radius:9px;}QTreeWidget::item{padding:6px 4px;}QTreeWidget::item:selected{background:#2b463e;color:#c0f7db;}
QHeaderView::section{background:#1c2530;color:#9daabc;border:0;border-bottom:1px solid #303d4c;padding:7px 6px;font-weight:700;}QSplitter::handle{background:transparent;}
#instanceSources::item{padding:6px 8px;border-bottom:0;}QTextBrowser{padding:18px;font-size:14px;}
)");
    refreshAccounts();refreshInstances();refreshPacks();refreshLost();m_nav->setCurrentRow(0);
}
void MainWindow::showPage(int i){m_nav->setCurrentRow(i);}
void MainWindow::error(const QString &e){QMessageBox::warning(this,"EBALIA",Language::message(e));}
void MainWindow::refreshAccounts(){if(m_playerName)m_playerName->setText(m_accounts->active().name);m_account->blockSignals(true);m_account->clear();for(const auto &a:m_accounts->accounts())m_account->addItem(a.name+(a.type=="msa"?" · Microsoft":" · "+Language::key("Local profile")),a.uuid);if(m_account->count()==0)m_account->addItem(text("Sin cuenta","No account","Sem conta"));m_account->setCurrentIndex(qMax(0,m_account->findData(m_accounts->active().uuid)));m_account->blockSignals(false);}
McInstance MainWindow::selected() const {if(m_selectedDir.isEmpty())return {};for(const auto &i:m_mc->instances())if(i.dir==m_selectedDir)return i;return {};}
void MainWindow::refreshInstances(){
    auto target=m_target->currentData().toString();m_target->clear();m_playInstance->blockSignals(true);m_playInstance->clear();
    auto all=m_mc->instances();std::stable_sort(all.begin(),all.end(),[](const McInstance &a,const McInstance &b){return a.lastPlayed>b.lastPlayed;}); // most recently played first, like the Minecraft Launcher
    if(std::none_of(all.begin(),all.end(),[this](const McInstance &i){return i.dir==m_selectedDir;}))m_selectedDir=all.isEmpty()?QString():all.first().dir;
    QList<InstanceInfo> infos;QStringList used;InstanceInfo current;
    for(const auto &i:all){
        QJsonObject meta;try{meta=ModRepository::read(i.dir+"/instance.json");}catch(...){}
        InstanceInfo info{i,meta["group"].toString().trimmed(),meta["icon"].toString(),meta["loaderVersion"].toString(),!meta["packProvider"].toString().isEmpty(),m_mc->isRunning(i.dir),m_installing.contains(i.dir)||m_mc->isInstalling(i.dir)};
        infos<<info;used<<info.group;if(i.dir==m_selectedDir)current=info;
        const auto label=i.name+"  ·  "+InstanceText::loader(i.loader)+" "+i.mcVersion;
        m_playInstance->addItem(InstanceIcons::icon(info.icon,i.dir),label,i.dir);if(i.loader!="vanilla")m_target->addItem(label,i.dir);
    }
    QStringList emptyGroups;for(const auto &g:QSettings().value("ui/groups").toStringList())if(!used.contains(g))emptyGroups<<g;
    m_grid->setInstances(infos,emptyGroups,m_selectedDir);
    if(m_library->currentWidget()==m_detail){if(current.base.dir.isEmpty())m_library->setCurrentWidget(m_grid);else m_detail->showInstance(current);}
    int idx=m_target->findData(target);if(idx>=0)m_target->setCurrentIndex(idx);m_playInstance->blockSignals(false);selection();
}
void MainWindow::selection(){
    auto i=selected();m_playInstance->setCurrentIndex(m_playInstance->findData(i.dir));const bool running=m_mc->isRunning(i.dir);
    m_playButton->setText(i.dir.isEmpty()?"＋  "+text("Crear instancia","Create instance","Criar instância"):running?"■  "+Language::key("Stop"):i.ready?text("▶ Jugar","▶ Play","▶ Jogar"):"⬇  "+text("Instalar","Install","Instalar"));
    m_playButton->setEnabled(!m_installing.contains(i.dir)&&!m_mc->isInstalling(i.dir));m_playButton->setProperty("danger",running);m_playButton->setProperty("play",!running);m_playButton->style()->unpolish(m_playButton);m_playButton->style()->polish(m_playButton);
}
void MainWindow::refreshPacks(){m_packs->clear();try{m_packData=ModRepository(m_root).packs();for(const auto &v:m_packData){auto p=v.toObject();m_packs->addItem(p["name"].toString()+"\n"+QString::number(p["projects"].toArray().size())+" mods");}if(m_packs->count())m_packs->setCurrentRow(0);else m_packDetails->setText(text("Todavía no hay packs. Guardá los mods de una instancia para empezar.","No packs yet. Save an instance's mods to get started.","Ainda não há packs. Salve os mods de uma instância para começar."));}catch(...){m_packDetails->setText(exception());}}
void MainWindow::refreshLost(){
    auto keep=filename(m_lost);{QSignalBlocker block(m_lost);m_lost->clear();}
    for(const auto &v:m_versions->getVersions()){
        if(!(v.name+v.category).contains(m_lostFilter->text().trimmed(),Qt::CaseInsensitive))continue;
        auto icon=v.category=="horror"?icons::horror(36):v.category=="release"?icons::release(36):icons::alpha(36);
        auto item=new QListWidgetItem(QIcon(icon),v.name+"\n"+lostCategory(v.category)+(m_versions->isVersionInstalled(v)?"  ·  "+text("Instalada","Installed","Instalada"):""),m_lost);item->setData(Qt::UserRole,v.id);if(v.id==keep)m_lost->setCurrentItem(item);
    }
    if(m_lost->currentRow()<0&&m_lost->count())m_lost->setCurrentRow(0);lostSelection();
}
QString MainWindow::lostCategory(const QString &category){return category=="horror"?Language::key("Horror"):category=="release"?Language::key("Release"):category=="alpha"?Language::key("Alpha"):category;}
void MainWindow::lostSelection(){
    auto id=filename(m_lost);VersionInfo v;for(const auto &entry:m_versions->getVersions())if(entry.id==id)v=entry;
    m_lostPlay->setEnabled(!v.id.isEmpty());m_lostInstall->setEnabled(!v.id.isEmpty());if(v.id.isEmpty()){m_lostTitle->setText(Language::key("No versions match this search."));m_lostInfo->clear();return;}
    const bool installed=m_versions->isVersionInstalled(v);m_lostTitle->setText(v.name);
    QStringList info{lostCategory(v.category)+"  ·  "+v.description,installed?text("Instalada","Installed","Instalada"):text("Pendiente de instalación","Not installed yet","Ainda não instalada")};
    if(LostInstaller::windowsPackage(v))info<<Language::key("Original Windows package. Runs with Java 8, which EBALIA prepares automatically. On Linux and macOS it runs in Wine with its own prefix.");
    else info<<Language::key("Runs with Java 8, which EBALIA prepares automatically.");
    if(!installed&&v.archiveSize>0)info<<Language::key("The first installation downloads the original archive (%1 MB); later versions reuse it.").arg(v.archiveSize/1024/1024);
    m_lostInfo->setText(info.join("\n\n"));m_lostInstall->setText(installed?Language::key("Reinstall"):text("Instalar","Install","Instalar"));m_lostPlay->setEnabled(installed);
}
void MainWindow::work(const QString &title,std::function<QJsonObject()> job,std::function<void(QJsonObject)> done){
    if(m_jobs)return;++m_jobs;auto dialog=new QProgressDialog(title,QString(),0,0,this);dialog->setCancelButton(nullptr);dialog->setWindowModality(Qt::ApplicationModal);dialog->setMinimumDuration(0);dialog->show();
    auto watcher=new QFutureWatcher<QJsonObject>(this);connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,dialog,watcher,done]{auto result=watcher->result();watcher->deleteLater();dialog->close();dialog->deleteLater();--m_jobs;if(result.contains("_error"))error(result["_error"].toString());else done(result);});
    watcher->setFuture(QtConcurrent::run([job]{try{return job();}catch(...){return QJsonObject{{"_error",exception()}};}}));
}
void MainWindow::createInstance(bool copy,int page){
    auto source=selected();if(copy&&source.dir.isEmpty())return;
    if(copy&&(m_mc->isRunning(source.dir)||m_installing.contains(source.dir))){error(text("Cerrá el juego antes de copiar sus mods.","Close the game before copying its mods.","Feche o jogo antes de copiar seus mods."));return;}
    if(m_manifest.isEmpty())m_mc->fetchManifest(); // the dialog fills the version list when the catalog arrives; imports do not need it
    CreateInstanceDialog dialog(m_mc,m_manifest,m_packData,source,copy,this);if(page)dialog.showPage(page);
    if(dialog.exec()!=QDialog::Accepted)return;auto c=dialog.configuration();
    auto reveal=[this](const QString &dir){m_selectedDir=dir;showPage(0);m_clientTabs->setCurrentIndex(1);m_library->setCurrentWidget(m_detail);refreshInstances();};
    if(!c["providerPack"].toObject().isEmpty()){auto root=m_root;work(Language::key("Installing modpack…"),[root,c]{auto dir=PackService(root).install(c["providerPack"].toObject(),c["providerVersion"].toObject(),c["name"].toString(),c["group"].toString(),c["xmx"].toInt(4096));auto info=ModRepository::read(dir+"/instance.json");if(info["icon"].toString()!="custom"){info["icon"]=c["icon"];ModRepository::write(dir+"/instance.json",info);}return QJsonObject{{"dir",dir}};},[this,reveal](QJsonObject result){reveal(result["dir"].toString());m_status->setText(Language::key("Modpack ready. Press Install to prepare Minecraft."));});return;}
    try{auto dir=m_mc->createInstance(c["name"].toString(),c["mcVersion"].toString(),c["loader"].toString(),c["loaderVersion"].toString());auto info=ModRepository::read(dir+"/instance.json");for(auto key:{"xmx","group","icon"})info[key]=c[key];ModRepository::write(dir+"/instance.json",info);reveal(dir);
        if(copy){auto root=m_root;work(text("Identificando los mods…","Identifying mods…","Identificando mods…"),[root,source]{return ModRepository(root).capture(source.dir,source.name);},[this,dir](QJsonObject p){preview(p["projects"].toArray(),dir);});}
        else if(!c["pack"].toObject().isEmpty())preview(c["pack"].toObject()["projects"].toArray(),dir);
    }catch(...){error(exception());}
}
void MainWindow::instanceMenu(){
    auto i=selected();if(i.dir.isEmpty())return;QMenu menu(this);
    auto playAction=menu.addAction(m_mc->isRunning(i.dir)?Language::key("Stop"):i.ready?Language::key("Play"):Language::key("Install"));auto open=menu.addAction(Language::key("Open"));menu.addSeparator();
    auto settings=menu.addAction(Language::key("Instance settings"));auto group=menu.addAction(Language::key("Change group"));auto folder=menu.addAction(Language::key("Open instance folder"));
    auto copy=menu.addAction(Language::key("Copy instance"));auto exportZip=menu.addAction(Language::key("Export instance"));menu.addSeparator();auto trash=menu.addAction(Language::key("Delete instance"));
    auto chosen=menu.exec(QCursor::pos());
    if(chosen==playAction)play();else if(chosen==open){m_library->setCurrentWidget(m_detail);refreshInstances();}else if(chosen==settings)editInstance();else if(chosen==group)changeGroup();
    else if(chosen==folder)QDesktopServices::openUrl(QUrl::fromLocalFile(i.dir));else if(chosen==copy)copyInstance();else if(chosen==exportZip)exportInstance();else if(chosen==trash)removeInstance();
}
void MainWindow::groupMenu(const QString &group){
    QMenu menu(this);auto rename=menu.addAction(Language::key("Rename group"));auto ungroup=menu.addAction(Language::key("Remove group (keep instances)"));auto chosen=menu.exec(QCursor::pos());if(!chosen)return;
    QString name;if(chosen==rename){bool ok;name=QInputDialog::getText(this,Language::key("Rename group"),Language::key("Group name"),QLineEdit::Normal,group,&ok).trimmed();if(!ok||name.isEmpty()||name==group)return;}
    for(const auto &i:m_mc->instances()){try{auto info=ModRepository::read(i.dir+"/instance.json");if(info["group"].toString().trimmed()!=group)continue;info["group"]=name;ModRepository::write(i.dir+"/instance.json",info);}catch(...){error(exception());}}
    auto groups=QSettings().value("ui/groups").toStringList();groups.removeAll(group);if(!name.isEmpty()&&!groups.contains(name))groups<<name;QSettings().setValue("ui/groups",groups);refreshInstances();
}
void MainWindow::changeGroup(){
    auto i=selected();if(i.dir.isEmpty())return;QJsonObject info;try{info=ModRepository::read(i.dir+"/instance.json");}catch(...){error(exception());return;}
    QStringList groups=QSettings().value("ui/groups").toStringList();for(const auto &other:m_mc->instances()){try{auto g=ModRepository::read(other.dir+"/instance.json")["group"].toString().trimmed();if(!g.isEmpty()&&!groups.contains(g))groups<<g;}catch(...){}}
    groups.sort(Qt::CaseInsensitive);groups.prepend(Language::key("No group"));bool ok;auto current=info["group"].toString();
    auto chosen=QInputDialog::getItem(this,Language::key("Change group"),Language::key("Group name"),groups,qMax(0,groups.indexOf(current)),true,&ok).trimmed();if(!ok)return;
    if(chosen==Language::key("No group"))chosen.clear();info["group"]=chosen;try{ModRepository::write(i.dir+"/instance.json",info);}catch(...){error(exception());return;}
    if(!chosen.isEmpty()&&!QSettings().value("ui/groups").toStringList().contains(chosen)){auto all=QSettings().value("ui/groups").toStringList();all<<chosen;QSettings().setValue("ui/groups",all);}
    refreshInstances();
}
void MainWindow::copyInstance(){
    auto i=selected();if(i.dir.isEmpty())return;if(m_mc->isRunning(i.dir)||m_installing.contains(i.dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}
    bool ok;auto name=QInputDialog::getText(this,Language::key("Copy instance"),Language::key("Name of the copy"),QLineEdit::Normal,i.name+" (2)",&ok).trimmed();if(!ok||name.isEmpty())return;
    auto mc=m_mc;auto dir=i.dir;work(Language::key("Copying worlds, mods and settings…"),[mc,dir,name]{return QJsonObject{{"dir",mc->copyInstance(dir,name)}};},[this](QJsonObject r){m_selectedDir=r["dir"].toString();m_library->setCurrentWidget(m_detail);refreshInstances();m_status->setText(Language::key("Instance copied."));});
}
void MainWindow::exportInstance(){
    auto i=selected();if(i.dir.isEmpty())return;if(m_mc->isRunning(i.dir)||m_installing.contains(i.dir)){error(text("La instancia está en uso.","The instance is in use.","A instância está em uso."));return;}
    auto safe=i.name;safe.replace(QRegularExpression("[^A-Za-z0-9 ._-]"),"_");auto file=QFileDialog::getSaveFileName(this,Language::key("Export instance"),QDir::homePath()+"/"+safe+".zip","ZIP (*.zip)");if(file.isEmpty())return;
    auto mc=m_mc;auto dir=i.dir;work(Language::key("Exporting instance…"),[mc,dir,file]{mc->exportInstance(dir,file);return QJsonObject{};},[this](QJsonObject){m_status->setText(Language::key("Instance exported. Import the ZIP in EBALIA to restore it."));});
}
void MainWindow::showLog(const QString &dir,QWidget *parent){
    if(dir.isEmpty())return;QDialog log(parent);log.setWindowTitle(text("Registro","Log","Registro"));log.resize(860,560);QVBoxLayout ll(&log);QPlainTextEdit view;view.setReadOnly(true);view.setObjectName("logView");
    QFile f(dir+"/launcher.log");if(!f.exists())f.setFileName(dir+"/logs/latest.log");
    if(f.open(QIODevice::ReadOnly)){f.seek(qMax<qint64>(0,f.size()-120000));view.setPlainText(QString::fromUtf8(f.readAll()));view.moveCursor(QTextCursor::End);}else view.setPlainText(text("Todavía no hay registro.","No log yet.","Ainda não há registro."));
    ll.addWidget(&view);QDialogButtonBox close(QDialogButtonBox::Close);ll.addWidget(&close);connect(&close,&QDialogButtonBox::rejected,&log,&QDialog::reject);log.exec();
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
    try{m_mc->deleteInstance(i.dir);m_selectedDir.clear();m_library->setCurrentWidget(m_grid);refreshInstances();m_status->setText(Language::key("Instance moved to trash. Your worlds are preserved."));}catch(...){error(exception());}
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
    button(text("Registro","Log","Registro"),&lay,[&,i]{showLog(i.dir,&d);},&d);
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
        m_news->clear();for(const auto &v:o["entries"].toArray()){const auto e=v.toObject();if(e["title"].toString().isEmpty())continue;auto item=new QListWidgetItem(e["title"].toString()+"\n"+e["date"].toString().left(10)+"  ·  "+e["text"].toString(),m_news);item->setData(Qt::UserRole,e["readMoreLink"].toString(e["link"].toString()));item->setToolTip(e["text"].toString());if(m_news->count()>=30)break;}if(m_news->count()==0)m_news->addItem(text("No hay noticias guardadas. Conectate y pulsá Actualizar.","No cached news. Connect and press Refresh.","Sem notícias salvas. Conecte e clique em Atualizar."));};
    auto path=m_root+"/news.json";if(!network){try{render(ModRepository::read(path));}catch(...){render({});}return;}
    auto watcher=new QFutureWatcher<QJsonObject>(this);connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,watcher,render]{auto r=watcher->result();watcher->deleteLater();if(r.contains("entries"))render(r);else if(m_status)m_status->setText(text("Noticias sin conexión: se conserva la última copia.","News offline: keeping the last cached copy.","Notícias offline: mantendo a última cópia salva."));});
    watcher->setFuture(QtConcurrent::run([path]{try{auto o=QJsonDocument::fromJson(ModRepository::fetch(QUrl("https://launchercontent.mojang.com/v2/news.json"))).object();if(o["entries"].toArray().isEmpty())throw std::runtime_error("Empty news");ModRepository::write(path,o);return o;}catch(...){return QJsonObject{};}}));
}
void MainWindow::lostAction(bool launch){
    auto id=filename(m_lost);VersionInfo v;for(const auto &entry:m_versions->getVersions())if(entry.id==id)v=entry;if(v.id.isEmpty())return;

    auto dest=m_versions->getInstallPath(v);
    if(launch){
        if(m_java->isRunning()){if(QMessageBox::question(this,"EBALIA",Language::key("Stop")+" · "+v.name+"?")==QMessageBox::Yes)m_java->stop();return;}
        if(!m_versions->isVersionInstalled(v)){error(text("Primero instalá esta versión.","Install this version first.","Instale esta versão primeiro."));return;}
        const bool windows=LostInstaller::windowsPackage(v);
#ifndef Q_OS_WIN
        if(windows&&LostInstaller::wine().isEmpty()){error(Language::key("This Windows package requires Wine. Install Wine or run it on Windows."));return;}
#endif
        auto root=m_root;
        work(Language::key("Preparing Java 8 for this version…"),[root,windows]{try{return QJsonObject{{"java",LostInstaller::java(root,windows)}};}catch(const std::exception &e){return QJsonObject{{"warning",QString::fromUtf8(e.what())}};}},
            [this,v,dest](QJsonObject r){if(r.contains("warning"))m_status->setText(Language::message(r["warning"].toString()));m_java->launch(v,dest,r["java"].toString());});
        return;
    }
    work(text("Descargando y preparando la versión perdida…","Downloading and preparing the lost version…","Baixando e preparando a versão perdida…"),[v,dest]{
        LostInstaller::install(v,dest);return QJsonObject{};
    },[this](QJsonObject){refreshLost();m_status->setText(text("Versión perdida instalada","Lost version installed","Versão perdida instalada"));});
}
void MainWindow::closeEvent(QCloseEvent *event){if(m_jobs||!m_installing.isEmpty()){error(text("Esperá a que terminen las operaciones antes de cerrar.","Wait for operations to finish before closing.","Aguarde as operações terminarem antes de fechar."));event->ignore();return;}for(const auto &i:m_mc->instances())if(m_mc->isRunning(i.dir)){error(text("Cerrá las instancias en ejecución antes de salir.","Close running instances before exiting.","Feche as instâncias em execução antes de sair."));event->ignore();return;}event->accept();}
