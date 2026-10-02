#include "SetupDialog.hpp"
#include "JavaRuntime.hpp"
#include "Language.hpp"
#include <QtWidgets>
#include <QtConcurrent>
#include <QOpenGLContext>
#include <QOffscreenSurface>
#include <QOpenGLFunctions>
SetupDialog::SetupDialog(QWidget *p):QDialog(p){
    auto tr=[](const char *s){return Language::key(s);};setWindowTitle(tr("Setup & diagnostics"));resize(820,620);QVBoxLayout *layout=new QVBoxLayout(this);auto text=new QLabel(tr("Check Java, graphics and sound before running lost versions."));text->setWordWrap(true);layout->addWidget(text);auto list=new QTreeWidget;list->setObjectName("javaInstallations");list->setHeaderLabels({"Java",tr("Architecture"),tr("Location")});list->header()->setSectionResizeMode(2,QHeaderView::Stretch);layout->addWidget(list,1);auto status=new QLabel(tr("Checking installed Java versions…"));status->setWordWrap(true);layout->addWidget(status);auto scan=new QPushButton(tr("Scan for Java again"));layout->addWidget(scan);auto watcher=new QFutureWatcher<QList<JavaInstallation>>(this);auto check=[watcher,scan]{if(watcher->isRunning())return;scan->setEnabled(false);watcher->setFuture(QtConcurrent::run([]{return JavaRuntime::discover();}));};connect(scan,&QPushButton::clicked,this,check);connect(watcher,&QFutureWatcher<QList<JavaInstallation>>::finished,this,[=]{scan->setEnabled(true);list->clear();auto runtimes=watcher->result();for(auto j:runtimes)new QTreeWidgetItem(list,{j.version,j.architecture,j.path});status->setText(runtimes.isEmpty()?tr("No working Java installation was found."):tr("Detected Java")+": "+QString::number(runtimes.size()));});check();
    auto graphics=new QLabel;graphics->setWordWrap(true);layout->addWidget(graphics);QOpenGLContext context;QOffscreenSurface surface;surface.create();if(context.create()&&context.makeCurrent(&surface)){auto gl=context.functions();auto vendor=reinterpret_cast<const char*>(gl->glGetString(GL_RENDERER));auto version=reinterpret_cast<const char*>(gl->glGetString(GL_VERSION));graphics->setText("OpenGL · "+QString::fromUtf8(vendor?vendor:"")+" · "+QString::fromUtf8(version?version:""));context.doneCurrent();}else graphics->setText(tr("OpenGL could not be verified in this session."));
    auto audio=new QLabel;audio->setWordWrap(true);layout->addWidget(audio);
#ifdef Q_OS_WIN
    auto win=qEnvironmentVariable("WINDIR","C:/Windows");QStringList found;for(auto path:{win+"/System32/OpenAL32.dll",win+"/SysWOW64/OpenAL32.dll"})if(QFile::exists(path))found<<path;audio->setText("OpenAL · "+(found.isEmpty()?tr("Not detected"):found.join(" · ")));
#else
    audio->setText(tr("Windows packages need Windows Java inside Wine. Native Linux or macOS Java cannot replace it."));
#endif
    auto links=new QGridLayout;layout->addLayout(links);int n=0;for(auto pair:{qMakePair("Get Java 8","https://adoptium.net/temurin/releases/?version=8"),qMakePair("OpenAL for Windows","https://www.openal.org/downloads/"),qMakePair("Jarfix (optional)","https://johann.loefflmann.net/en/software/jarfix/index.html"),qMakePair("NVIDIA drivers","https://www.nvidia.com/Download/index.aspx"),qMakePair("AMD drivers","https://www.amd.com/en/support"),qMakePair("Intel drivers","https://www.intel.com/content/www/us/en/download-center/home.html")}){auto b=new QPushButton(tr(pair.first));links->addWidget(b,n/2,n%2);++n;connect(b,&QPushButton::clicked,this,[pair]{QDesktopServices::openUrl(QUrl(pair.second));});}
    auto note=new QLabel(tr("Install drivers from your hardware vendor. Jarfix only repairs double-click associations; EBALIA launches Java directly. Recheck after installing dependencies."));note->setWordWrap(true);layout->addWidget(note);auto close=new QDialogButtonBox(QDialogButtonBox::Close);layout->addWidget(close);connect(close,&QDialogButtonBox::rejected,this,&QDialog::reject);
}
