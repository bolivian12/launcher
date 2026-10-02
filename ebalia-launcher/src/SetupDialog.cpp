#include "SetupDialog.hpp"
#include "JavaRuntime.hpp"
#include "JavaDownloader.hpp"
#include "LostInstaller.hpp"
#include "Language.hpp"
#include <QtWidgets>
#include <QtConcurrent>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
namespace {
QString t(const char *s){return Language::key(QString::fromUtf8(s));}
QString mark(bool ok){return ok?"✔  ":"✖  ";}
QString openGL(){
    QOpenGLContext context;QOffscreenSurface surface;surface.create();
    if(!context.create()||!context.makeCurrent(&surface))return mark(false)+t("OpenGL could not be verified in this session. Update the graphics driver from your hardware vendor.");
    auto gl=context.functions();auto renderer=reinterpret_cast<const char*>(gl->glGetString(GL_RENDERER));auto version=reinterpret_cast<const char*>(gl->glGetString(GL_VERSION));context.doneCurrent();
    return mark(true)+"OpenGL · "+QString::fromUtf8(renderer?renderer:"")+" · "+QString::fromUtf8(version?version:"");
}
QString openAL(){
#ifdef Q_OS_WIN
    // Checked again after installing: an installer that was only opened does not count as installed.
    auto win=qEnvironmentVariable("WINDIR","C:/Windows");QStringList found;for(auto path:{win+"/System32/OpenAL32.dll",win+"/SysWOW64/OpenAL32.dll"})if(QFile::exists(path))found<<QDir::toNativeSeparators(path);
    return found.isEmpty()?mark(false)+"OpenAL · "+t("Not installed in Windows. Lost versions need it for sound: install OpenAL for Windows and check again."):mark(true)+"OpenAL · "+found.join(" · ");
#else
    return mark(true)+"OpenAL · "+t("In Wine, sound goes through Wine's OpenAL support; if a version has no sound, install your system's OpenAL library.");
#endif
}
}
SetupDialog::SetupDialog(QWidget *p):QDialog(p){
    setObjectName("setupDialog");setWindowTitle(t("Setup & diagnostics"));resize(880,680);auto layout=new QVBoxLayout(this);layout->setSpacing(10);
    auto intro=new QLabel(t("Check Java, graphics and sound before running lost versions."));intro->setWordWrap(true);layout->addWidget(intro);
    auto list=new QTreeWidget;list->setObjectName("javaInstallations");list->setRootIsDecorated(false);list->setHeaderLabels({"Java",t("Architecture"),t("Location")});list->header()->setSectionResizeMode(2,QHeaderView::Stretch);layout->addWidget(list,1);
    auto javaStatus=new QLabel(t("Checking installed Java versions…"));javaStatus->setWordWrap(true);layout->addWidget(javaStatus);
    auto lost=new QLabel;lost->setObjectName("lostJava");lost->setWordWrap(true);layout->addWidget(lost);
    auto graphics=new QLabel;graphics->setWordWrap(true);layout->addWidget(graphics);auto audio=new QLabel;audio->setWordWrap(true);layout->addWidget(audio);
    auto actions=new QHBoxLayout;layout->addLayout(actions);
    auto check=new QPushButton(t("Check again"));check->setObjectName("checkAgain");actions->addWidget(check);
    auto install=new QPushButton(t("Install official Java 8 (Mojang)"));install->setObjectName("installJava8");install->setProperty("play",true);actions->addWidget(install);actions->addStretch();
    auto links=new QGridLayout;layout->addLayout(links);int n=0;
    for(auto pair:{qMakePair("Get Java 8","https://adoptium.net/temurin/releases/?version=8"),qMakePair("OpenAL for Windows","https://www.openal.org/downloads/"),qMakePair("Jarfix (optional)","https://johann.loefflmann.net/en/software/jarfix/index.html"),
                   qMakePair("NVIDIA drivers","https://www.nvidia.com/Download/index.aspx"),qMakePair("AMD drivers","https://www.amd.com/en/support"),qMakePair("Intel drivers","https://www.intel.com/content/www/us/en/download-center/home.html")}){
        auto b=new QPushButton(t(pair.first));links->addWidget(b,n/3,n%3);++n;connect(b,&QPushButton::clicked,this,[pair]{QDesktopServices::openUrl(QUrl(pair.second));});
    }
    auto note=new QLabel(t("Install drivers from your hardware vendor. Jarfix only repairs double-click associations; EBALIA launches Java directly. Recheck after installing dependencies."));note->setWordWrap(true);note->setObjectName("muted");layout->addWidget(note);
    auto close=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,this,&QDialog::reject);
    const auto data=JavaRuntime::dataDir();
    auto watcher=new QFutureWatcher<QList<JavaInstallation>>(this);
    auto lostState=[=,this](const QList<JavaInstallation> &runtimes){
        bool ready=false;QString text;
#ifdef Q_OS_WIN
        QString java8;for(const auto &j:runtimes)if(j.major==8){java8=j.path;break;}ready=!java8.isEmpty();
        text=ready?mark(true)+t("Java 8 for lost versions")+" · "+QDir::toNativeSeparators(java8):mark(false)+t("Java 8 for lost versions is missing. Install the official one; EBALIA also does it when you press Play.");
#else
        Q_UNUSED(runtimes)
        auto wine=LostInstaller::wine();auto java8=JavaDownloader::installed(data+"/mc/java","jre-legacy","windows-x64");ready=!java8.isEmpty();
        text=(wine.isEmpty()?mark(false)+t("Wine is not installed. Windows packages need Wine on Linux and macOS."):mark(true)+"Wine · "+wine+" · "+t("separate prefix")+" "+LostInstaller::winePrefix(data))+"\n"+
             (ready?mark(true)+t("Windows Java 8 for Wine")+" · "+java8:mark(false)+t("Windows Java 8 for Wine is missing. Install the official one; EBALIA also does it when you press Play."));
#endif
        lost->setText(text);install->setVisible(!ready);
    };
    auto recheck=[=,this]{
        if(watcher->isRunning())return;check->setEnabled(false);javaStatus->setText(t("Checking installed Java versions…"));graphics->setText(openGL());audio->setText(openAL());
        watcher->setFuture(QtConcurrent::run([]{return JavaRuntime::discover();}));
    };
    connect(watcher,&QFutureWatcher<QList<JavaInstallation>>::finished,this,[=,this]{
        check->setEnabled(true);list->clear();auto runtimes=watcher->result();for(const auto &j:runtimes)new QTreeWidgetItem(list,{j.version,j.architecture,QDir::toNativeSeparators(j.path)});
        QStringList majors;for(const auto &j:runtimes)majors<<QString::number(j.major);majors.removeDuplicates();
        javaStatus->setText(runtimes.isEmpty()?mark(false)+t("No working Java installation was found. EBALIA downloads the official Java when an instance needs it."):mark(true)+t("Detected Java")+": "+majors.join(", "));
        lostState(runtimes);
    });
    connect(check,&QPushButton::clicked,this,recheck);
    connect(install,&QPushButton::clicked,this,[=,this]{
        install->setEnabled(false);check->setEnabled(false);lost->setText(t("Downloading official Java 8 from Mojang…"));auto job=new QFutureWatcher<QString>(this);
        connect(job,&QFutureWatcher<QString>::finished,this,[=,this]{auto error=job->result();job->deleteLater();install->setEnabled(true);check->setEnabled(true);if(!error.isEmpty())QMessageBox::warning(this,"EBALIA",Language::message(error));recheck();});
        job->setFuture(QtConcurrent::run([data]{
            try{LostInstaller::java(data,true);return QString();}catch(const std::exception &e){return QString::fromUtf8(e.what());}
        }));
    });
    recheck();
}
