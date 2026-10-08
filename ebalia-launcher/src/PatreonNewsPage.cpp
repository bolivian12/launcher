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
    void fit(){if(adjusting)return;const int full=qMax(48,int(document()->size().height())+12);setFixedHeight(collapsed&&full>limit?limit:full);emit_overflow(full>limit);}
public:
    // Long posts show their beginning; "Show more" opens them.
    bool collapsed=true;static constexpr int limit=230;std::function<void(bool)> overflowChanged;
    void setCollapsed(bool value){collapsed=value;fit();}
private:
    void emit_overflow(bool value){if(overflowChanged)overflowChanged(value);}
};
// Patreon's HTML carries its editor's fonts, sizes and small caps: keep the text, links, emphasis and pictures only.
QString cleanPost(QString html){
    static const QRegularExpression attributes(R"re(\s(style|class|align|face|size|color|data-[\w-]+)\s*=\s*("[^"]*"|'[^']*'))re",QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression fonts(R"re(</?(font|span)\b[^>]*>)re",QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression breaks(R"re((<br\s*/?>\s*){3,})re",QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression empty(R"re(<p>\s*(<br\s*/?>)?\s*</p>)re",QRegularExpression::CaseInsensitiveOption);
    html.remove(attributes);html.remove(fonts);html.replace(breaks,"<br><br>");html.remove(empty);
    return html;
}
QString readableDate(const QString &iso){
    const auto date=QDate::fromString(iso.left(10),Qt::ISODate);if(!date.isValid())return iso.left(10);
    const QLocale locale(Language::current);auto text=locale.toString(date,QLocale::LongFormat);const auto day=locale.dayName(date.dayOfWeek());
    if(text.startsWith(day,Qt::CaseInsensitive)){text=text.mid(day.size());while(!text.isEmpty()&&!text.front().isLetterOrNumber())text.remove(0,1);} // no weekday
    return text;
}
}

PatreonNewsPage::PatreonNewsPage(PatreonAuth *auth,QWidget *parent):QWidget(parent){
    auto t=[](const char *text){return Language::key(QString::fromUtf8(text));};
    auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,12,0,0);layout->setSpacing(12);
    // Membership card: who is linked and what to do next.
    auto header=new QFrame;header->setObjectName("patreonHeader");header->setStyleSheet("QFrame#patreonHeader{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #33201f,stop:1 #1d1e22);border:1px solid #4a2f2c;border-radius:16px;}QFrame#patreonHeader QLabel{background:transparent;}");layout->addWidget(header);
    auto hl=new QVBoxLayout(header);hl->setContentsMargins(20,16,20,16);hl->setSpacing(10);
    auto top=new Ui::ResponsiveRow(720);top->box()->setSpacing(14);hl->addWidget(top);
    auto badge=new QLabel;badge->setFixedSize(46,46);badge->setAlignment(Qt::AlignCenter);badge->setPixmap(Ui::pixmap("heart",24,Qt::white));badge->setStyleSheet("background:#f0645a;border-radius:23px;");top->box()->addWidget(badge,0,Qt::AlignVCenter);
    auto who=new QVBoxLayout;who->setSpacing(2);top->box()->addLayout(who,1);
    auto name=new QLabel;name->setObjectName("patreonMember");name->setTextFormat(Qt::PlainText);name->setStyleSheet("font-size:19px;font-weight:800;color:white;");who->addWidget(name);
    auto status=new QLabel;status->setObjectName("patreonNewsStatus");status->setWordWrap(true);status->setStyleSheet("color:#c9b9b6;font-size:13px;");who->addWidget(status);
    auto actions=new QHBoxLayout;actions->setSpacing(8);top->box()->addLayout(actions);
    auto add=[actions](const QString &text,const char *name,const QString &icon,std::function<void()> action){auto b=new QPushButton(text);b->setObjectName(name);b->setCursor(Qt::PointingHandCursor);if(!icon.isEmpty()){b->setIcon(Ui::icon(icon,QColor(235,235,240)));b->setIconSize({16,16});}actions->addWidget(b);QObject::connect(b,&QPushButton::clicked,b,action);return b;};
    auto link=add(" "+t("Link Patreon"),"newsLinkPatreon","heart",[auth]{auth->startLogin();});link->setProperty("patreon",true);link->setIcon(Ui::icon("heart",Qt::white));
    auto unlink=add(t("Unlink"),"newsUnlinkPatreon",{},[auth]{auth->logout();});
    auto refresh=add({},"newsRefreshPatreon","refresh-cw",[auth]{auth->refreshNews();});refresh->setToolTip(t("Refresh now"));refresh->setAccessibleName(refresh->toolTip());refresh->setFixedWidth(42);
    add(" "+t("Join EBALIA on Patreon")+"  ↗","newsJoinPatreon",{},[]{QDesktopServices::openUrl(QUrl("https://www.patreon.com/EBALIA"));});
    auto tiersLabel=new QLabel;tiersLabel->setObjectName("patreonAvailableTiers");tiersLabel->setWordWrap(true);tiersLabel->setTextFormat(Qt::PlainText);tiersLabel->setStyleSheet("color:#e8a59e;font-size:12px;font-weight:600;");hl->addWidget(tiersLabel);
    auto sections=new QTabWidget;sections->setObjectName("patreonNewsTabs");sections->setDocumentMode(true);layout->addWidget(sections,1);
    auto page=[sections](const QString &title,const char *name){auto widget=new QWidget;auto box=new QVBoxLayout(widget);box->setContentsMargins(0,12,0,0);auto note=new QLabel;note->setWordWrap(true);box->addWidget(note);auto list=new QScrollArea;list->setObjectName(name);list->setWidgetResizable(true);list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);list->setFrameShape(QFrame::NoFrame);box->addWidget(list,1);sections->addTab(widget,title);return qMakePair(note,list);};
    auto publicPage=page(t("Public posts"),"patreonPublicPosts"),paidPage=page(t("Paid members"),"patreonPaidPosts");
    publicPage.first->setText(t("Public posts are available to everyone."));paidPage.first->setObjectName("patreonMembership");
    auto render=[=,this]{
        name->setText(auth->verified()?auth->patronName()+"  ·  "+auth->tierTitle():"EBALIA · Patreon");
        status->setText(auth->isConfigured()?auth->status():t("Patreon news will appear when the EBALIA service is connected."));link->setEnabled(auth->isConfigured());
        link->setVisible(!auth->hasTokens());unlink->setVisible(auth->hasTokens());unlink->setEnabled(auth->hasTokens());
        paidPage.first->setText(auth->paidMember()?auth->patronName()+" · "+auth->tierTitle():t("Link an active paid membership to read your exclusive posts."));
        QStringList tierNames;for(auto value:auth->availableTiers()){auto tier=value.toObject();if(tier["amount_cents"].toInt()>0)tierNames.append(tier["title"].toString());}tiersLabel->setText(tierNames.isEmpty()?QString():t("Memberships")+":   "+tierNames.join("   ·   "));tiersLabel->setVisible(!tierNames.isEmpty());
        const auto data=auth->posts();const QByteArray signature=QJsonDocument(data).toJson(QJsonDocument::Compact)+QByteArray::number(auth->paidMember());
        if(publicPage.second->property("feed").toByteArray()==signature)return;publicPage.second->setProperty("feed",signature);
        auto publicList=publicPage.second,paidList=paidPage.second;
        const int a=publicList->verticalScrollBar()->value(),b=paidList->verticalScrollBar()->value();
        auto reset=[](QScrollArea *area){auto body=new QWidget;auto box=new QVBoxLayout(body);box->setContentsMargins(0,0,4,0);box->setSpacing(16);area->setWidget(body);return box;};
        auto publicBox=reset(publicList),paidBox=reset(paidList);
        for(auto v:data){auto post=v.toObject();const bool isPublic=post["is_public"].toBool();if(!isPublic&&!auth->paidMember())continue;
            auto card=new QFrame;card->setObjectName("patreonPostCard");card->setStyleSheet("QFrame#patreonPostCard{background:#1d1e22;border:1px solid #2f3036;border-radius:16px;}QFrame#patreonPostCard QLabel{background:transparent;}");
            auto box=new QVBoxLayout(card);box->setContentsMargins(22,18,22,16);box->setSpacing(10);
            auto metaRow=new QHBoxLayout;metaRow->setSpacing(10);box->addLayout(metaRow);
            auto chip=new QLabel(isPublic?t("Public posts"):t("Paid members"));chip->setStyleSheet(QString("padding:3px 10px;border-radius:9px;font-size:11px;font-weight:800;color:%1;background:%2;").arg(isPublic?"#bfe8a9":"#ffd0ca",isPublic?"#24371f":"#4a2522"));metaRow->addWidget(chip);
            auto when=new QLabel(readableDate(post["date"].toString()));when->setStyleSheet("color:#8f939c;font-size:12px;");metaRow->addWidget(when);metaRow->addStretch();
            auto title=new QLabel(post["title"].toString());title->setTextFormat(Qt::PlainText);title->setWordWrap(true);title->setStyleSheet("font-size:20px;font-weight:800;color:white;");box->addWidget(title);
            auto content=new PostBody;content->setObjectName("patreonPostBody");content->setHtml(post["content"].toString().isEmpty()?post["excerpt"].toString().toHtmlEscaped():cleanPost(post["content"].toString()));box->addWidget(content);content->loadImages();
            auto footer=new QHBoxLayout;footer->setSpacing(10);box->addLayout(footer);
            auto more=new QPushButton(t("Show more"));more->setObjectName("patreonShowMore");more->setProperty("link",true);more->setCursor(Qt::PointingHandCursor);more->hide();footer->addWidget(more);
            content->overflowChanged=[more,content](bool overflow){more->setVisible(overflow);more->setText(content->collapsed?Language::key("Show more"):Language::key("Show less"));};
            connect(more,&QPushButton::clicked,card,[content]{content->setCollapsed(!content->collapsed);});
            footer->addStretch();
            auto read=new QPushButton(t("Read post")+"  ↗");read->setObjectName("patreonReadPost");read->setCursor(Qt::PointingHandCursor);footer->addWidget(read);const QUrl url(post["url"].toString());connect(read,&QPushButton::clicked,card,[url]{QDesktopServices::openUrl(url);});
            (isPublic?publicBox:paidBox)->addWidget(card);
        }
        auto finish=[](QVBoxLayout *box,const QString &message){if(!box->count()){auto note=new QLabel(message);note->setWordWrap(true);note->setContentsMargins(20,24,20,24);box->addWidget(note);}box->addStretch();};
        finish(publicBox,t("No public posts available yet."));finish(paidBox,t(auth->paidMember()?"No exclusive posts available for your membership yet.":"Link an active paid membership to read your exclusive posts."));
        QTimer::singleShot(0,this,[=]{publicList->verticalScrollBar()->setValue(a);paidList->verticalScrollBar()->setValue(b);});
    };connect(auth,&PatreonAuth::changed,this,render);render();
}
