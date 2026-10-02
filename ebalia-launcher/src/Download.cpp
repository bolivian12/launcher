#include "Download.hpp"
#include <QtNetwork>
#include <QSaveFile>
#include <stdexcept>
void Download::file(const QUrl &url,const QString &path,const QByteArray &expected,QCryptographicHash::Algorithm algorithm,std::function<void(qint64,qint64)> progress){
    auto fail=[](QString s){throw std::runtime_error(s.toStdString());};
    if(url.scheme()!="https"||url.host().isEmpty()||!url.userInfo().isEmpty())fail("Invalid HTTPS download address");
    if(!expected.isEmpty()){QFile existing(path);if(existing.open(QIODevice::ReadOnly)){QCryptographicHash sum(algorithm);sum.addData(&existing);if(sum.result().toHex()==expected.toLower())return;}}
    QDir().mkpath(QFileInfo(path).absolutePath());QSaveFile target(path);if(!target.open(QIODevice::WriteOnly))fail("Could not write "+path);
    QNetworkAccessManager nam;nam.setTransferTimeout(30000);QNetworkRequest req(url);req.setHeader(QNetworkRequest::UserAgentHeader,"EBALIA-Launcher/4.0");req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::NoLessSafeRedirectPolicy);auto reply=nam.get(req);reply->setReadBufferSize(2*1024*1024);QEventLoop loop;QTimer deadline;deadline.setSingleShot(true);QObject::connect(&deadline,&QTimer::timeout,reply,&QNetworkReply::abort);deadline.start(30*60*1000);QCryptographicHash sum(algorithm);bool writeError=false;qint64 total=0;
    auto consume=[&]{auto bytes=reply->readAll();total+=bytes.size();if(total>4LL*1024*1024*1024||target.write(bytes)!=bytes.size()){writeError=true;reply->abort();return;}sum.addData(bytes);};
    QObject::connect(reply,&QNetworkReply::readyRead,&loop,consume);QObject::connect(reply,&QNetworkReply::downloadProgress,&loop,[&](qint64 a,qint64 b){if(progress)progress(a,b);});QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);loop.exec();consume();
    if(writeError)fail("Download could not be written or exceeds 4 GB");if(reply->error()!=QNetworkReply::NoError)fail(reply->errorString());if(!expected.isEmpty()&&sum.result().toHex()!=expected.toLower())fail("Download checksum mismatch: "+url.fileName());if(!target.commit())fail("Could not commit download");
}
