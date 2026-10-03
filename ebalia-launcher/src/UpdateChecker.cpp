#include "UpdateChecker.hpp"
#include <QCoreApplication>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVersionNumber>
#include <QRegularExpression>
#include <QTimer>
UpdateChecker::UpdateChecker(QObject *parent):QObject(parent){m_nam.setTransferTimeout(15000);}
bool UpdateChecker::newerStable(QString tag,const QString &current){
 if(tag.startsWith('v'))tag.remove(0,1);
 static const QRegularExpression semver("^[0-9]+\\.[0-9]+\\.[0-9]+$");
 return semver.match(tag).hasMatch() && QVersionNumber::compare(QVersionNumber::fromString(tag),QVersionNumber::fromString(current.startsWith('v')?current.mid(1):current))>0;
}
QString UpdateChecker::releasePage(const QJsonObject &release,bool gitgud){
 const QUrl url(gitgud?release["_links"].toObject()["self"].toString():release["html_url"].toString());
 const QString prefix=gitgud?"/castigarse/launcher/-/releases/":"/bolivian12/launcher/releases/tag/";
 if(url.scheme()!="https"||url.host()!=(gitgud?"gitgud.io":"github.com")||!url.path().startsWith(prefix)||!url.userInfo().isEmpty())return {};
 return url.toString();
}
void UpdateChecker::check(){if(m_busy||qEnvironmentVariableIsSet("EBALIA_NO_NETWORK"))return;m_busy=true;fetch(false);}
void UpdateChecker::fetch(bool gitgud){
 QNetworkRequest request{QUrl(gitgud?"https://gitgud.io/api/v4/projects/castigarse%2Flauncher/releases?per_page=20":"https://api.github.com/repos/bolivian12/launcher/releases?per_page=20")};
 request.setHeader(QNetworkRequest::UserAgentHeader,"EBALIA-Launcher/"+QCoreApplication::applicationVersion());
 auto reply=m_nam.get(request);
 connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 n,qint64){if(n>2*1024*1024)reply->abort();});
 connect(reply,&QNetworkReply::finished,this,[this,reply,gitgud]{
  reply->deleteLater();const auto doc=QJsonDocument::fromJson(reply->readAll());
  if(reply->error()!=QNetworkReply::NoError||!doc.isArray()){if(!gitgud){fetch(true);return;}m_busy=false;emit checkFailed("No se pudo consultar GitHub ni GitGud.");return;}
  QString best=QCoreApplication::applicationVersion(),page;
  for(auto v:doc.array()){auto release=v.toObject();auto tag=release["tag_name"].toString();if(release["draft"].toBool()||release["prerelease"].toBool()||release["upcoming_release"].toBool())continue;auto url=releasePage(release,gitgud);if(url.isEmpty()||!newerStable(tag,best))continue;best=tag;page=url;}
  if(page.isEmpty()&&!gitgud){fetch(true);return;}
  m_busy=false;if(page.isEmpty()){emit upToDate();return;}m_releaseUrl=page;emit updateAvailable(best);
 });
}
