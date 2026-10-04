#include "BedrockPage.hpp"
#include "BedrockInstaller.hpp"
#include <QtConcurrent>
#include "Language.hpp"
#include "Ui.hpp"
#include <QtWidgets>
#include <memory>

namespace Bedrock {
Platform hostPlatform() {
#if defined(Q_OS_WIN)
    return Platform::Windows;
#elif defined(Q_OS_MACOS)
    return Platform::MacOS;
#elif defined(Q_OS_LINUX)
    return Platform::Linux;
#else
    return Platform::Unsupported;
#endif
}
Command localCommand(const QString &path, Platform platform) {
    if (platform == Platform::Unsupported || !QDir::isAbsolutePath(path)) return {};
    const QFileInfo file(path);
    if (platform == Platform::MacOS && file.isDir() && path.endsWith(".app", Qt::CaseInsensitive))
        return {"/usr/bin/open", {"-a", file.absoluteFilePath()}};
    if (!file.isFile()) return {};
    if (platform == Platform::Windows) {
        if (!path.endsWith(".exe", Qt::CaseInsensitive)) return {};
    } else if (!file.isExecutable()) return {};
    return {file.absoluteFilePath(), {}};
}
Command flatpakCommand(const QString &executable) {
    if (!QFileInfo(executable).isExecutable()) return {};
    return {executable, {"run", "io.mrarm.mcpelauncher"}};
}
}

BedrockPage::BedrockPage(const QString &dataRoot, QWidget *parent) : QWidget(parent) {
    setObjectName("bedrockLabPage");
    auto tr = [](const char *es, const char *en) {
        return QString::fromUtf8(Language::current == "es" ? es : en);
    };
    const auto platform = Bedrock::hostPlatform();
    const bool windows = platform == Bedrock::Platform::Windows;
    const QString provider = windows ? "BedrockLauncher" : "Minecraft Linux · mcpelauncher";
    auto settings = new QSettings(dataRoot + "/bedrock-lab.ini", QSettings::IniFormat, this);
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(28,24,28,24);layout->setSpacing(16);
    auto title = new QLabel("Bedrock · Beta");title->setObjectName("pageTitle");layout->addWidget(title);
    auto paragraph = [layout](const QString &text) {
        auto label = new QLabel(text);label->setWordWrap(true);label->setTextFormat(Qt::PlainText);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(label);return label;
    };
    paragraph(tr("Integración experimental. Abre un gestor externo de Bedrock; instala el gestor y abre su asistente para obtener el juego.",
                 "Experimental integration. Opens an external Bedrock manager; it installs the manager and opens its setup to obtain the game."));
    auto box = new QFrame;box->setObjectName("bedrockProviderCard");
    box->setStyleSheet("#bedrockProviderCard{background:#202b26;border:1px solid #416748;border-radius:12px;}");layout->addWidget(box);
    auto card = new QVBoxLayout(box);card->setContentsMargins(20,20,20,20);card->setSpacing(12);
    auto name = new QLabel(provider);name->setStyleSheet("font-size:20px;font-weight:700;");card->addWidget(name);
    auto path = new QLineEdit(settings->value("provider/path").toString());
    path->setObjectName("bedrockProviderPath");
    path->setPlaceholderText(tr("Ruta del gestor instalado (opcional)", "Installed manager path (optional)"));
    card->addWidget(path);
    auto controls = new QHBoxLayout;card->addLayout(controls);
    auto browse = new QPushButton(tr("Elegir gestor…","Choose manager…"));
    auto detect = new QPushButton(tr("Detectar","Detect"));
    controls->addWidget(browse);controls->addWidget(detect);controls->addStretch();
    auto status = new QLabel;status->setObjectName("bedrockProviderStatus");status->setWordWrap(true);
    status->setTextFormat(Qt::PlainText);card->addWidget(status);
    auto launch = new QPushButton(tr("Abrir gestor Bedrock","Open Bedrock manager"));
    launch->setObjectName("bedrockOpenManager");launch->setProperty("primary",true);launch->setEnabled(false);card->addWidget(launch);
    const QString license = windows
        ? tr("Windows: necesitás Minecraft para Windows o Game Pass. El inicio de sesión se realiza en BedrockLauncher.",
             "Windows: Minecraft for Windows ownership or Game Pass is required. Sign in through BedrockLauncher.")
        : tr("Linux/macOS: necesitás Minecraft comprado en Google Play. La licencia de Windows no sustituye la de Android. Iniciá sesión en el gestor externo.",
             "Linux/macOS: Google Play ownership is required. A Windows license does not replace Android ownership. Sign in through the external manager.");
    paragraph(license);
    auto accountHelp=new QPushButton(windows
        ? tr("Microsoft · Minecraft para Windows","Microsoft · Minecraft for Windows")
        : tr("Tengo Bedrock en Microsoft / Xbox","I own Bedrock on Microsoft / Xbox"));
    accountHelp->setObjectName("bedrockMicrosoftHelp");layout->addWidget(accountHelp,0,Qt::AlignLeft);
    connect(accountHelp,&QPushButton::clicked,this,[=,this] {
        QMessageBox::information(this,"Bedrock · Microsoft",windows
            ? tr("Usá en BedrockLauncher la cuenta Microsoft que tiene Minecraft para Windows o Game Pass. El gestor comprobará tu licencia.",
                 "Use the Microsoft account that owns Minecraft for Windows or Game Pass in BedrockLauncher. The manager checks your license.")
            : tr("Tu cuenta Microsoft sirve para Xbox y multijugador dentro del juego, pero no descarga la edición Android. Este gestor para Linux/macOS requiere una compra en Google Play. La edición Windows de Microsoft Store no se puede ejecutar con este proveedor.",
                 "Your Microsoft account can be used for Xbox and multiplayer in the game, but cannot download the Android edition. This Linux/macOS manager requires Google Play ownership. This provider cannot run the Windows Microsoft Store edition."));
    });

    paragraph(tr("Sin compra: este prototipo no ofrece el juego completo. Sin conexión: depende del gestor y de una instalación autorizada previa; no verificado.",
                 "Without purchase: this prototype does not offer the full game. Offline play depends on the manager and a previously authorized installation; unverified."));
    paragraph(tr("Referencia consultada el 04/10/2026: Bedrock 26.52. Compatibilidad del juego pendiente de pruebas en cada sistema y GPU. Abrir el gestor no confirma que el juego funcione.",
                 "Reference checked on 2026-10-04: Bedrock 26.52. Game compatibility still needs testing on each OS and GPU. Opening the manager does not confirm that the game works."));
    auto docs = new QPushButton(tr("Instalación y requisitos ↗","Installation and requirements ↗"));
    layout->addWidget(docs,0,Qt::AlignLeft);
    connect(docs,&QPushButton::clicked,this,[windows] {
        QDesktopServices::openUrl(QUrl(windows
            ? "https://github.com/BedrockLauncher/BedrockLauncher"
            : "https://minecraft-linux.github.io/"));
    });
    auto trial = new QPushButton(tr("Prueba gratuita oficial ↗","Official free trial ↗"));
    layout->addWidget(trial,0,Qt::AlignLeft);
    connect(trial,&QPushButton::clicked,this,[] {
        QDesktopServices::openUrl(QUrl("https://www.minecraft.net/free-trial"));
    });
    paragraph(tr("Los mundos y cuentas de Bedrock los administra el gestor externo. Tus instancias Java permanecen separadas de Bedrock.",
                 "The external manager owns Bedrock worlds and accounts. Your Java instances remain separate from Bedrock."));
    layout->addStretch();

    auto command = std::make_shared<Bedrock::Command>();
    auto probe = m_probe = new QProcess(this);
    auto timeout = new QTimer(probe);timeout->setSingleShot(true);timeout->setInterval(4000);
    connect(timeout,&QTimer::timeout,probe,[probe]{probe->kill();});
    auto ready = [=,this](Bedrock::Command value, const QString &message) {
        if(property("bedrockInstalling").toBool())return;
        *command = value;launch->setEnabled(value.valid());status->setText(message);detect->setEnabled(true);
        path->setEnabled(true);browse->setEnabled(true);
    };
    const QString flatpak = QStandardPaths::findExecutable("flatpak");
    connect(probe,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[=](int code,QProcess::ExitStatus exit) {
        timeout->stop();
        if(code==0 && exit==QProcess::NormalExit)
            ready(Bedrock::flatpakCommand(flatpak),tr("Gestor Flatpak detectado. Compatibilidad del juego sin verificar.","Flatpak manager detected. Game compatibility unverified."));
        else ready({},tr("No se encontró un gestor. Instalalo siguiendo los requisitos o elegí su ejecutable.","No manager found. Follow installation requirements or choose its executable."));
    });
    connect(probe,&QProcess::errorOccurred,this,[=](QProcess::ProcessError error) {
        if(error==QProcess::FailedToStart) {timeout->stop();ready({},tr("No se pudo comprobar Flatpak.","Could not check Flatpak."));}
    });
    auto refresh = [=, this] {
        launch->setEnabled(false);*command={};
        const auto configured = path->text().trimmed();settings->setValue("provider/path",configured);
        if(!configured.isEmpty()) {
            auto local = Bedrock::localCommand(configured,platform);
            ready(local,local.valid()?tr("Gestor seleccionado. Compatibilidad del juego sin verificar.","Manager selected. Game compatibility unverified.")
                                    :tr("Ruta inválida: elegí un ejecutable compatible con tu sistema.","Invalid path: choose an executable compatible with your OS."));
            return;
        }
        auto native = QStandardPaths::findExecutable(windows ? "BedrockLauncher.exe" : "mcpelauncher-ui-qt");
        auto local = Bedrock::localCommand(native,platform);
        if(local.valid()) {ready(local,tr("Gestor detectado. Compatibilidad del juego sin verificar.","Manager detected. Game compatibility unverified."));return;}
        if(platform==Bedrock::Platform::Linux && !flatpak.isEmpty()) {
            detect->setEnabled(false);path->setEnabled(false);browse->setEnabled(false);
            status->setText(tr("Buscando instalación Flatpak…","Looking for a Flatpak installation…"));
            probe->start(flatpak,{"info","io.mrarm.mcpelauncher"});timeout->start();return;
        }
        ready({},tr("No se encontró un gestor. Elegí una instalación compatible.","No manager found. Choose a compatible installation."));
    };
    auto install = new QPushButton(tr("Instalar gestor Bedrock","Install Bedrock manager"));
    install->setObjectName("bedrockInstall");card->insertWidget(1,install);
    auto watcher=new QFutureWatcher<QString>(this);
    auto installTask=std::make_shared<QPointer<QWidget>>();

    connect(install,&QPushButton::clicked,this,[=,this] {
        setProperty("bedrockInstalling",true);
        if(auto main=qobject_cast<QMainWindow*>(window())) {
            auto task=new QWidget(main);task->setMaximumWidth(300);task->setObjectName("bedrockInstallProgress");
            auto taskLayout=new QVBoxLayout(task);taskLayout->setContentsMargins(8,4,8,4);
            taskLayout->addWidget(new QLabel(tr("Instalando Bedrock…","Installing Bedrock…")));
            auto bar=new QProgressBar;bar->setRange(0,0);bar->setFixedHeight(8);bar->setTextVisible(false);taskLayout->addWidget(bar);
            main->statusBar()->addPermanentWidget(task);*installTask=task;
        }
        install->setEnabled(false);launch->setEnabled(false);detect->setEnabled(false);
        path->setEnabled(false);browse->setEnabled(false);
        status->setText(tr("Instalando el gestor y sus dependencias… Puede descargar varios GB. Al terminar, iniciá sesión en el gestor para descargar el juego.",
                          "Installing the manager and dependencies… May download several GB. Afterwards, sign in through the manager to download the game."));
        watcher->setFuture(QtConcurrent::run([dataRoot] {
            try {return BedrockInstaller::install(dataRoot+"/bedrock");}
            catch(const std::exception &e){return QString("ERROR: ")+QString::fromUtf8(e.what());}
        }));
    });
    connect(watcher,&QFutureWatcher<QString>::finished,this,[=,this] {
        setProperty("bedrockInstalling",false);
        if(*installTask) {if(auto main=qobject_cast<QMainWindow*>(window()))main->statusBar()->removeWidget(*installTask);(*installTask)->deleteLater();}

        install->setEnabled(true);detect->setEnabled(true);path->setEnabled(true);browse->setEnabled(true);
        const auto result=watcher->result();
        if(result.startsWith("ERROR: ")) {ready({},result);return;}
        if(result=="flatpak")ready(Bedrock::flatpakCommand(flatpak),tr("Gestor instalado. Iniciá sesión para descargar Bedrock.","Manager installed. Sign in to download Bedrock."));
        else {
            path->setText(result);settings->setValue("provider/path",result);
            ready(Bedrock::localCommand(result,platform),tr("Gestor instalado. Windows puede requerir activar el modo desarrollador; macOS puede pedir autorización del sistema.",
                "Manager installed. Windows may require Developer Mode; macOS may require system approval."));
        }
        if(command->valid())launch->click();
    });
    connect(detect,&QPushButton::clicked,this,refresh);
    connect(path,&QLineEdit::textEdited,this,[=, this] {launch->setEnabled(false);*command={};});
    connect(browse,&QPushButton::clicked,this,[=, this] {
        QString selected;
        if(platform==Bedrock::Platform::MacOS)
            selected=QFileDialog::getExistingDirectory(this,tr("Elegir aplicación .app","Choose .app bundle"));
        else selected=QFileDialog::getOpenFileName(this,tr("Elegir gestor Bedrock","Choose Bedrock manager"),{},windows?"Executables (*.exe)":"*");
        if(!selected.isEmpty()){path->setText(selected);refresh();}
    });
    connect(launch,&QPushButton::clicked,this,[=, this] {
        if(!command->valid()) return;
        QProcess process;
        process.setProgram(command->program);process.setArguments(command->arguments);
        process.setWorkingDirectory(QFileInfo(command->program).absolutePath());
        const bool started=process.startDetached();
        status->setText(started
            ? tr("Gestor iniciado. Elegí la versión e iniciá sesión allí. El juego aún no está verificado.","Manager started. Choose a version and sign in there. The game is not yet verified.")
            : tr("No se pudo abrir el gestor. Comprobá la instalación y sus dependencias.","Could not open the manager. Check its installation and dependencies."));
    });
    QTimer::singleShot(0,this,refresh);
}

BedrockPage::~BedrockPage() {
    // Stop process callbacks before the labels/buttons are destroyed.
    if (m_probe) {
        m_probe->disconnect(this);
        if (m_probe->state()!=QProcess::NotRunning) {
            m_probe->kill();
            m_probe->waitForFinished(1000);
        }
    }
}
