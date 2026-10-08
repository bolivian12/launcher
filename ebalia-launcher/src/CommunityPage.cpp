#include "CommunityPage.hpp"
#include "PatreonNewsPage.hpp"
#include "PatreonAuth.hpp"
#include "Language.hpp"
#include "Ui.hpp"
#include <QtWidgets>
#include <QtNetwork>
#include <memory>
namespace {
QString trc(const char *s){return Language::key(QString::fromUtf8(s));}
QLabel *label(const QString &text,QVBoxLayout *l,const char *name="muted"){auto w=new QLabel(text);w->setTextFormat(Qt::PlainText);w->setWordWrap(true);w->setObjectName(name);l->addWidget(w);return w;}
QPushButton *button(const QString &text,QBoxLayout *l,QObject *context,std::function<void()> action){auto b=new QPushButton(QString(text).replace("&","&&"));l->addWidget(b);QObject::connect(b,&QPushButton::clicked,context,action);return b;}
void link(const QString &text,const QString &url,QBoxLayout *l,QObject *context){auto b=button(text+"  ↗",l,context,[url]{QDesktopServices::openUrl(QUrl(url));});b->setToolTip(url);}
// Calls back when the watched widget changes size (the fan art grid reflows to the window width).
class OnResize:public QObject {
public:
    OnResize(QWidget *watched,std::function<void()> callback):QObject(watched),m_callback(std::move(callback)){watched->installEventFilter(this);}
    bool eventFilter(QObject *,QEvent *event) override {if(event->type()==QEvent::Resize)m_callback();return false;}
private:
    std::function<void()> m_callback;
};
QJsonObject resource(const char *path){QFile f(QString::fromUtf8(path));if(!f.open(QIODevice::ReadOnly))return {};return QJsonDocument::fromJson(f.readAll()).object();}
}
CommunityPage::CommunityPage(int section,const QString &root,PatreonAuth *auth,QWidget *parent):QWidget(parent){
    setObjectName("communityPage");auto outer=new QVBoxLayout(this);outer->setContentsMargins(0,16,0,0);outer->setSpacing(14); // the tab already names the section
    auto scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);outer->addWidget(scroll,1);auto content=new QWidget;scroll->setWidget(content);auto l=new QVBoxLayout(content);l->setContentsMargins(0,0,12,0);l->setSpacing(14);auto catalog=resource(":/community.json");
    if(section==0){
        label(trc("Art from the EBALIA community. Select a picture to enlarge it."),l);auto grid=new QGridLayout;l->addLayout(grid);int i=0;QList<QToolButton*> cards;
        for(auto value:catalog["fanarts"].toArray()){auto art=value.toObject();auto file=":"+QString("/")+art["file"].toString();auto artist=art["artist"].toString();QImageReader reader(file);reader.setAutoTransform(true);reader.setScaledSize(reader.size().scaled(240,150,Qt::KeepAspectRatio));auto thumb=QPixmap::fromImage(reader.read());auto card=new QToolButton;card->setObjectName("fanartCard");card->setIcon(QIcon(thumb));card->setIconSize(QSize(240,150));card->setText(artist);card->setAccessibleName(trc("Fan arts")+" · "+artist);card->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);card->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);card->setMinimumHeight(188);cards<<card;grid->addWidget(card,i/3,i%3);++i;
            connect(card,&QToolButton::clicked,this,[this,file,artist]{QDialog d(this);d.setObjectName("fanartViewer");d.setWindowTitle(artist);Ui::fitToScreen(&d,{850,640});QVBoxLayout layout(&d);auto area=new QScrollArea;area->setWidgetResizable(true);auto image=new QLabel;image->setAlignment(Qt::AlignCenter);QPixmap p(file);image->setPixmap(p.scaled(800,540,Qt::KeepAspectRatio,Qt::SmoothTransformation));area->setWidget(image);layout.addWidget(area);auto credit=new QLabel(artist);layout.addWidget(credit);QDialogButtonBox close(QDialogButtonBox::Close);layout.addWidget(&close);connect(&close,&QDialogButtonBox::rejected,&d,&QDialog::reject);Ui::openWindow(d);});
        }
        auto columns=std::make_shared<int>(3);
        new OnResize(scroll->viewport(),[grid,cards,columns,scroll]{
            const int fit=qBound(1,(scroll->viewport()->width()-12+12)/(cards.isEmpty()?260:cards.first()->sizeHint().width()+12),4);if(fit==*columns)return;*columns=fit;
            for(auto card:cards)grid->removeWidget(card);for(int n=0;n<cards.size();++n)grid->addWidget(cards[n],n/fit,n%fit);
        });
        label(trc("Credits"),l,"sectionTitle");label("autumnus_ · Has0ne · Niravixis · ashorsmth · awesomegoat152 · realtbnrsaiyan · malidactae · anonymousbut2nd",l);label("Locod (Minecraft 1.7.10) · Ext 16.05 Team (Alpha 1.0.16.05_20)",l);link(trc("Share your art on Discord"),"https://discord.gg/ZrgMwfdwZB",l,this);
    }else if(section==1){
        label(trc("Historical servers and downloads. Availability depends on their owners."),l);auto nam=new QNetworkAccessManager(this);nam->setTransferTimeout(30000);
        for(auto value:catalog["servers"].toArray()){auto server=value.toObject();label(server["name"].toString(),l,"sectionTitle");auto status=label("",l);auto row=new QHBoxLayout;l->addLayout(row);auto dest=root+"/servers/"+server["file"].toString();auto download=button(trc("Download server"),row,this,[]{});connect(download,&QPushButton::clicked,this,[this,nam,server,dest,status,download]{if(QFile::exists(dest)&&QMessageBox::question(this,"EBALIA",trc("Replace the downloaded server file?"))!=QMessageBox::Yes)return;download->setEnabled(false);status->setText(trc("Downloading…"));QNetworkRequest req(QUrl(server["url"].toString()));req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);req.setHeader(QNetworkRequest::UserAgentHeader,"EBALIA-Launcher/4.0");auto reply=nam->get(req);connect(reply,&QNetworkReply::finished,this,[reply,dest,status,download]{reply->deleteLater();download->setEnabled(true);auto bytes=reply->readAll();if(reply->error()!=QNetworkReply::NoError||!bytes.startsWith("PK")){status->setText(trc("Download failed. Open the source for details."));return;}QDir().mkpath(QFileInfo(dest).absolutePath());QSaveFile f(dest);if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit()){status->setText(trc("Could not save file."));return;}status->setText(trc("Saved")+" · "+dest);});});link(trc("Open source"),server["url"].toString(),row,this);}
        button(trc("Open servers folder"),l,this,[root]{QDir().mkpath(root+"/servers");QDesktopServices::openUrl(QUrl::fromLocalFile(root+"/servers"));});label(trc("Community servers"),l,"sectionTitle");
        for(auto value:catalog["addresses"].toArray()){auto ip=value.toString();auto row=new QHBoxLayout;l->addLayout(row);row->addWidget(new QLabel(ip),1);auto copy=button(trc("Copy address"),row,this,[ip]{QApplication::clipboard()->setText(ip);});copy->setProperty("address",ip);}
        label(trc("How to host"),l,"sectionTitle");label(trc("Download the matching server into an empty folder. Run java -jar <server>.jar nogui. Read the EULA if that version provides one, configure server.properties and follow the server documentation before sharing its address."),l);
    }else if(section==2){
        label(trc("Courses and resources from the original EBALIA launcher. Course languages are shown below."),l);
        for(auto value:catalog["courses"].toArray()){auto c=value.toObject();link(c["name"].toString()+" · "+c["language"].toString(),c["url"].toString(),l,this);}link("Alphaver Wiki","https://alphaver.miraheze.org/",l,this);
    }else if(section==3){
        label(trc("Get help, follow development and support EBALIA."),l);link("Discord · "+trc("Support & community"),"https://discord.gg/ZrgMwfdwZB",l,this);link("YouTube · EBALIA","https://www.youtube.com/@EBALIA",l,this);link(trc("Patreon membership"),"https://www.patreon.com/c/ebalia/membership",l,this);link(trc("Patreon shop"),"https://www.patreon.com/c/ebalia/shop",l,this);link("Alphaver Wiki","https://alphaver.miraheze.org/",l,this);label(trc("For launcher problems, include the game version, loader and instance log. Never share account tokens."),l);
    }else{
        label(trc("Public posts appear without signing in. Link Patreon to add posts included in your active membership. Refreshes every minute."),l);
        auto page=new PatreonNewsPage(auth,this);page->setMinimumHeight(560);l->addWidget(page,1);
    }
    l->addStretch();
}
