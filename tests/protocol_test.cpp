#include <QtTest>
#include <QtEndian>
#include "dvrip.h"
class ProtocolTest:public QObject {
 Q_OBJECT
private slots:
 void channelCounts(){
  QCOMPARE(xm::channelCount({{"ChannelNum",16}}),16);
  QCOMPARE(xm::channelCount({{"ChannelNum","0x08"}}),8);
  QCOMPARE(xm::channelCount({},{{"VideoInChannel",0},{"DigChannel",32}}),32);
  QCOMPARE(xm::channelCount({{"ChannelNum",8}},{{"VideoInChannel",16}}),8);
  QCOMPARE(xm::channelCount({}),0);
  QVERIFY_EXCEPTION_THROWN(xm::channelCount({{"ChannelNum",257}}),std::runtime_error);
  QVERIFY_EXCEPTION_THROWN(xm::channelCount({{"ChannelNum",-1}}),std::runtime_error);
  QVERIFY_EXCEPTION_THROWN(xm::channelCount({{"ChannelNum",1.5}}),std::runtime_error);
  QVERIFY_EXCEPTION_THROWN(xm::channelCount({{"ChannelNum","garbage"}}),std::runtime_error);
 }
 void recorderUrls(){
  QUrl source("dvrip://admin:p%40ss@127.0.0.1:34568");
  auto urls=xm::channelUrls(source,16,true);QCOMPARE(urls.size(),16);
  for(int i=0;i<16;++i){QCOMPARE(urls[i].password(),QString("p@ss"));QCOMPARE(urls[i].port(),34568);QCOMPARE(QUrlQuery(urls[i]).queryItemValue("channel"),QString::number(i));QCOMPARE(QUrlQuery(urls[i]).queryItemValue("subtype"),QString("1"));}
  QCOMPARE(QUrlQuery(xm::channelUrls(source,1,false)[0]).queryItemValue("subtype"),QString("0"));
 }
 void hash(){QCOMPARE(xm::digest(""),QString("tlJwpbo6"));QCOMPARE(xm::digest("admin"),QString("6QNMIQGe"));}
 void headerVersions(){
  for(int version:{0,1}){auto h=xm::header(1,2,1001,12);h[1]=char(version);QCOMPARE(xm::payloadSize(h),12u);}
  auto h=xm::header(1,2,1001,12);h[1]=2;QVERIFY_EXCEPTION_THROWN(xm::payloadSize(h),std::runtime_error);
  h[1]=1;h[0]=0;QVERIFY_EXCEPTION_THROWN(xm::payloadSize(h),std::runtime_error);
  h[0]=char(255);qToLittleEndian<quint32>(xm::MaxPayload+1,h.data()+16);QVERIFY_EXCEPTION_THROWN(xm::payloadSize(h),std::runtime_error);
 }
 void framing(){auto h=xm::header(0x12345678,2,1000,12);QCOMPARE(h.toHex(),QByteArray("ff00000078563412020000000000e8030c000000"));QCOMPARE(xm::payloadSize(h),12u);}
 void reject(){QVERIFY_EXCEPTION_THROWN(xm::payloadSize(QByteArray(19,0)),std::runtime_error);QVERIFY_EXCEPTION_THROWN(xm::header(0,0,0,xm::MaxPayload+1),std::runtime_error);auto h=xm::header(0,0,0,0);qToLittleEndian<quint32>(0xffffffff,h.data()+16);QVERIFY_EXCEPTION_THROWN(xm::payloadSize(h),std::runtime_error);}
 void emptySearch_data(){QTest::addColumn<int>("loginCode");QTest::addColumn<int>("queryCode");QTest::addColumn<int>("count");QTest::newRow("no files")<<100<<119<<0;QTest::newRow("minimal search fallback")<<100<<1200<<1;QTest::newRow("hour windows")<<100<<1210<<1;QTest::newRow("permission denied")<<100<<107<<-1;QTest::newRow("119 during login is an error")<<119<<119<<-1;}
 void emptySearch(){
  QFETCH(int,loginCode);QFETCH(int,queryCode);QFETCH(int,count);
  QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));
  connect(&server,&QTcpServer::newConnection,this,[&]{auto *socket=server.nextPendingConnection();auto buffer=std::make_shared<QByteArray>();connect(socket,&QTcpSocket::readyRead,this,[&,socket,buffer]{*buffer+=socket->readAll();while(buffer->size()>=20){auto size=xm::payloadSize(buffer->left(20));if(buffer->size()<20+size)return;int command=qFromLittleEndian<quint16>(buffer->constData()+14);auto payload=buffer->mid(20,size);while(payload.endsWith('\0')||payload.endsWith('\n'))payload.chop(1);auto request=QJsonDocument::fromJson(payload).object();buffer->remove(0,20+size);
   QJsonObject reply{{"Ret",command==1000?loginCode:queryCode}};
   if(command==1440&&queryCode==1200){auto query=request["OPFileQuery"].toObject();QCOMPARE(query["Channel"].toInt(),1);QCOMPARE(query["BeginTime"].toString(),QString("2026-09-11 00:00:00"));
    if(query.contains("StreamType"))reply["Ret"]=119;
    else {QVERIFY(!query.contains("DriverTypeMask"));reply["Ret"]=100;reply["OPFileQuery"]=QJsonArray{QJsonObject{{"FileName","channel2.h264"}}};}}
   if(command==1440&&queryCode==1210){auto query=request["OPFileQuery"].toObject();QCOMPARE(query["Channel"].toInt(),1);reply["Ret"]=119;
    if(query["BeginTime"].toString()=="2026-09-11 12:00:00"&&query["EndTime"].toString()=="2026-09-11 13:00:00"){
     reply["Ret"]=100;reply["OPFileQuery"]=QJsonArray{QJsonObject{{"FileName","channel2-noon.h264"},{"BeginTime","2026-09-11 12:00:00"},{"EndTime","2026-09-11 13:00:00"}}};}}
   auto data=QJsonDocument(reply).toJson(QJsonDocument::Compact);socket->write(xm::header(123,0,command==1000?1001:1441,data.size())+data);}});});
  QUrl url;url.setScheme("dvrip");url.setHost("127.0.0.1");url.setPort(server.serverPort());int found=-1;QString error;std::atomic_bool cancel{false},done{false};
  auto *worker=QThread::create([&]{try{xm::Client c(cancel);found=c.recordings(url,1,"2026-09-11 00:00:00","2026-09-11 23:59:59").size();}catch(const std::exception&e){error=e.what();}done=true;});worker->start();
  QElapsedTimer timeout;timeout.start();while(!done&&timeout.elapsed()<8000)QTest::qWait(10);cancel=true;worker->wait();delete worker;QVERIFY(done);QCOMPARE(found,count);QCOMPARE(error.isEmpty(),count>=0);
 }
 void timestamps(){
  xm::MediaParser p;auto frame=QByteArray::fromHex("000001fc020a80000000000001000000")+"x";
  quint32 packed=(26u<<26)|(9u<<22)|(11u<<17)|(12u<<12)|(22u<<6)|30u;qToLittleEndian(packed,frame.data()+8);
  p.append(frame);p.take();auto base=QDateTime(QDate(2026,9,11),QTime(12,22,30),Qt::UTC).toMSecsSinceEpoch();QCOMPARE(p.timestamp,base);
  p.append(QByteArray::fromHex("000001fd01000000")+"y");p.take();QCOMPARE(p.timestamp,base+100);
  qToLittleEndian<quint32>(0,frame.data()+8);p.append(frame);p.take();QCOMPARE(p.timestamp,qint64(-1));
 }
 void fragmentedMedia(){QByteArray frame=QByteArray::fromHex("000001fc021980000000000005000000")+"hello";for(int split=0;split<frame.size();++split){xm::MediaParser p;p.append(frame.left(split));QVERIFY(p.take().isEmpty());p.append(frame.mid(split));QCOMPARE(p.take(),QByteArray("hello"));QCOMPARE(p.codec,2);QCOMPARE(p.fps,25);QVERIFY(p.take().isEmpty());}}
 void coalescedMedia(){xm::MediaParser p;auto frame=QByteArray::fromHex("000001fd03000000")+"abc";p.append(frame+frame);QCOMPARE(p.take(),QByteArray("abc"));QCOMPARE(p.take(),QByteArray("abc"));}
 void rejectMedia(){xm::MediaParser p;p.append(QByteArray::fromHex("000001fdffffffff"));QVERIFY_EXCEPTION_THROWN(p.take(),std::runtime_error);}
 void skipAudio(){xm::MediaParser p;p.append(QByteArray::fromHex("000001fa0e0202001122000001fd0100000033"));QCOMPARE(p.take(),QByteArray::fromHex("33"));}
};
QTEST_GUILESS_MAIN(ProtocolTest)
#include "protocol_test.moc"
