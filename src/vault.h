#pragma once
#include <QtCore>
namespace vault {
QByteArray seal(const QByteArray &plain,const QString &password);
QByteArray open(const QByteArray &data,const QString &password);
}
