#include "PatreonAuth.hpp"
#include "Language.hpp"
#include "PatreonConfig.hpp"
#include <QtNetwork>
#include <QSettings>
#include <QDesktopServices>
#include <QTimer>
#include <QTextDocument>
namespace {
QNetworkRequest request(const QString &url){QNetworkRequest r{QUrl(url)};r.setHeader(QNetworkRequest::UserAgentHeader,"EBALIA-Launcher/4.0");r.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);return r;}
QString plain(const QString &html){QTextDocument d;d.setHtml(html);return d.toPlainText();}
}
PatreonAuth::PatreonAuth(QObject *p):QObject(p),m_timer(new QTimer(this)){
    m_nam.setTransferTimeout(30000);m_timer->setInterval(60000);connect(m_timer,&QTimer::timeout,this,[this]{if(!qEnvironmentVariableIsSet("EBALIA_NO_NETWORK"))refreshNews();});m_timer->start();
}
QString PatreonAuth::serviceUrl(){auto url=qEnvironmentVariable("EBALIA_PATREON_SERVICE_URL");if(url.isEmpty())url=QString::fromUtf8(EBALIA_BUILTIN_PATREON_SERVICE); /* not editable in the launcher */while(url.endsWith('/'))url.chop(1);return url;}
bool PatreonAuth::isConfigured()const{QUrl u(serviceUrl());return u.isValid()&&u.scheme()=="https"&&!u.host().isEmpty()&&u.userInfo().isEmpty()&&u.query().isEmpty()&&u.fragment().isEmpty();}
QString PatreonAuth::sessionKey()const{return "patreon/sessions/"+QString::fromLatin1(QCryptographicHash::hash(serviceUrl().toUtf8(),QCryptographicHash::Sha256).toHex());}
bool PatreonAuth::hasTokens()const{return !QSettings().value(sessionKey()).toString().isEmpty();}
void PatreonAuth::clearView(){m_posts={};m_verified=false;m_paid=false;m_name.clear();m_title.clear();m_tierIds.clear();}
void PatreonAuth::startLogin(){
    if(m_busy)return;if(!isConfigured()){m_status=Language::key("Configure the EBALIA Patreon service to synchronize posts.");emit changed();return;}
    ++m_generation;auto generation=m_generation;auto url=serviceUrl();auto key=sessionKey();m_busy=true;auto reply=m_nam.post(request(url+"/v1/login"),QByteArray{});
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,url,key]{reply->deleteLater();if(generation!=m_generation)return;m_busy=false;if(url!=serviceUrl())return;auto d=QJsonDocument::fromJson(reply->readAll()).object();QUrl auth(d["auth_url"].toString());auto token=d["session"].toString();if(reply->error()!=QNetworkReply::NoError||auth.scheme()!="https"||auth.host()!="www.patreon.com"||auth.path()!="/oauth2/authorize"||token.isEmpty()){m_status=Language::key("Could not connect to Patreon. Try again.");emit changed();return;}QSettings().setValue(key,token);clearView();m_authUrl=auth.toString();m_pendingUntil=QDateTime::currentSecsSinceEpoch()+300;m_timer->setInterval(3000);m_status=Language::key("Complete sign-in in your browser.");QDesktopServices::openUrl(auth);emit changed();emit loginStarted();});
}
void PatreonAuth::refreshNews(){
    if(m_busy){m_refreshQueued=true;return;}if(!isConfigured()){clearView();m_status=Language::key("Configure the EBALIA Patreon service to synchronize posts.");emit changed();return;}
    if(m_pendingUntil&&QDateTime::currentSecsSinceEpoch()>m_pendingUntil){QSettings().remove(sessionKey());m_pendingUntil=0;m_timer->setInterval(60000);}
    m_busy=true;auto req=request(serviceUrl()+"/v1/feed");auto token=QSettings().value(sessionKey()).toByteArray();if(!token.isEmpty())req.setRawHeader("Authorization","Bearer "+token);auto reply=m_nam.get(req);auto generation=m_generation;auto url=serviceUrl();auto key=sessionKey();
    connect(reply,&QNetworkReply::finished,this,[this,reply,generation,url,key]{reply->deleteLater();if(generation!=m_generation)return;m_busy=false;if(url!=serviceUrl())return;
        if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()==401){QSettings().remove(key);clearView();m_pendingUntil=0;m_timer->setInterval(60000);refreshNews();return;}
        auto d=QJsonDocument::fromJson(reply->readAll()).object();if(reply->error()!=QNetworkReply::NoError){clearView();m_status=Language::key("Could not connect to Patreon. Try again.");emit changed();return;}
        if(d["pending"].toBool()){m_status=Language::key("Complete sign-in in your browser.");emit changed();return;}
        if(m_refreshQueued){m_refreshQueued=false;QTimer::singleShot(0,this,[this]{refreshNews();});return;}
        if(!d["posts"].isArray()||!d["identity"].isObject()){clearView();m_status=Language::key("Could not connect to Patreon. Try again.");emit changed();return;}
        m_availableTiers=d["tiers"].toArray();m_pendingUntil=0;m_timer->setInterval(60000);auto who=d["identity"].toObject();m_verified=hasTokens();m_paid=m_verified&&who["active"].toBool();m_name=who["name"].toString();m_title=who["tier"].toString();m_tierIds.clear();if(m_paid)for(auto id:who["tiers"].toArray())if(id.isString()&&!m_tierIds.contains(id.toString()))m_tierIds.append(id.toString());if(m_title.isEmpty())m_title=Language::key(who["active"].toBool()?"Active member":"Free · public posts");m_posts={};
        for(auto v:d["posts"].toArray()){auto post=v.toObject();if(!post["is_public"].isBool()||(!post["is_public"].toBool()&&!paidMember()))continue;QUrl link(post["url"].toString());if(link.scheme()!="https"||(link.host()!="www.patreon.com"&&link.host()!="patreon.com"))continue;post["title"]=plain(post["title"].toString());post["excerpt"]=plain(post["content"].toString()).left(1400);post["content"]=post["content"].toString().left(40000);m_posts.append(post);}
        m_revision=d["revision"].toString();watchUpdates();
        m_status=Language::key("Updated")+" · "+QDateTime::currentDateTime().toString("HH:mm:ss");emit changed();
    });
}
void PatreonAuth::watchUpdates(){
    if(m_events||!isConfigured()||qEnvironmentVariableIsSet("EBALIA_NO_NETWORK"))return;
    const auto url=serviceUrl();QUrl endpoint(url+"/v1/events");QUrlQuery query;query.addQueryItem("after",m_revision);endpoint.setQuery(query);
    auto reply=m_nam.get(request(endpoint.toString()));m_events=reply;
    connect(reply,&QNetworkReply::finished,this,[this,reply,url]{
        m_events=nullptr;reply->deleteLater();if(url!=serviceUrl())return;
        auto doc=QJsonDocument::fromJson(reply->readAll()).object();const auto revision=doc["revision"].toString();
        if(reply->error()!=QNetworkReply::NoError||revision.isEmpty()){QTimer::singleShot(5000,this,[this]{watchUpdates();});return;}
        if(revision!=m_revision){m_revision=revision;QJsonArray publicPosts;for(auto v:m_posts)if(v.toObject()["is_public"].toBool())publicPosts.append(v);m_posts=publicPosts;m_paid=false;emit changed();refreshNews();}
        QTimer::singleShot(0,this,[this]{watchUpdates();});
    });
}
void PatreonAuth::logout(){
    ++m_generation;m_busy=false;auto token=QSettings().value(sessionKey()).toByteArray();QSettings().remove(sessionKey());if(isConfigured()&&!token.isEmpty()){auto req=request(serviceUrl()+"/v1/logout");req.setRawHeader("Authorization","Bearer "+token);auto reply=m_nam.post(req,QByteArray{});connect(reply,&QNetworkReply::finished,reply,&QObject::deleteLater);}clearView();m_pendingUntil=0;m_timer->setInterval(60000);m_status.clear();emit changed();refreshNews();
}
