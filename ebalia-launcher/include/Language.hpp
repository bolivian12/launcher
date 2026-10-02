#pragma once
#include <QString>
#include <QStringList>
namespace Language {
extern QString current;
QString text(const char *es,const char *en,const char *pt);
QString message(QString text);
QStringList available();
QString key(const QString &source);
QString standard(const QString &source);
}
