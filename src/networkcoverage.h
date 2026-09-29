#pragma once
#include "archivetimeline.h"
#include <QJsonArray>
#include <QJsonObject>
namespace archive {
// Recorder timestamps are wall-clock values, not the workstation timezone.
inline QVector<Clip> recordingRanges(const QJsonArray &rows, QDate day, int channel) {
 QVector<Clip> result; const QDateTime midnight(day,QTime(0,0),Qt::UTC);
 QSet<QString> seen;
 for(const auto &value:rows){
  const auto row=value.toObject();
  auto begin=QDateTime::fromString(row["BeginTime"].toString(),"yyyy-MM-dd HH:mm:ss");
  auto end=QDateTime::fromString(row["EndTime"].toString(),"yyyy-MM-dd HH:mm:ss");
  begin.setTimeSpec(Qt::UTC);end.setTimeSpec(Qt::UTC);
  if(!begin.isValid()||!end.isValid()||end<=begin)continue;
  if(row.contains("Channel")&&row["Channel"].isDouble()&&row["Channel"].toInt()!=channel)continue;
  const int b=int(qBound<qint64>(0LL,midnight.secsTo(begin),86400LL));
  const int e=int(qBound<qint64>(0LL,midnight.secsTo(end),86400LL));
  const QString key=QString::number(b)+":"+QString::number(e);
  if(b>=e||seen.contains(key))continue;
  seen.insert(key);result.append({channel,b,e,row["FileName"].toString()});
 }
 std::sort(result.begin(),result.end(),[](const Clip&a,const Clip&b){return a.begin<b.begin;});
 return result;
}
}
