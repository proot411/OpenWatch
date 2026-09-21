#include <QtTest>
#include "devices.h"
class DevicesTest:public QObject {
 Q_OBJECT
private slots:
 void renamePreservesConnection(){
  QString name="Old";QUrl source("rtsp://u%40ser:p%26ss@127.0.0.1/live?channel=2");auto original=source;bool sanitized=false;
  QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;auto *url=dialog->findChild<QLineEdit*>("deviceUrl");sanitized=QUrl(url->text()).userInfo().isEmpty();dialog->findChild<QLineEdit*>("deviceName")->setText("Renamed");dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();});
  QVERIFY(devices::edit(nullptr,name,source));QVERIFY(sanitized);QCOMPARE(name,QString("Renamed"));QCOMPARE(source,original);
 }
 void cancelledEdit(){QString name="Keep";QUrl source("rtsp://127.0.0.1/live");auto original=source;
  QTimer::singleShot(0,[]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(dialog)dialog->reject();});
  QVERIFY(!devices::edit(nullptr,name,source));QCOMPARE(name,QString("Keep"));QCOMPARE(source,original);
 }
 void invalidEdit(){QString name="Keep";QUrl source("rtsp://127.0.0.1/live");auto original=source;bool stayedOpen=false;
  QTimer::singleShot(0,[&]{auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;dialog->findChild<QLineEdit*>("deviceUrl")->setText("not a stream");dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();stayedOpen=dialog->isVisible();dialog->reject();});
  QVERIFY(!devices::edit(nullptr,name,source));QVERIFY(stayedOpen);QCOMPARE(source,original);
 }
 void removalKeepsIdentity(){
  QListWidget list;list.addItems({"ONVIF camera","Recorder CH 1","Other camera"});QVector<QUrl> urls{QUrl("rtsp://a/live"),QUrl("dvrip://b?channel=0"),QUrl("rtsp://c/live")};
  Cell one,duplicate,other;QVector<Cell*> cells{&one,&duplicate,&other};auto removed=devices::id(list.item(0));auto kept=devices::id(list.item(1));
  one.deviceId=duplicate.deviceId=removed;one.name=duplicate.name="ONVIF camera";one.zoom=3;other.deviceId=kept;other.name="Recorder CH 1";
  devices::remove(&list,urls,cells,0);QCOMPARE(list.count(),2);QCOMPARE(urls.size(),2);QCOMPARE(urls[0].host(),QString("b"));
  QVERIFY(one.deviceId.isNull());QVERIFY(duplicate.deviceId.isNull());QVERIFY(one.name.isEmpty());QCOMPARE(one.zoom,1.0);QCOMPARE(one.video.status(),QString("Ready"));QCOMPARE(other.deviceId,kept);QCOMPARE(other.name,QString("Recorder CH 1"));
  QCOMPARE(devices::rowForId(&list,kept),0);QCOMPARE(devices::rowForId(&list,removed),-1);
  devices::remove(&list,urls,cells,1);QCOMPARE(list.count(),1);devices::remove(&list,urls,cells,0);QVERIFY(other.name.isEmpty());QVERIFY(urls.isEmpty());
  devices::remove(&list,urls,cells,-1);devices::remove(&list,urls,cells,0);
 }
};
QTEST_MAIN(DevicesTest)
#include "devices_test.moc"
