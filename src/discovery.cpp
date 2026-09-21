#include "discovery.h"
#include "dvrip.h"
#include <QtEndian>
namespace lan {
static const QString DeviceNs="http://www.onvif.org/ver10/device/wsdl";
static const QString MediaNs="http://www.onvif.org/ver10/media/wsdl";
static const QString Media2Ns="http://www.onvif.org/ver20/media/wsdl";
QDomDocument xmlDocument(const QByteArray &data){
 if(data.size()>1024*1024 || data.toUpper().contains("<!DOCTYPE") || data.toUpper().contains("<!ENTITY"))throw std::runtime_error("Unsafe or oversized XML reply");
 QXmlStreamReader reader(data);int depth=0,nodes=0;
 while(!reader.atEnd()){auto token=reader.readNext();if(token==QXmlStreamReader::DTD || token==QXmlStreamReader::EntityReference)throw std::runtime_error("XML entities are not supported");if(reader.isStartElement()){if(++depth>128 || ++nodes>20000)throw std::runtime_error("XML reply too complex");}if(reader.isEndElement())--depth;}
 if(reader.hasError())throw std::runtime_error("Invalid XML reply");
 QDomDocument doc;if(!doc.setContent(data,true))throw std::runtime_error("Invalid XML reply");return doc;
}
QList<QDomElement> elements(const QDomNode &node,const QString &name){
 QList<QDomElement> result;QList<QDomNode> pending{node};int count=0;
 while(!pending.isEmpty()){auto next=pending.takeLast();if(++count>20000)throw std::runtime_error("XML reply too complex");auto e=next.toElement();if(!e.isNull() && e.localName()==name)result.append(e);for(auto child=next.lastChild();!child.isNull();child=child.previousSibling())pending.append(child);}
 return result;
}
QString value(const QDomNode &node,const QString &name){auto list=elements(node,name);return list.isEmpty()?QString():list.first().text().trimmed();}
QByteArray probe(const QString &id){return QString(R"(<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery" xmlns:dn="http://www.onvif.org/ver10/network/wsdl"><s:Header><a:MessageID>%1</a:MessageID><a:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</a:To><a:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe</a:Action></s:Header><s:Body><d:Probe><d:Types>dn:NetworkVideoTransmitter</d:Types></d:Probe></s:Body></s:Envelope>)").arg(id.toHtmlEscaped()).toUtf8();}
QVector<Device> parseOnvif(const QByteArray &xml,const QHostAddress &sender,const QSet<QString> &requests){
 auto doc=xmlDocument(xml);QVector<Device> result;
 if(!requests.contains(value(doc,"RelatesTo")))return result;
 for(auto match:elements(doc,"ProbeMatch")){
  for(const auto &text:value(match,"XAddrs").split(QRegularExpression("\\s+"),Qt::SkipEmptyParts)){
   QUrl url(text);if(!url.isValid() || !QStringList{"http","https"}.contains(url.scheme()) || !url.userInfo().isEmpty() || QHostAddress(url.host())!=sender)continue;
   Device d;d.kind="ONVIF";d.host=sender.toString();d.endpoint=url;d.id=value(match,"Address");d.name="ONVIF device";
   for(auto scope:value(match,"Scopes").split(' ')){QString prefix="onvif://www.onvif.org/name/";if(scope.startsWith(prefix))d.name=QUrl::fromPercentEncoding(scope.mid(prefix.size()).toUtf8());}
   result.append(d);break;
  }
 }return result;
}
Device parseXm(const QByteArray &bytes,const QHostAddress &sender){
 if(bytes.size()<20 || qFromLittleEndian<quint16>(bytes.constData()+14)!=1531)throw std::runtime_error("Not an XM discovery response");
 auto size=xm::payloadSize(bytes.left(20));if(size!=quint32(bytes.size()-20))throw std::runtime_error("Truncated XM discovery response");
 auto body=bytes.mid(20);while(body.endsWith('\0')||body.endsWith('\n'))body.chop(1);
 auto object=QJsonDocument::fromJson(body).object();auto net=object.value("NetWork.NetCommon").toObject();
 if(object.value("Ret").toInt()!=100 || net.isEmpty())throw std::runtime_error("Invalid XM discovery response");
 Device d;d.kind="XMEye";d.name=net.value("HostName").toString("XM recorder / camera");d.host=sender.toString();d.id=net.value("SN").toString();d.port=net.value("TCPPort").toInt(34567);
 if(d.port<1||d.port>65535)throw std::runtime_error("Invalid XM service port");d.endpoint.setScheme("dvrip");d.endpoint.setHost(d.host);d.endpoint.setPort(d.port);return d;
}
Scanner::Scanner(QObject *parent):QObject(parent){
 repeat.setInterval(1800);finish.setSingleShot(true);finish.setInterval(6000);
 connect(&repeat,&QTimer::timeout,this,&Scanner::transmit);connect(&finish,&QTimer::timeout,this,[this]{stop();emit finished();});
 connect(&xmSocket,&QUdpSocket::readyRead,this,[this]{int limit=0;while(xmSocket.hasPendingDatagrams() && limit++<512){auto d=xmSocket.receiveDatagram(65536);try{publish(parseXm(d.data(),d.senderAddress()));}catch(...) {}}});
}
void Scanner::publish(const Device &d){auto key=d.kind+"|"+d.endpoint.toString();if(seen.contains(key)||seen.size()>=512)return;seen.insert(key);emit found(d);}
void Scanner::stop(){repeat.stop();finish.stop();xmSocket.close();for(auto *socket:onvifSockets)delete socket;onvifSockets.clear();}
void Scanner::start(int interfaceIndex){
 stop();requests.clear();seen.clear();interfaces.clear();
 for(auto iface:QNetworkInterface::allInterfaces())if((!interfaceIndex || iface.index()==interfaceIndex) && iface.flags().testFlag(QNetworkInterface::IsUp) && !iface.flags().testFlag(QNetworkInterface::IsLoopBack)){
  bool ipv4=false;for(auto address:iface.addressEntries())if(address.ip().protocol()==QAbstractSocket::IPv4Protocol)ipv4=true;
  if(ipv4)interfaces.append(iface);
 }
 if(interfaces.isEmpty()){emit status("No active IPv4 network interface found.");emit finished();return;}
 if(!xmSocket.bind(QHostAddress::AnyIPv4,34569,QUdpSocket::ShareAddress|QUdpSocket::ReuseAddressHint))emit status("XM discovery port 34569 is unavailable; ONVIF scanning continues.");
 for(const auto &iface:interfaces){
  auto *socket=new QUdpSocket(this);if(!socket->bind(QHostAddress::AnyIPv4,0)){delete socket;continue;}
  socket->setMulticastInterface(iface);socket->setSocketOption(QAbstractSocket::MulticastTtlOption,1);onvifSockets.append(socket);
  connect(socket,&QUdpSocket::readyRead,this,[this,socket]{int limit=0;while(socket->hasPendingDatagrams() && limit++<512){auto d=socket->receiveDatagram(65536);try{for(auto device:parseOnvif(d.data(),d.senderAddress(),requests))publish(device);}catch(...) {}}});
 }
 transmit();repeat.start();finish.start();
}
void Scanner::transmit(){
 auto id="urn:uuid:"+QUuid::createUuid().toString(QUuid::WithoutBraces);requests.insert(id);auto xml=probe(id);
 for(auto *socket:onvifSockets)socket->writeDatagram(xml,QHostAddress("239.255.255.250"),3702);
 if(xmSocket.state()==QAbstractSocket::BoundState){auto bytes=xm::header(0,0,1530,0);
  for(auto iface:interfaces)for(auto address:iface.addressEntries())if(!address.broadcast().isNull())xmSocket.writeDatagram(bytes,address.broadcast(),34569);
 }
}
Onvif::Onvif(const QUrl &device,QString username,QString secret,std::atomic_bool &cancelled):cancel(cancelled),user(std::move(username)),password(std::move(secret)),host(device.host()){}
QUrl Onvif::endpoint(QUrl url) const {
 if(!url.isValid() || !QStringList{"http","https"}.contains(url.scheme()) || url.host().compare(host,Qt::CaseInsensitive)!=0 || !url.userInfo().isEmpty())throw std::runtime_error("ONVIF returned a service address on a different host or an invalid service URL");return url;
}
QDomDocument Onvif::request(QUrl url,QString space,QString operation,QString body,bool authentication){
 if(cancel)throw std::runtime_error("Cancelled");url=endpoint(url);
 QString security;
 if(authentication && !user.isEmpty()){
  QByteArray nonce;for(int i=0;i<4;++i){quint32 n=QRandomGenerator::system()->generate();nonce.append(reinterpret_cast<const char*>(&n),4);}
  auto created=QDateTime::currentDateTimeUtc().addMSecs(clockOffset).toString(Qt::ISODateWithMs);
  auto digest=QCryptographicHash::hash(nonce+created.toUtf8()+password.toUtf8(),QCryptographicHash::Sha1).toBase64();
  security=QString(R"(<wsse:Security s:mustUnderstand="1" xmlns:wsse="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd" xmlns:wsu="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd"><wsse:UsernameToken><wsse:Username>%1</wsse:Username><wsse:Password Type="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest">%2</wsse:Password><wsse:Nonce EncodingType="http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary">%3</wsse:Nonce><wsu:Created>%4</wsu:Created></wsse:UsernameToken></wsse:Security>)").arg(user.toHtmlEscaped(),QString::fromLatin1(digest),QString::fromLatin1(nonce.toBase64()),created);
 }
 auto xml=QString(R"(<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:m="%1" xmlns:tt="http://www.onvif.org/ver10/schema"><s:Header>%2</s:Header><s:Body><m:%3>%4</m:%3></s:Body></s:Envelope>)").arg(space,security,operation,body).toUtf8();
 QNetworkRequest req(url);req.setHeader(QNetworkRequest::ContentTypeHeader,"application/soap+xml; charset=utf-8; action=\""+space+"/"+operation+"\"");req.setRawHeader("SOAPAction",("\""+space+"/"+operation+"\"").toUtf8());req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);req.setTransferTimeout(8000);
 QEventLoop loop;QTimer poll;poll.setInterval(50);QElapsedTimer deadline;deadline.start();auto *reply=manager.post(req,xml);int authAttempts=0;
 auto auth=QObject::connect(&manager,&QNetworkAccessManager::authenticationRequired,&loop,[&](QNetworkReply *r,QAuthenticator *a){if(r==reply && authentication && authAttempts++==0){a->setUser(user);a->setPassword(password);}});
 QByteArray data;bool oversized=false,timedout=false;
 QObject::connect(reply,&QNetworkReply::readyRead,&loop,[&]{data+=reply->readAll();if(data.size()>1024*1024){oversized=true;reply->abort();}});
 QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);
 QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(cancel || deadline.elapsed()>8000){timedout=true;reply->abort();}});poll.start();if(!reply->isFinished())loop.exec();QObject::disconnect(auth);
 data+=reply->readAll();auto code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();auto error=reply->error();delete reply;
 if(cancel)throw std::runtime_error("Cancelled");if(oversized)throw std::runtime_error("ONVIF reply too large");if(timedout)throw std::runtime_error("ONVIF request timed out");
 if(code==401||code==403)throw std::runtime_error("ONVIF authentication rejected. Check the ONVIF account and password.");
 if(code>=300 && code<400)throw std::runtime_error("ONVIF redirected the request; enter the final service URL manually.");
 if(error!=QNetworkReply::NoError && data.isEmpty())throw std::runtime_error(QString("Cannot reach ONVIF %1 at %2://%3:%4%5. Check the camera's ONVIF port and VPN route/firewall; working RTSP video does not prove this service is reachable.").arg(operation,url.scheme(),url.host()).arg(url.port(url.scheme()=="https"?443:80)).arg(url.path()).toStdString());
 auto doc=xmlDocument(data);if(!elements(doc,"Fault").isEmpty())throw std::runtime_error("ONVIF SOAP request rejected (check account, clock and supported services)");
 if(code<200||code>=300)throw std::runtime_error("ONVIF HTTP request failed");return doc;
}
QVector<Profile> Onvif::profiles(const QUrl &device){
 try {auto doc=request(device,DeviceNs,"GetSystemDateAndTime",{},false);auto utc=elements(doc,"UTCDateTime");if(!utc.isEmpty()){auto e=utc.first();QDate date(value(e,"Year").toInt(),value(e,"Month").toInt(),value(e,"Day").toInt());QTime time(value(e,"Hour").toInt(),value(e,"Minute").toInt(),value(e,"Second").toInt());if(date.isValid()&&time.isValid())clockOffset=QDateTime::currentDateTimeUtc().msecsTo(QDateTime(date,time,Qt::UTC));}}catch(...) {if(cancel)throw;}
 QList<QPair<QUrl,bool>> services;
 try{auto doc=request(device,DeviceNs,"GetServices","<m:IncludeCapability>false</m:IncludeCapability>");for(auto service:elements(doc,"Service")){auto ns=value(service,"Namespace");if(ns==MediaNs||ns==Media2Ns)services.append({endpoint(QUrl(value(service,"XAddr"))),ns==Media2Ns});}}catch(...) {if(cancel)throw;}
 if(services.isEmpty()){auto doc=request(device,DeviceNs,"GetCapabilities","<m:Category>Media</m:Category>");for(auto media:elements(doc,"Media")){auto address=value(media,"XAddr");if(!address.isEmpty())services.append({endpoint(QUrl(address)),false});}}
 if(services.isEmpty())throw std::runtime_error("No ONVIF Media or Media2 service advertised");
 QString lastError;
 for(auto service:services){try{auto doc=request(service.first,service.second?Media2Ns:MediaNs,"GetProfiles",service.second?"<m:Type>All</m:Type>":QString());QVector<Profile> profiles;
  for(auto e:elements(doc,"Profiles")){auto token=e.attribute("token");if(token.isEmpty())continue;Profile p;p.token=token;p.name=value(e,"Name");if(p.name.isEmpty())p.name="Profile "+token;p.source=value(e,"SourceToken");auto configs=elements(e,"PTZConfiguration");if(configs.isEmpty())configs=elements(e,"PTZ");if(!configs.isEmpty())p.ptzConfig=configs.first().attribute("token");p.endpoint=service.first;p.media2=service.second;profiles.append(p);if(profiles.size()>=256)break;}
  if(!profiles.isEmpty())return profiles;
 }catch(const std::exception &e){if(cancel)throw;lastError=e.what();}}
 throw std::runtime_error(lastError.isEmpty()?"No ONVIF video profiles returned":lastError.toStdString());
}
QUrl Onvif::ptzService(const QUrl &device){
 try{auto doc=request(device,DeviceNs,"GetCapabilities","<m:Category>PTZ</m:Category>");
 for(auto e:elements(doc,"PTZ")){auto address=value(e,"XAddr");if(!address.isEmpty())return endpoint(QUrl(address));}}catch(const std::exception &){if(cancel)throw;}
 auto services=request(device,DeviceNs,"GetServices","<m:IncludeCapability>false</m:IncludeCapability>");
 for(auto e:elements(services,"Service"))if(value(e,"Namespace")=="http://www.onvif.org/ver20/ptz/wsdl")return endpoint(QUrl(value(e,"XAddr")));
 throw std::runtime_error("This device does not advertise ONVIF PTZ");
}
QUrl Onvif::stream(const Profile &p){
 auto token=p.token.toHtmlEscaped();QString body=p.media2?"<m:Protocol>RTSP</m:Protocol><m:ProfileToken>"+token+"</m:ProfileToken>":"<m:StreamSetup><tt:Stream>RTP-Unicast</tt:Stream><tt:Transport><tt:Protocol>RTSP</tt:Protocol></tt:Transport></m:StreamSetup><m:ProfileToken>"+token+"</m:ProfileToken>";
 auto doc=request(p.endpoint,p.media2?Media2Ns:MediaNs,"GetStreamUri",body);QUrl url(value(doc,"Uri"));
 if(!url.isValid() || !QStringList{"rtsp","rtsps","http","https"}.contains(url.scheme()) || url.host().isEmpty())throw std::runtime_error("ONVIF returned an unsupported stream URI");
 if(url.host()=="0.0.0.0")url.setHost(host);
 if(url.host().compare(host,Qt::CaseInsensitive)!=0)throw std::runtime_error("ONVIF stream points to a different host. Add that stream manually.");
 if(!user.isEmpty()){url.setUserName(user);url.setPassword(password);}return url;
}
}
