#pragma once
#include <QString>
#include <QUrl>
#include <QCryptographicHash>
#include <functional>
namespace Download {void file(const QUrl &url,const QString &path,const QByteArray &hash={},QCryptographicHash::Algorithm algorithm=QCryptographicHash::Sha256,std::function<void(qint64,qint64)> progress={});}
