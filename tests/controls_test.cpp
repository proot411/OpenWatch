#include <QtTest>
#include "controls.h"
#include "dvrip.h"
#include "registry.h"
#include "ptzkeys.h"
class ControlsTest:public QObject {
 Q_OBJECT
 QPushButton *button(QWidget &widget,QString text){for(auto *b:widget.findChildren<QPushButton*>())if(b->text()==text)return b;return nullptr;}
private slots:
 void keyboardBindings(){QWidget window;QString last;installPtzKeys(&window,[&](QString action){last=action;});window.show();window.activateWindow();QTest::qWait(100);QTest::keyClick(&window,Qt::Key_Right,Qt::AltModifier);QCOMPARE(last,QString("right"));last.clear();QTest::keyClick(&window,Qt::Key_Right);QVERIFY(last.isEmpty());QTest::keyClick(&window,Qt::Key_PageUp,Qt::AltModifier);QCOMPARE(last,QString("zoomIn"));QTest::keyClick(&window,Qt::Key_End,Qt::AltModifier);QCOMPARE(last,QString("stop"));}

 void vaultIntegrity(){
  QByteArray secret=R"({"password":"test-secret","name":"front"})";auto bytes=vault::seal(secret,"a long test passphrase");QVERIFY(!bytes.contains("test-secret"));QCOMPARE(vault::open(bytes,"a long test passphrase"),secret);QVERIFY(bytes!=vault::seal(secret,"a long test passphrase"));
  QVERIFY_EXCEPTION_THROWN(vault::open(bytes,"wrong"),std::runtime_error);
  for(int index:{0,4,20,32,int(bytes.size()-1)}){auto damaged=bytes;damaged[index]=char(damaged[index]^1);QVERIFY_EXCEPTION_THROWN(vault::open(damaged,"a long test passphrase"),std::runtime_error);}
  QVERIFY_EXCEPTION_THROWN(vault::open(bytes.left(40),"a long test passphrase"),std::runtime_error);
 }
 void registryValidation(){QJsonObject entry{{"id",QUuid::createUuid().toString()},{"name","Front"},{"url","rtsp://admin:test@192.0.2.1/live"},{"group","Home"},{"onvif","http://192.0.2.1:8080/device"}};QJsonArray rows{entry};auto data=QJsonDocument(rows).toJson();QCOMPARE(registry::validate(vault::open(vault::seal(data,"long test password"),"long test password")),rows);rows.append(entry);QVERIFY_EXCEPTION_THROWN(registry::validate(QJsonDocument(rows).toJson()),std::runtime_error);entry["url"]="file:///tmp/private";QVERIFY_EXCEPTION_THROWN(registry::validate(QJsonDocument(QJsonArray{entry}).toJson()),std::runtime_error);entry["url"]="rtsp://192.0.2.2/live";QVERIFY_EXCEPTION_THROWN(registry::validate(QJsonDocument(QJsonArray{entry}).toJson()),std::runtime_error);}
 void layoutEnvelope(){
  // Legacy bare-array files still load and report no layout.
  QJsonObject entry{{"id",QUuid::createUuid().toString()},{"name","Front"},{"url","rtsp://192.0.2.1/live"},{"group",QString()},{"onvif",QString()}};
  auto legacy=registry::unpack(QJsonDocument(QJsonArray{entry}).toJson());
  QCOMPARE(legacy.devices.size(),1);QCOMPARE(legacy.grid,0);QVERIFY(legacy.cells.isEmpty());
  // Envelope round-trip keeps grid size and cell assignments.
  QListWidget list;QVector<QUrl> urls{QUrl("rtsp://192.0.2.1/live"),QUrl("rtsp://192.0.2.2/live")};
  auto *first=new QListWidgetItem("Front");first->setData(Qt::UserRole,QUuid(entry["id"].toString()));list.addItem(first);
  auto *second=new QListWidgetItem("Back");second->setData(Qt::UserRole,QUuid::createUuid());list.addItem(second);
  QUuid front(first->data(Qt::UserRole).toUuid());
  auto packed=registry::pack(&list,urls,4,{front,QUuid(),QUuid(),QUuid()});
  auto snapshot=registry::unpack(QJsonDocument(packed).toJson(QJsonDocument::Compact));
  QCOMPARE(snapshot.devices.size(),2);QCOMPARE(snapshot.grid,4);QCOMPARE(snapshot.cells.size(),4);QCOMPARE(snapshot.cells[0],front);QVERIFY(snapshot.cells[1].isNull());
  // Wrong passphrase and damaged payloads never yield a partial snapshot.
  auto sealed=vault::seal(QJsonDocument(packed).toJson(QJsonDocument::Compact),"a long test passphrase");
  QVERIFY_EXCEPTION_THROWN(registry::unpack(vault::open(sealed,"wrong passphrase")),std::runtime_error);
  auto damaged=sealed;damaged[damaged.size()/2]^=0x40;QVERIFY_EXCEPTION_THROWN(registry::unpack(vault::open(damaged,"a long test passphrase")),std::runtime_error);
  // A cell pointing at a dropped device discards the layout instead of reconnecting blind.
  auto dangling=QJsonDocument(packed).object();auto cells=dangling["layout"].toObject()["cells"].toArray();cells[0]=QUuid::createUuid().toString();auto layout=dangling["layout"].toObject();layout["cells"]=cells;dangling["layout"]=layout;
  auto dropped=registry::unpack(QJsonDocument(dangling).toJson(QJsonDocument::Compact));
  QCOMPARE(dropped.devices.size(),2);QCOMPARE(dropped.grid,0);QVERIFY(dropped.cells.isEmpty());
  // Unknown grid sizes are ignored, not trusted.
  auto badGrid=QJsonDocument(packed).object();auto badLayout=badGrid["layout"].toObject();badLayout["grid"]=7;badGrid["layout"]=badLayout;
  QCOMPARE(registry::unpack(QJsonDocument(badGrid).toJson(QJsonDocument::Compact)).grid,0);
 }
 void xmMovement_data(){QTest::addColumn<bool>("rejectMove");QTest::newRow("success and stop")<<false;QTest::newRow("rejection still stops")<<true;}
 void xmMovement(){
  QFETCH(bool,rejectMove);QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QVector<QJsonObject> commands;
  connect(&server,&QTcpServer::newConnection,this,[&]{auto *socket=server.nextPendingConnection();auto buffer=std::make_shared<QByteArray>();connect(socket,&QTcpSocket::readyRead,this,[&,socket,buffer]{*buffer+=socket->readAll();while(buffer->size()>=20){auto size=xm::payloadSize(buffer->left(20));if(buffer->size()<20+size)return;auto payload=buffer->mid(20,size);buffer->remove(0,20+size);while(payload.endsWith('\0')||payload.endsWith('\n'))payload.chop(1);auto request=QJsonDocument::fromJson(payload).object();int ret=100;if(request.contains("OPPTZControl")){commands.append(request["OPPTZControl"].toObject());QCOMPARE(request["SessionID"].toString(),QString("0x0000007b"));if(rejectMove&&commands.size()==1)ret=103;}auto data=QJsonDocument(QJsonObject{{"Ret",ret}}).toJson(QJsonDocument::Compact);socket->write(xm::header(123,0,1001,data.size())+data);}});});
  CameraControls dialog("Test",QUrl(QString("dvrip://admin:test@127.0.0.1:%1?channel=3").arg(server.serverPort())),{});dialog.show();button(dialog,"↑")->click();QTRY_VERIFY_WITH_TIMEOUT(dialog.isEnabled(),5000);QCOMPARE(commands.size(),2);QCOMPARE(commands[0]["Command"].toString(),QString("DirectionUp"));QCOMPARE(commands[0]["Parameter"].toObject()["Channel"].toInt(),3);QCOMPARE(commands[0]["Parameter"].toObject()["Preset"].toInt(),65535);QCOMPARE(commands[1]["Parameter"].toObject()["Preset"].toInt(),-1);
 }
 void onvifControls_data(){QTest::addColumn<bool>("rejectMove");QTest::addColumn<bool>("missing");QTest::addColumn<bool>("legacy");QTest::newRow("success")<<false<<false<<false;QTest::newRow("rejection still stops")<<true<<false<<false;QTest::newRow("missing profile config")<<false<<true<<false;QTest::newRow("incomplete firmware compatibility")<<false<<true<<true;}
 void onvifControls(){
  QFETCH(bool,rejectMove);QFETCH(bool,missing);QFETCH(bool,legacy);
  QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QString base=QString("http://127.0.0.1:%1").arg(server.serverPort());QStringList requests;bool validMove=false,validStop=false;
  connect(&server,&QTcpServer::newConnection,this,[&]{auto *socket=server.nextPendingConnection();auto buffer=std::make_shared<QByteArray>();connect(socket,&QTcpSocket::readyRead,this,[&,socket,buffer]{*buffer+=socket->readAll();int end=buffer->indexOf("\r\n\r\n");if(end<0)return;auto headers=buffer->left(end);QRegularExpression length("content-length: (\\d+)",QRegularExpression::CaseInsensitiveOption);int size=length.match(QString::fromLatin1(headers)).captured(1).toInt();if(buffer->size()<end+4+size)return;auto doc=lan::xmlDocument(buffer->mid(end+4,size));QString op;for(auto name:{"GetSystemDateAndTime","GetServices","GetProfiles","GetCapabilities","GetConfigurationOptions","GetCompatibleConfigurations","GetPresets","ContinuousMove","Stop"})if(!lan::elements(doc,name).isEmpty())op=name;requests.append(op);QString response;
   if(op=="GetServices")response="<Service><Namespace>http://www.onvif.org/ver10/media/wsdl</Namespace><XAddr>"+base+"/media</XAddr></Service>";
   if(op=="GetServices"&&missing)response+="<Service><Namespace>http://www.onvif.org/ver20/ptz/wsdl</Namespace><XAddr>"+base+"/ptz</XAddr></Service>";
   if(op=="GetProfiles")response="<Profiles token=\"p&amp;1\"><Name>Camera</Name><PTZConfiguration token=\"config\"/></Profiles>";
   if(op=="GetProfiles"&&missing)response="<Profiles token=\"p&amp;1\"><Name>Camera</Name></Profiles>";
   if(op=="GetCompatibleConfigurations")response="<PTZConfiguration token=\"config\"/>";
   if(op=="GetCapabilities")response="<PTZ><XAddr>"+base+"/ptz</XAddr></PTZ>";
   if(op=="GetCapabilities"&&missing)response.clear();
   if(op=="GetConfigurationOptions")response="<ContinuousPanTiltVelocitySpace><URI>http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace</URI><XRange><Min>-1</Min><Max>1</Max></XRange><YRange><Min>-1</Min><Max>1</Max></YRange></ContinuousPanTiltVelocitySpace>";
   if(op=="GetConfigurationOptions"&&missing)response+="<PTZTimeout><Min>PT1S</Min><Max>PT100S</Max></PTZTimeout>";
   if(op=="GetPresets")response="<Preset token=\"one\"><Name>Door</Name></Preset>";
   if(op=="ContinuousMove")validMove=lan::value(doc,"Timeout")==QString(missing?"PT1S":"PT0.2S")&&lan::value(doc,"ProfileToken")=="p&1"&&lan::elements(doc,"PanTilt").first().attribute("y")=="0.2"&&lan::elements(doc,"PanTilt").first().hasAttribute("space")==!legacy;
   if(op=="Stop")validStop=lan::value(doc,"PanTilt")=="true"&&lan::value(doc,"Zoom")=="true";
   auto bytes=("<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\"><s:Body><"+op+"Response>"+response+"</"+op+"Response></s:Body></s:Envelope>").toUtf8();socket->write(QByteArray(op=="ContinuousMove"&&rejectMove?"HTTP/1.1 500 Error\r\nContent-Length: ":"HTTP/1.1 200 OK\r\nContent-Length: ")+QByteArray::number(bytes.size())+"\r\nConnection: close\r\n\r\n"+bytes);socket->disconnectFromHost();});});
  CameraControls dialog("ONVIF",QUrl("rtsp://admin:test@127.0.0.1/live"),base+"/device");dialog.show();dialog.keyboard("up");QVERIFY(requests.isEmpty());if(legacy)dialog.findChild<QCheckBox*>()->setChecked(true);QVERIFY(!button(dialog,"↑")->isEnabled());button(dialog,"Load ONVIF controls")->click();QTRY_VERIFY_WITH_TIMEOUT(requests.contains("GetPresets")&&dialog.isEnabled(),5000);QVERIFY(button(dialog,"↑")->isEnabled());QCOMPARE(button(dialog,"Zoom +")->isEnabled(),legacy);dialog.hide();dialog.keyboard("up");QTRY_VERIFY_WITH_TIMEOUT(requests.contains("Stop")&&dialog.isEnabled(),5000);QVERIFY(validMove);QVERIFY(validStop);if(missing&&!legacy)QVERIFY(requests.contains("GetCompatibleConfigurations"));
 }
};
QTEST_MAIN(ControlsTest)
#include "controls_test.moc"
