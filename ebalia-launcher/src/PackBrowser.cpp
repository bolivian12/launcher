#include "PackBrowser.hpp"
#include "PackService.hpp"
#include "ModRepository.hpp"
#include "Language.hpp"
#include "InstanceIcons.hpp"
#include <QtWidgets>
#include <QtConcurrent>
#include <QtNetwork>
namespace {
QString t(const char *s){return Language::key(QString::fromUtf8(s));}
QHash<QString,QPixmap> &iconCache(){static QHash<QString,QPixmap> cache;return cache;}
}
PackBrowser::PackBrowser(const QString &root,const QString &provider,QWidget *parent):QWidget(parent),m_root(root),m_provider(provider){
    setObjectName(provider+"Page");auto layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    auto bar=new QHBoxLayout;layout->addLayout(bar);m_search=new QLineEdit;m_search->setObjectName("packSearch");m_search->setPlaceholderText(t("Search modpacks…"));m_search->setClearButtonEnabled(true);bar->addWidget(m_search,1);
    m_find=new QPushButton(t("Search"));bar->addWidget(m_find);
    if(provider=="curseforge"){
        // CurseForge only answers launchers that send an API key; paste it once and the page searches right away.
        m_keyRow=new QWidget;auto row=new QHBoxLayout(m_keyRow);row->setContentsMargins(0,0,0,0);
        auto field=new QLineEdit;field->setObjectName("curseForgeKey");field->setEchoMode(QLineEdit::Password);field->setPlaceholderText(t("CurseForge API key"));row->addWidget(field,1);
        auto save=new QPushButton(t("Save"));save->setProperty("play",true);row->addWidget(save);
        auto get=new QPushButton(t("Get a key"));row->addWidget(get);layout->addWidget(m_keyRow);
        connect(get,&QPushButton::clicked,this,[]{QDesktopServices::openUrl(QUrl("https://console.curseforge.com/"));});
        auto store=[this,field]{auto key=field->text().trimmed();if(key.isEmpty())return;QSettings().setValue("integrations/curseforgeKey",key);field->clear();m_keyRow->setVisible(false);m_started=true;lookup(false);};
        connect(save,&QPushButton::clicked,this,store);connect(field,&QLineEdit::returnPressed,this,store);
        m_keyRow->setVisible(ModRepository::curseForgeKey().isEmpty());
    }
    auto split=new QSplitter;layout->addWidget(split,1);
    m_results=new QListWidget;m_results->setObjectName("packResults");m_results->setIconSize(QSize(40,40));m_results->setWordWrap(true);m_results->setTextElideMode(Qt::ElideRight);m_results->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);split->addWidget(m_results);
    m_description=new QTextBrowser;m_description->setOpenLinks(false);m_description->document()->setDefaultStyleSheet("a{color:#5fd38d;font-weight:bold;}");split->addWidget(m_description);split->setStretchFactor(0,3);split->setStretchFactor(1,2);
    // Pack pages are third-party content: only https links are opened, in the system browser.
    connect(m_description,&QTextBrowser::anchorClicked,this,[](const QUrl &url){if(url.scheme()=="https")QDesktopServices::openUrl(url);});
    m_status=new QLabel;m_status->setWordWrap(true);m_status->setObjectName("muted");layout->addWidget(m_status);
    auto bottom=new QHBoxLayout;layout->addLayout(bottom);bottom->addWidget(new QLabel(t("Version")));m_versions=new QComboBox;m_versions->setObjectName("packVersions");bottom->addWidget(m_versions,1);
    m_more=new QPushButton(t("Load more"));m_more->setVisible(provider=="modrinth"||provider=="curseforge");bottom->addWidget(m_more);
    connect(m_find,&QPushButton::clicked,this,[this]{lookup(false);});connect(m_search,&QLineEdit::returnPressed,this,[this]{lookup(false);});connect(m_more,&QPushButton::clicked,this,[this]{lookup(true);});
    connect(m_versions,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{selectedVersion=m_versions->currentData().toJsonObject();if(changed)changed();});
    connect(m_results,&QListWidget::currentItemChanged,this,[this](QListWidgetItem *item){
        auto ticket=++m_versionEpoch;{QSignalBlocker block(m_versions);m_versions->clear();}selectedVersion={};selectedPack=item?item->data(Qt::UserRole).toJsonObject():QJsonObject();if(changed)changed();if(!item)return;
        describe(selectedPack);m_status->setText(t("Loading versions…"));auto pack=selectedPack;auto root=m_root;auto watcher=new QFutureWatcher<QJsonObject>(this);
        connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,watcher,ticket]{
            auto d=watcher->result();watcher->deleteLater();if(ticket!=m_versionEpoch)return;if(d.contains("error")){m_status->setText(Language::message(d["error"].toString()));return;}
            {QSignalBlocker block(m_versions);for(auto v:d["versions"].toArray())m_versions->addItem(v.toObject()["name"].toString(),v.toObject());
             // Preselect the newest stable build rather than an alpha or beta.
             for(int n=0;n<m_versions->count();++n){auto data=m_versions->itemData(n).toJsonObject()["data"].toObject();if(data["version_type"]=="release"||data["releaseType"].toInt()==1){m_versions->setCurrentIndex(n);break;}}}
            selectedVersion=m_versions->currentData().toJsonObject();m_status->setText(m_versions->count()?t("Choose a modpack and its version."):t("This modpack has no versions to install."));if(changed)changed();
        });
        watcher->setFuture(QtConcurrent::run([root,pack]{try{return QJsonObject{{"versions",PackService(root).versions(pack)}};}catch(const std::exception &e){return QJsonObject{{"error",QString::fromUtf8(e.what())}};}}));
    });
}
void PackBrowser::activate(){
    if(m_started)return;m_started=true;
    if(qEnvironmentVariableIsSet("EBALIA_NO_NETWORK")){m_status->setText(t("Search to see modpacks."));return;}
    if(m_keyRow&&ModRepository::curseForgeKey().isEmpty())return; // nothing to ask CurseForge until a key is pasted
    lookup(false);
}
void PackBrowser::lookup(bool append){
    if(m_keyRow&&ModRepository::curseForgeKey().isEmpty()){m_keyRow->setVisible(true);if(auto field=m_keyRow->findChild<QLineEdit*>())field->setFocus();return;}
    auto ticket=++m_searchEpoch;m_find->setEnabled(false);m_more->setEnabled(false);
    if(!append){m_page=0;m_query=m_search->text().trimmed();++m_versionEpoch;{QSignalBlocker a(m_results),b(m_versions);m_results->clear();m_versions->clear();}selectedPack={};selectedVersion={};m_description->clear();if(changed)changed();}
    else ++m_page;
    m_status->setText(t("Loading modpacks…"));auto root=m_root,provider=m_provider,query=m_query;int page=m_page;auto watcher=new QFutureWatcher<QJsonObject>(this);
    connect(watcher,&QFutureWatcher<QJsonObject>::finished,this,[this,watcher,ticket]{
        auto d=watcher->result();watcher->deleteLater();if(ticket!=m_searchEpoch)return;m_find->setEnabled(true);m_more->setEnabled(true);
        if(d.contains("error")){auto error=d["error"].toString();if(m_keyRow&&(error.contains("403")||error.contains("401")||error.contains("Forbidden",Qt::CaseInsensitive))){QSettings().remove("integrations/curseforgeKey");m_keyRow->setVisible(ModRepository::curseForgeKey().isEmpty());error=t("CurseForge rejected this API key.");}m_status->setText(Language::message(error));return;}
        auto packs=d["packs"].toArray();
        for(auto v:packs){
            auto p=v.toObject();QTextDocument plain;plain.setHtml(p["description"].toString());auto summary=plain.toPlainText().simplified();if(summary.size()>140)summary=summary.left(137)+"…";
            auto item=new QListWidgetItem(InstanceIcons::provider(m_provider),p["name"].toString()+(summary.isEmpty()?QString():"\n"+summary),m_results);item->setData(Qt::UserRole,p);item->setToolTip(plain.toPlainText().left(500));
            auto icon=p["icon"].toString();if(icon.startsWith("https://"))loadIcon(icon,p["id"].toString());
        }
        m_status->setText(m_results->count()?QString::number(m_results->count())+" · "+t("Choose a modpack and its version."):t("No modpacks found."));
        if(packs.isEmpty())m_more->setEnabled(false);
    });
    watcher->setFuture(QtConcurrent::run([root,provider,query,page]{try{return QJsonObject{{"packs",PackService(root).search(provider,query,page)}};}catch(const std::exception &e){return QJsonObject{{"error",QString::fromUtf8(e.what())}};}}));
}
void PackBrowser::loadIcon(const QString &url,const QString &id){
    auto apply=[this,id](const QPixmap &pixmap){for(int i=0;i<m_results->count();++i)if(m_results->item(i)->data(Qt::UserRole).toJsonObject()["id"].toString()==id)m_results->item(i)->setIcon(pixmap);};
    if(iconCache().contains(url)){apply(iconCache()[url]);return;}
    if(!m_icons)m_icons=new QNetworkAccessManager(this);
    QNetworkRequest request{QUrl(url)};request.setHeader(QNetworkRequest::UserAgentHeader,"EBALIA-Launcher/4.0");request.setTransferTimeout(20000);
    auto reply=m_icons->get(request);
    connect(reply,&QNetworkReply::finished,this,[reply,url,apply]{reply->deleteLater();if(reply->error()!=QNetworkReply::NoError)return;auto data=reply->read(4*1024*1024);QPixmap pixmap;if(!pixmap.loadFromData(data))return;
        pixmap=pixmap.scaled(40,40,Qt::KeepAspectRatio,Qt::SmoothTransformation);iconCache().insert(url,pixmap);apply(pixmap);});
}
void PackBrowser::describe(const QJsonObject &p){
    QStringList meta;if(!p["author"].toString().isEmpty())meta<<t("By")+" "+p["author"].toString();
    if(p["downloads"].toDouble()>0)meta<<QLocale().toString(qint64(p["downloads"].toDouble()))+" "+t("downloads");
    auto url=p["url"].toString();auto body=p["body"].toString().isEmpty()?p["description"].toString():p["body"].toString();
    if(p["bodyFormat"]=="markdown"){
        // Remote images are not loaded; drop them instead of showing broken placeholders.
        body.remove(QRegularExpression("!\\[[^\\]]*\\]\\([^)]*\\)"));body.remove(QRegularExpression("<img[^>]*>",QRegularExpression::CaseInsensitiveOption));
        QString text="## "+p["name"].toString()+"\n\n";if(!meta.isEmpty())text+="*"+meta.join(" · ")+"*\n\n";if(url.startsWith("https://"))text+="["+t("Open the modpack page")+"]("+url+")\n\n";
        m_description->setMarkdown(text+body);return;
    }
    QString html="<h2>"+p["name"].toString().toHtmlEscaped()+"</h2>";if(!meta.isEmpty())html+="<p><i>"+meta.join(" · ").toHtmlEscaped()+"</i></p>";
    if(url.startsWith("https://"))html+="<p><a href=\""+url.toHtmlEscaped()+"\">"+t("Open the modpack page")+"</a></p>";
    body.remove(QRegularExpression("<img[^>]*>",QRegularExpression::CaseInsensitiveOption));
    html+=p["bodyFormat"]=="html"?"<p>"+body+"</p>":"<p>"+body.toHtmlEscaped()+"</p>";m_description->setHtml(html);
}
