#include <QtTest>
#include "discovery.h"
#include "dvrip.h"
class DiscoveryTest:public QObject {
 Q_OBJECT
private slots:
 void xmReply(){QByteArray body=R"({"Ret":100,"NetWork.NetCommon":{"HostName":"Test NVR","TCPPort":34570,"SN":"123"}})";auto h=xm::header(0,0,1531,body.size());h[1]=1;auto device=lan::parseXm(h+body,QHostAddress("192.168.1.2"));QCOMPARE(device.name,QString("Test NVR"));QCOMPARE(device.port,34570);QCOMPARE(device.host,QString("192.168.1.2"));QVERIFY_EXCEPTION_THROWN(lan::parseXm(h+body.left(5),QHostAddress::LocalHost),std::runtime_error);}
 void probeReply(){
  QByteArray xml=R"(<s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery"><s:Header><a:RelatesTo>urn:uuid:test</a:RelatesTo></s:Header><s:Body><d:ProbeMatches><d:ProbeMatch><a:EndpointReference><a:Address>urn:uuid:camera</a:Address></a:EndpointReference><d:Scopes>onvif://www.onvif.org/name/Front%20door</d:Scopes><d:XAddrs>http://192.168.1.3/onvif/device_service</d:XAddrs></d:ProbeMatch></d:ProbeMatches></s:Body></s:Envelope>)";
  auto devices=lan::parseOnvif(xml,QHostAddress("192.168.1.3"),{"urn:uuid:test"});QCOMPARE(devices.size(),1);QCOMPARE(devices[0].name,QString("Front door"));QVERIFY(lan::parseOnvif(xml,QHostAddress("192.168.1.3"),{"other"}).isEmpty());QVERIFY(lan::parseOnvif(xml,QHostAddress("192.168.1.4"),{"urn:uuid:test"}).isEmpty());
  auto probe=lan::xmlDocument(lan::probe("urn:uuid:test"));QCOMPARE(lan::value(probe,"MessageID"),QString("urn:uuid:test"));
 }
 void rejectXml(){QVERIFY_EXCEPTION_THROWN(lan::xmlDocument("<!DOCTYPE a [<!ENTITY e 'test'>]><a>&e;</a>"),std::runtime_error);QVERIFY_EXCEPTION_THROWN(lan::xmlDocument(QByteArray(1024*1024+1,'x')),std::runtime_error);QByteArray deep;for(int i=0;i<150;++i)deep+="<a>";for(int i=0;i<150;++i)deep+="</a>";QVERIFY_EXCEPTION_THROWN(lan::xmlDocument(deep),std::runtime_error);}
 void cancellation(){QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));std::atomic_bool cancel{false},done{false};QString error;
  auto *worker=QThread::create([&]{try{lan::Onvif client(QUrl(QString("http://127.0.0.1:%1/device").arg(server.serverPort())),"admin","test",cancel);client.profiles(QUrl(QString("http://127.0.0.1:%1/device").arg(server.serverPort())));}catch(const std::exception &e){error=e.what();}done=true;});worker->start();QTest::qWait(100);cancel=true;worker->wait();delete worker;QVERIFY(done);QCOMPARE(error,QString("Cancelled"));
 }
 void onvif_data(){QTest::addColumn<int>("mode");QTest::newRow("Media WSSE")<<0;QTest::newRow("Media2 WSSE")<<1;QTest::newRow("Capabilities fallback")<<2;QTest::newRow("Wrong credentials")<<3;QTest::newRow("Foreign stream rejected")<<4;QTest::newRow("HTTP authentication")<<5;}
 void onvif(){
  QFETCH(int,mode);QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QString base=QString("http://127.0.0.1:%1").arg(server.serverPort());int authenticated=0;bool digestValid=true;
  connect(&server,&QTcpServer::newConnection,this,[&]{auto *socket=server.nextPendingConnection();auto buffer=std::make_shared<QByteArray>();connect(socket,&QTcpSocket::readyRead,this,[&,socket,buffer]{
   *buffer+=socket->readAll();int headerEnd=buffer->indexOf("\r\n\r\n");if(headerEnd<0)return;auto headers=buffer->left(headerEnd);QRegularExpression length("content-length: (\\d+)",QRegularExpression::CaseInsensitiveOption);int size=length.match(QString::fromLatin1(headers)).captured(1).toInt();if(buffer->size()<headerEnd+4+size)return;
   auto doc=lan::xmlDocument(buffer->mid(headerEnd+4,size));QString op;for(QString name:{"GetSystemDateAndTime","GetServices","GetCapabilities","GetProfiles","GetStreamUri"})if(!lan::elements(doc,name).isEmpty())op=name;
   int code=200;QString response,extra;
   if(op!="GetSystemDateAndTime"){
    auto nonce=QByteArray::fromBase64(lan::value(doc,"Nonce").toLatin1());auto created=lan::value(doc,"Created");auto expected=QCryptographicHash::hash(nonce+created.toUtf8()+QByteArray("p&ss"),QCryptographicHash::Sha1).toBase64();
    digestValid &= lan::value(doc,"Username")=="u&ser" && lan::value(doc,"Password")==QString::fromLatin1(expected);++authenticated;
   }
   if(op=="GetSystemDateAndTime")response="<GetSystemDateAndTimeResponse/>";
   else if(mode==3){code=401;response="denied";}
   else if(mode==5 && !headers.toLower().contains("authorization:")){code=401;extra="WWW-Authenticate: Basic realm=\"camera\"\r\n";response="authenticate";}
   else if(op=="GetServices"){
    if(mode==2)response="<s:Fault><s:Reason>Unsupported</s:Reason></s:Fault>";
    else response="<GetServicesResponse><Service><Namespace>http://www.onvif.org/"+QString(mode==1?"ver20":"ver10")+"/media/wsdl</Namespace><XAddr>"+base+"/media</XAddr></Service></GetServicesResponse>";
   }else if(op=="GetCapabilities")response="<GetCapabilitiesResponse><Capabilities><Media><XAddr>"+base+"/media</XAddr></Media></Capabilities></GetCapabilitiesResponse>";
   else if(op=="GetProfiles")response="<GetProfilesResponse><Profiles token=\"main\"><Name>Front camera</Name><VideoSourceConfiguration><SourceToken>source1</SourceToken></VideoSourceConfiguration></Profiles></GetProfilesResponse>";
   else if(op=="GetStreamUri")response="<GetStreamUriResponse><MediaUri><Uri>rtsp://"+QString(mode==4?"192.0.2.1":"127.0.0.1")+"/live</Uri></MediaUri></GetStreamUriResponse>";
   else {code=500;response="unknown operation";}
   if(op=="GetProfiles"&&mode==1){response="<GetProfilesResponse><Profiles token=\"main\"><Name>Front camera</Name>";if(lan::value(doc,"Type")=="All")response+="<Configurations><PTZ token=\"ptz-main\"/></Configurations>";response+="</Profiles></GetProfilesResponse>";}
   auto xml=("<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns=\"http://www.onvif.org/ver10/media/wsdl\"><s:Body>"+response+"</s:Body></s:Envelope>").toUtf8();
   socket->write("HTTP/1.1 "+QByteArray::number(code)+" Reply\r\nContent-Type: application/soap+xml\r\n"+extra.toUtf8()+"Content-Length: "+QByteArray::number(xml.size())+"\r\nConnection: close\r\n\r\n"+xml);socket->disconnectFromHost();
  });});
  std::atomic_bool cancelled{false},done{false};QString error,ptzConfig;QUrl stream;int count=0;
  auto *worker=QThread::create([&]{try{lan::Onvif client(QUrl(base+"/device"),"u&ser","p&ss",cancelled);auto profiles=client.profiles(QUrl(base+"/device"));count=profiles.size();ptzConfig=profiles[0].ptzConfig;stream=client.stream(profiles[0]);}catch(const std::exception &e){error=e.what();}done=true;});worker->start();
  QElapsedTimer wait;wait.start();while(!done && wait.elapsed()<10000)QTest::qWait(10);cancelled=true;worker->wait();delete worker;
  QVERIFY(done);QVERIFY(digestValid);QVERIFY(authenticated>0);
  if(mode==3 || mode==4){QVERIFY(!error.isEmpty());QVERIFY(stream.isEmpty());}else{QVERIFY2(error.isEmpty(),qPrintable(error));QCOMPARE(count,1);if(mode==1)QCOMPARE(ptzConfig,QString("ptz-main"));QCOMPARE(stream.host(),QString("127.0.0.1"));QCOMPARE(stream.password(),QString("p&ss"));}
 }
};
QTEST_GUILESS_MAIN(DiscoveryTest)
#include "discovery_test.moc"
