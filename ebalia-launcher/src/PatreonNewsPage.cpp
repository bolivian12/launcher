#include "PatreonNewsPage.hpp"
#include "PatreonAuth.hpp"
#include "Language.hpp"
#include "Ui.hpp"
#include <QtWidgets>
#include <QtNetwork>

namespace {
class PostBody : public QTextBrowser {
public:
    explicit PostBody(QWidget *parent=nullptr):QTextBrowser(parent){
        setFrameShape(QFrame::NoFrame);setOpenLinks(false);setOpenExternalLinks(false);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);setStyleSheet("background:transparent;color:#d9dce3;border:0;font-size:14px;");
        setLineWrapMode(QTextEdit::WidgetWidth);setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        document()->setDefaultStyleSheet("a{color:#9bd6ff;text-decoration:underline;} p{margin-top:6px;margin-bottom:12px;} h1,h2,h3{color:white;}");
        connect(this,&QTextBrowser::anchorClicked,this,[](const QUrl &url){if(url.scheme()=="https"||url.scheme()=="http")QDesktopServices::openUrl(url);});
        connect(document()->documentLayout(),&QAbstractTextDocumentLayout::documentSizeChanged,this,[this]{fit();});
    }
    QVariant loadResource(int,const QUrl &) override {return {};}
    void loadImages(){
        QSet<QString> seen;
        for(auto block=document()->begin();block.isValid();block=block.next())for(auto it=block.begin();!it.atEnd();++it){auto f=it.fragment();if(!f.charFormat().isImageFormat())continue;auto name=f.charFormat().toImageFormat().name();QUrl url(name);
            if(seen.contains(name)||seen.size()>=6||url.scheme()!="https"||!(url.host()=="patreonusercontent.com"||url.host().endsWith(".patreonusercontent.com")||url.host()=="attachments.patreon.com"))continue;
            seen.insert(name);if(qEnvironmentVariableIsSet("EBALIA_NO_NETWORK"))continue;auto manager=findChild<QNetworkAccessManager*>();if(!manager){manager=new QNetworkAccessManager(this);manager->setTransferTimeout(15000);}
            QNetworkRequest request(url);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);auto reply=manager->get(request);
            connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 bytes,qint64){if(bytes>6*1024*1024)reply->abort();});
            connect(reply,&QNetworkReply::finished,this,[this,reply,url]{reply->deleteLater();if(reply->error()!=QNetworkReply::NoError)return;auto bytes=reply->readAll();if(bytes.size()>6*1024*1024)return;QBuffer buffer(&bytes);buffer.open(QIODevice::ReadOnly);QImageReader reader(&buffer);auto size=reader.size();if(!size.isValid()||size.width()>12000||size.height()>12000)return;reader.setScaledSize(size.scaled(1000,600,Qt::KeepAspectRatio));auto image=reader.read();if(image.isNull())return;document()->addResource(QTextDocument::ImageResource,url,image);resizeImages();});
        }
        resizeImages();
    }
protected:
    void resizeEvent(QResizeEvent *event) override {QTextBrowser::resizeEvent(event);resizeImages();fit();}
private:
    bool adjusting=false;
    void resizeImages(){if(adjusting)return;adjusting=true;
        for(auto block=document()->begin();block.isValid();block=block.next())for(auto it=block.begin();!it.atEnd();++it){auto f=it.fragment();if(!f.charFormat().isImageFormat())continue;auto fmt=f.charFormat().toImageFormat();auto image=document()->resource(QTextDocument::ImageResource,QUrl(fmt.name())).value<QImage>();if(image.isNull()){fmt.setWidth(1);fmt.setHeight(1);}else{auto size=image.size().scaled(qMax(40,viewport()->width()-12),440,Qt::KeepAspectRatio);fmt.setWidth(size.width());fmt.setHeight(size.height());}QTextCursor cursor(document());cursor.setPosition(f.position());cursor.setPosition(f.position()+f.length(),QTextCursor::KeepAnchor);cursor.setCharFormat(fmt);}
        adjusting=false;fit();
    }
    void fit(){if(adjusting)return;setFixedHeight(qMax(48,int(document()->size().height())+12));}
};
}

PatreonNewsPage::PatreonNewsPage(PatreonAuth *auth,QWidget *parent):QWidget(parent){
    auto t=[](const char *text){return Language::key(QString::fromUtf8(text));};
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,12,0,0);layout->setSpacing(12);
    auto actions=new Ui::ResponsiveRow(760);layout->addWidget(actions);
    auto add=[actions](const QString &text,const char *name,std::function<void()> action){auto b=new QPushButton(text);b->setObjectName(name);actions->box()->addWidget(b);QObject::connect(b,&QPushButton::clicked,b,action);return b;};
    auto link=add(t("Link Patreon"),"newsLinkPatreon",[auth]{auth->startLogin();});auto unlink=add(t("Unlink"),"newsUnlinkPatreon",[auth]{auth->logout();});
    add(t("Refresh now"),"newsRefreshPatreon",[auth]{auth->refreshNews();});add(t("Join EBALIA on Patreon"),"newsJoinPatreon",[]{QDesktopServices::openUrl(QUrl("https://www.patreon.com/EBALIA"));});
    auto status=new QLabel;status->setObjectName("patreonNewsStatus");status->setWordWrap(true);layout->addWidget(status);
    auto tiersLabel=new QLabel;tiersLabel->setObjectName("patreonAvailableTiers");tiersLabel->setWordWrap(true);tiersLabel->setTextFormat(Qt::PlainText);tiersLabel->setStyleSheet("color:#b4bac6;font-size:12px;");layout->addWidget(tiersLabel);
    auto sections=new QTabWidget;sections->setObjectName("patreonNewsTabs");sections->setDocumentMode(true);layout->addWidget(sections,1);
    auto page=[sections](const QString &title,const char *name){auto widget=new QWidget;auto box=new QVBoxLayout(widget);box->setContentsMargins(0,12,0,0);auto note=new QLabel;note->setWordWrap(true);box->addWidget(note);auto list=new QScrollArea;list->setObjectName(name);list->setWidgetResizable(true);list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);list->setFrameShape(QFrame::NoFrame);box->addWidget(list,1);sections->addTab(widget,title);return qMakePair(note,list);};
    auto publicPage=page(t("Public posts"),"patreonPublicPosts"),paidPage=page(t("Paid members"),"patreonPaidPosts");
    publicPage.first->setText(t("Public posts are available to everyone."));paidPage.first->setObjectName("patreonMembership");
    auto render=[=,this]{
        status->setText(auth->isConfigured()?auth->status():t("Patreon news will appear when the EBALIA service is connected."));link->setEnabled(auth->isConfigured());unlink->setEnabled(auth->hasTokens());
        paidPage.first->setText(auth->paidMember()?auth->patronName()+" · "+auth->tierTitle():t("Link an active paid membership to read your exclusive posts."));
        QStringList tierNames;for(auto value:auth->availableTiers()){auto tier=value.toObject();if(tier["amount_cents"].toInt()>0)tierNames.append(tier["title"].toString());}tiersLabel->setText(tierNames.join("   ·   "));tiersLabel->setVisible(!tierNames.isEmpty());
        const auto data=auth->posts();const QByteArray signature=QJsonDocument(data).toJson(QJsonDocument::Compact)+QByteArray::number(auth->paidMember());
        if(publicPage.second->property("feed").toByteArray()==signature)return;publicPage.second->setProperty("feed",signature);
        auto publicList=publicPage.second,paidList=paidPage.second;
        const int a=publicList->verticalScrollBar()->value(),b=paidList->verticalScrollBar()->value();
        auto reset=[](QScrollArea *area){auto body=new QWidget;auto box=new QVBoxLayout(body);box->setContentsMargins(0,0,4,0);box->setSpacing(16);area->setWidget(body);return box;};
        auto publicBox=reset(publicList),paidBox=reset(paidList);
        for(auto v:data){auto post=v.toObject();const bool isPublic=post["is_public"].toBool();if(!isPublic&&!auth->paidMember())continue;
            auto card=new QFrame;card->setObjectName("patreonPostCard");card->setStyleSheet("QFrame#patreonPostCard{background:#1d1e22;border:1px solid #34353c;border-radius:14px;}");
            auto box=new QVBoxLayout(card);box->setContentsMargins(20,18,20,18);box->setSpacing(12);
            auto meta=new QLabel((isPublic?t("Public posts"):t("Paid members"))+"  ·  "+post["date"].toString().left(10));meta->setStyleSheet("color:#9fce92;font-size:12px;");box->addWidget(meta);
            auto title=new QLabel(post["title"].toString());title->setTextFormat(Qt::PlainText);title->setWordWrap(true);title->setStyleSheet("font-size:21px;font-weight:700;color:white;");box->addWidget(title);
            auto content=new PostBody;content->setObjectName("patreonPostBody");content->setHtml(post["content"].toString().isEmpty()?post["excerpt"].toString().toHtmlEscaped():post["content"].toString());box->addWidget(content);content->loadImages();
            auto read=new QPushButton(t("Read post")+"  ↗");read->setObjectName("patreonReadPost");read->setCursor(Qt::PointingHandCursor);box->addWidget(read,0,Qt::AlignLeft);const QUrl url(post["url"].toString());connect(read,&QPushButton::clicked,card,[url]{QDesktopServices::openUrl(url);});
            (isPublic?publicBox:paidBox)->addWidget(card);
        }
        auto finish=[](QVBoxLayout *box,const QString &message){if(!box->count()){auto note=new QLabel(message);note->setWordWrap(true);note->setContentsMargins(20,24,20,24);box->addWidget(note);}box->addStretch();};
        finish(publicBox,t("No public posts available yet."));finish(paidBox,t(auth->paidMember()?"No exclusive posts available for your membership yet.":"Link an active paid membership to read your exclusive posts."));
        QTimer::singleShot(0,this,[=]{publicList->verticalScrollBar()->setValue(a);paidList->verticalScrollBar()->setValue(b);});
    };connect(auth,&PatreonAuth::changed,this,render);render();
}
