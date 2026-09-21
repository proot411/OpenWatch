#include <QtTest>
#include "recorderdialog.h"
class RecorderTest:public QObject {
 Q_OBJECT
private slots:
 void scan_data(){QTest::addColumn<bool>("fallback");QTest::addColumn<int>("version");QTest::newRow("v0 login channels")<<false<<0;QTest::newRow("v0 system info fallback")<<true<<0;QTest::newRow("v1 login channels")<<false<<1;QTest::newRow("v1 system info fallback")<<true<<1;}
 void scan(){
  QFETCH(bool,fallback);QFETCH(int,version);QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));int requests=0;
  connect(&server,&QTcpServer::newConnection,this,[&]{auto *socket=server.nextPendingConnection();auto buffer=std::make_shared<QByteArray>();connect(socket,&QTcpSocket::readyRead,this,[&,socket,buffer]{
   *buffer+=socket->readAll();
   while(buffer->size()>=20){auto size=xm::payloadSize(buffer->left(20));if(buffer->size()<20+size)return;auto body=buffer->mid(20,size);buffer->remove(0,20+size);while(body.endsWith('\0')||body.endsWith('\n'))body.chop(1);auto request=QJsonDocument::fromJson(body).object();++requests;
    QJsonObject reply{{"Ret",100}};
    if(request.contains("UserName")){QCOMPARE(request.value("PassWord").toString(),xm::digest("p@ss"));if(!fallback)reply.insert("ChannelNum",4);}
    else {QCOMPARE(request.value("Name").toString(),QString("SystemInfo"));reply.insert("SystemInfo",QJsonObject{{"VideoInChannel",0},{"DigChannel",4}});}
    auto data=QJsonDocument(reply).toJson(QJsonDocument::Compact)+QByteArray("\n\0",2);auto h=xm::header(123,0,requests==1?1001:1021,data.size());h[1]=char(version);socket->write(h.left(3));socket->write(h.mid(3)+data);
   }
  });});
  RecorderDialog dialog;dialog.findChild<QLineEdit*>("recorderIp")->setText("127.0.0.1");dialog.findChild<QSpinBox*>("recorderPort")->setValue(server.serverPort());dialog.findChild<QLineEdit*>("recorderPassword")->setText("p@ss");
  QSignalSpy accepted(&dialog,&QDialog::accepted);dialog.show();dialog.findChild<QPushButton*>("scanRecorder")->click();
  QTRY_COMPARE_WITH_TIMEOUT(accepted.count(),1,5000);QCOMPARE(dialog.channels().size(),4);QCOMPARE(requests,fallback?2:1);
 }
 void rejectSerial(){RecorderDialog dialog;dialog.findChild<QLineEdit*>("recorderIp")->setText("1234567890abcdef");dialog.findChild<QPushButton*>("scanRecorder")->click();QVERIFY(dialog.findChild<QLabel*>("scanStatus")->text().contains("Serial-number"));QVERIFY(dialog.channels().isEmpty());}
 void cancelled(){QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));RecorderDialog dialog;dialog.findChild<QLineEdit*>("recorderIp")->setText("127.0.0.1");dialog.findChild<QSpinBox*>("recorderPort")->setValue(server.serverPort());dialog.findChild<QPushButton*>("scanRecorder")->click();QSignalSpy accepted(&dialog,&QDialog::accepted);dialog.reject();QTest::qWait(200);QCOMPARE(accepted.count(),0);}
};
QTEST_MAIN(RecorderTest)
#include "recorder_test.moc"
