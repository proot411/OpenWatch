#include "dvrip.h"
#include <QtEndian>
namespace xm {

int channelCount(const QJsonObject &login, const QJsonObject &system) {
 auto count=[](const QJsonValue &value) {
  bool ok=false; int n=0;
  if(value.isDouble()) { double d=value.toDouble(); if(d<0 || d>256 || d!=int(d)) throw std::runtime_error("Invalid recorder channel count"); n=int(d); ok=true; }
  else if(value.isString()) {auto text=value.toString();n=text.toInt(&ok,text.startsWith("0x",Qt::CaseInsensitive)?16:10);}
  else if(value.isUndefined() || value.isNull()) return 0;
  if(!ok || n<0 || n>256) throw std::runtime_error("Invalid recorder channel count");
  return n;
 };
 int channels=count(login.value("ChannelNum"));
 if(channels>0) return channels;
 // Digital input counts vary across firmware. Do not add possibly overlapping counts.
 return qMax(count(system.value("VideoInChannel")),count(system.value("DigChannel")));
}
QVector<QUrl> channelUrls(const QUrl &recorder,int count,bool substream) {
 if(count<1 || count>256) throw std::runtime_error("Invalid recorder channel count");
 QVector<QUrl> urls;
 for(int i=0;i<count;++i) {QUrl url=recorder;QUrlQuery query;query.addQueryItem("channel",QString::number(i));query.addQueryItem("subtype",substream?"1":"0");url.setQuery(query);urls.append(url);}
 return urls;
}
QString digest(const QString &password) {
 const auto hash=QCryptographicHash::hash(password.toUtf8(),QCryptographicHash::Md5);
 const char *alphabet="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
 QString result;
 for(int i=0;i<16;i+=2) result+=alphabet[(quint8(hash[i])+quint8(hash[i+1]))%62];
 return result;
}
QByteArray header(quint32 session, quint32 sequence, quint16 command, quint32 size) {
 if(size>MaxPayload) throw std::runtime_error("DVRIP payload too large");
 QByteArray b(20,0); b[0]=char(255);
 qToLittleEndian(session,b.data()+4); qToLittleEndian(sequence,b.data()+8);
 qToLittleEndian(command,b.data()+14); qToLittleEndian(size,b.data()+16); return b;
}
quint32 payloadSize(const QByteArray &b) {
 if(b.size()!=20) throw std::runtime_error("Incomplete DVRIP header (expected 20 bytes)");
 if(quint8(b[0])!=255) throw std::runtime_error("The recorder returned a non-DVRIP reply. Check the DVRIP TCP port (usually 34567).");
 // Both version 0 and version 1 occur in public DVRIP implementations.
 const auto version=quint8(b[1]);
 if(version!=0 && version!=1) throw std::runtime_error(QString("Unsupported DVRIP header version %1 (expected 0 or 1)").arg(version).toStdString());
 auto size=qFromLittleEndian<quint32>(b.constData()+16);
 if(size>MaxPayload) throw std::runtime_error("DVRIP payload too large"); return size;
}
void MediaParser::append(const QByteArray &b) {
 if(buffer.size()+b.size()>2*MaxPayload) throw std::runtime_error("DVRIP media buffer overflow"); buffer+=b;
}
QByteArray MediaParser::take() {
 while(buffer.size()>=8) {
  if(buffer.left(3)!=QByteArray::fromHex("000001")) throw std::runtime_error("Invalid Sofia media prefix");
  auto type=quint8(buffer[3]); int prefix; quint32 length;
  if(type==0xfc || type==0xfe) {
   if(buffer.size()<16) return {};
   prefix=16; length=qFromLittleEndian<quint32>(buffer.constData()+12);
  } else if(type==0xfd) { prefix=8; length=qFromLittleEndian<quint32>(buffer.constData()+4); }
  else if(type==0xfa || type==0xf9) { prefix=8; length=qFromLittleEndian<quint16>(buffer.constData()+6); }
  else throw std::runtime_error("Unsupported Sofia media type");
  if(length>MaxPayload) throw std::runtime_error("Sofia frame too large");
  if(buffer.size()<prefix+length) return {};
  if(prefix==16) { codec=quint8(buffer[4]); fps=qBound(1,int(quint8(buffer[5])&31),120); }
  if(prefix==16){
   auto t=qFromLittleEndian<quint32>(buffer.constData()+8);
   QDate date(2000+int((t>>26)&63),int((t>>22)&15),int((t>>17)&31));
   QTime time(int((t>>12)&31),int((t>>6)&63),int(t&63));
   anchor=date.isValid()&&time.isValid()?QDateTime(date,time,Qt::UTC).toMSecsSinceEpoch():-1;sinceAnchor=0;
  }
  if(type==0xfc||type==0xfe||type==0xfd){timestamp=anchor<0?-1:anchor+qint64(sinceAnchor++)*1000/fps;}
  auto result=buffer.mid(prefix,length); buffer.remove(0,prefix+length);
  if(type==0xfc || type==0xfe || type==0xfd) return result;
 }
 return {};
}
QByteArray Client::exact(qint64 size) {
 QByteArray result; QElapsedTimer timer; timer.start();
 while(result.size()<size) {
  if(stop || timer.elapsed()>5000) throw std::runtime_error("DVRIP read cancelled or timed out");
  if(!socket.bytesAvailable()) { socket.waitForReadyRead(100); if(socket.state()!=QAbstractSocket::ConnectedState) throw std::runtime_error("DVRIP disconnected"); continue; }
  result+=socket.read(size-result.size());
 }
 return result;
}
QByteArray Client::chunk() { auto h=exact(20); auto size=payloadSize(h); session=qFromLittleEndian<quint32>(h.constData()+4); messageId=qFromLittleEndian<quint16>(h.constData()+14); return exact(size); }
void Client::send(quint16 command,QJsonObject body) {
 auto payload=QJsonDocument(body).toJson(QJsonDocument::Compact)+QByteArray("\n\0",2);
 auto bytes=header(session,sequence++,command,payload.size())+payload;
 if(socket.write(bytes)!=bytes.size()) throw std::runtime_error("DVRIP write failed");
 QElapsedTimer deadline; deadline.start();
 while(socket.bytesToWrite()) { if(stop || deadline.elapsed()>5000) throw std::runtime_error("DVRIP write timed out"); socket.waitForBytesWritten(100); }
}
QJsonObject Client::response(bool fileSearch) {
 auto bytes=chunk(); while(bytes.endsWith('\0') || bytes.endsWith('\n')) bytes.chop(1);
 QJsonParseError error; auto doc=QJsonDocument::fromJson(bytes,&error);
 if(error.error!=QJsonParseError::NoError || !doc.isObject()) throw std::runtime_error("Invalid DVRIP JSON response");
 int ret=doc.object().value("Ret").toInt();
 if(ret!=100 && ret!=515 && !(fileSearch && messageId==1441 && ret==119)) throw std::runtime_error(QString("DVRIP request rejected (code %1; check permissions, channel and firmware support)").arg(ret).toStdString());
 return doc.object();
}
QJsonObject Client::login(const QUrl &url) {
 socket.connectToHost(url.host(),url.port(34567));
 if(!socket.waitForConnected(3000)) throw std::runtime_error("Cannot connect to DVRIP device");
 send(1000,{{"EncryptType","MD5"},{"LoginType","DVRIP-Web"},{"UserName",url.userName().isEmpty()?"admin":url.userName()},{"PassWord",digest(url.password())}}); return response();
}
QJsonObject Client::control(quint16 command,const QString &name,const QJsonValue &parameters) {
 QJsonObject body{{"Name",name},{"SessionID",QString("0x%1").arg(session,8,16,QChar('0'))}};
 if(!parameters.isUndefined() && !parameters.isNull())body.insert(name,parameters);
 send(command,body);return response(command==1440 && name=="OPFileQuery");
}
void Client::ptz(int channel,const QString &command,int preset,int step){
 static const QStringList allowed{"DirectionUp","DirectionDown","DirectionLeft","DirectionRight","ZoomTile","ZoomWide","FocusNear","FocusFar","IrisSmall","IrisLarge","SetPreset","GotoPreset","ClearPreset"};
 if(channel<0||channel>255||!allowed.contains(command)||step<1||step>8)throw std::runtime_error("Invalid PTZ command");
 control(1400,"OPPTZControl",QJsonObject{{"Command",command},{"Parameter",QJsonObject{{"Channel",channel},{"Step",step},{"Preset",preset},{"Tour",0},{"Pattern","Start"},{"MenuOpts","Enter"},{"AUX",QJsonObject{{"Number",0},{"Status","On"}}}}}});
}
int Client::scanChannels(const QUrl &url) {
 auto info=login(url);
 int count=channelCount(info);
 if(count>0) return count;
 send(1020,{{"Name","SystemInfo"},{"SessionID",QString("0x%1").arg(session,8,16,QChar('0'))}});
 count=channelCount(info,response().value("SystemInfo").toObject());
 if(count<1) throw std::runtime_error("The recorder did not report a usable channel count. Add streams manually.");
 return count;
}
Client::~Client() {
 // Best-effort stop without waiting for a reply when the user cancels.
 if(archiveControl && !archiveRequest.isEmpty()) {
  auto body=archiveRequest;body["Action"]=download?"DownloadStop":"Stop";
  auto payload=QJsonDocument(QJsonObject{{"Name","OPPlayBack"},{"SessionID",QString("0x%1").arg(archiveControl->session,8,16,QChar('0'))},{"OPPlayBack",body}}).toJson(QJsonDocument::Compact)+QByteArray("\n\0",2);
  archiveControl->socket.write(header(archiveControl->session,archiveControl->sequence++,1420,payload.size())+payload);
  archiveControl->socket.flush(); // Do not wait on a cancelled session.
 }
}
QJsonArray Client::recordings(const QUrl &url,int channel,const QString &begin,const QString &end) {
 if(channel<0||channel>255||!QDateTime::fromString(begin,"yyyy-MM-dd HH:mm:ss").isValid()||!QDateTime::fromString(end,"yyyy-MM-dd HH:mm:ss").isValid()||begin>end)throw std::runtime_error("Invalid recording search range");
 login(url);
 auto search=[&](const QString &from,const QString &to){
  QJsonObject query{{"BeginTime",from},{"EndTime",to},{"Channel",channel},{"DriverTypeMask","0x0000FFFF"},{"Event","*"},{"StreamType","0x00000000"},{"Type","h264"}};
  auto reply=control(1440,"OPFileQuery",query);auto files=reply.value("OPFileQuery");
  if(reply.value("Ret").toInt()==119||files.isNull()||(files.isArray()&&files.toArray().isEmpty())){
   query.remove("DriverTypeMask");query.remove("StreamType");reply=control(1440,"OPFileQuery",query);files=reply.value("OPFileQuery");
  }
  if(reply.value("Ret").toInt()==119||files.isNull())return QJsonArray{};
  if(!files.isArray())throw std::runtime_error("Recorder returned an unsupported recording list");return files.toArray();
 };
 auto files=search(begin,end);if(!files.isEmpty())return files;
 // A bounded compatibility probe, not a claim that all firmware needs this.
 auto from=QDateTime::fromString(begin,"yyyy-MM-dd HH:mm:ss"),until=QDateTime::fromString(end,"yyyy-MM-dd HH:mm:ss");
 from.setTimeSpec(Qt::UTC);until.setTimeSpec(Qt::UTC);
 if(from.secsTo(until)<=3600)return {};
 if(from.secsTo(until)>86400)throw std::runtime_error("Choose a single day for the recording search");
 QSet<QString> seen;QElapsedTimer alive;alive.start();
 while(from<until){
  if(stop)throw std::runtime_error("Recording search cancelled");
  if(alive.elapsed()>10000){control(1006,"KeepAlive");alive.restart();}
  auto to=qMin(from.addSecs(3600),until);auto page=search(from.toString("yyyy-MM-dd HH:mm:ss"),to.toString("yyyy-MM-dd HH:mm:ss"));
  for(auto item:page){QString name=item.toObject()["FileName"].toString();if(name.isEmpty()||seen.contains(name))continue;seen.insert(name);files.append(item);}
  // Keep the existing caller's 64-item pagination convention. Do not truncate
  // a firmware page: its last BeginTime is needed for the next request.
  if(files.size()>=64)break;
  from=to; // Inclusive overlap; duplicate file names are removed above.
 }
 return files;
}
void Client::open(const QUrl &url) {
 QUrlQuery archiveQuery(url);
 if(archiveQuery.hasQueryItem("archiveFile")) {
  download=archiveQuery.queryItemValue("speed").toDouble()>1;archive=true;archiveControl=std::make_unique<Client>(stop);archiveControl->login(url);session=archiveControl->session;
  socket.connectToHost(url.host(),url.port(34567));
  if(!socket.waitForConnected(3000))throw std::runtime_error("Cannot connect to DVRIP archive data socket");
  archiveRequest={{"Action","Claim"},{"StartTime",archiveQuery.queryItemValue("begin",QUrl::FullyDecoded)},{"EndTime",archiveQuery.queryItemValue("end",QUrl::FullyDecoded)},
   {"Parameter",QJsonObject{{"PlayMode","ByName"},{"FileName",archiveQuery.queryItemValue("archiveFile",QUrl::FullyDecoded)},{"Channel",archiveQuery.queryItemValue("channel").toInt()},{"StreamType",0},{"Value",0},{"TransMode","TCP"}}}};
  control(1424,"OPPlayBack",archiveRequest);
  auto start=archiveRequest;start["Action"]=download?"DownloadStart":"Start";archiveControl->control(1420,"OPPlayBack",start);heartbeat.start();return;
 }
 login(url);
 QUrlQuery query(url); bool ok=false; auto value=query.queryItemValue("channel"); int channel=value.isEmpty()?0:value.toInt(&ok);
 if(!value.isEmpty() && (!ok || channel<0 || channel>255)) throw std::runtime_error("Channel must be 0–255");
 QJsonObject parameters{{"Channel",channel},{"CombinMode","NONE"},{"StreamType",query.queryItemValue("subtype")=="1"?"Extra1":"Main"},{"TransMode","TCP"}};
 auto sid=QString("0x%1").arg(session,8,16,QChar('0'));
 auto command=[&](QString action){return QJsonObject{{"Name","OPMonitor"},{"SessionID",sid},{"OPMonitor",QJsonObject{{"Action",action},{"Parameter",parameters}}}};};
 send(1413,command("Claim")); response(); send(1410,command("Start")); heartbeat.start();
}
void Client::keepArchiveAlive(){if(archiveControl&&heartbeat.elapsed()>10000){archiveControl->control(1006,"KeepAlive");heartbeat.restart();}}
QByteArray Client::readVideo() {
 while(!stop && !archiveEnded) {
  auto frame=parser.take(); if(!frame.isEmpty()) return frame;
  keepArchiveAlive();
  if(heartbeat.elapsed()>10000) { send(1006,{{"Name","KeepAlive"},{"SessionID",QString("0x%1").arg(session,8,16,QChar('0'))}}); heartbeat.restart(); }
  auto data=chunk();
  if(messageId==(archive?1422:1412) || (archive && messageId==1426)) {if(archive && data.isEmpty()){archiveEnded=true;return {};}parser.append(data);}
  else if(archive && messageId==1421) {throw std::runtime_error("Recorder ended archive playback");}
 }
 return {};
}
}
