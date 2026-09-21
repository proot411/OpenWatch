#include <QtTest>
#include "zoom.h"
#include "xmrecording.h"
class ZoomRecordingTest:public QObject {
 Q_OBJECT
private slots:
 void cursorAnchor(){QRectF view(0,0,800,600);QSize image(1600,1200);double z=1;QPointF pan,mouse(600,150);auto before=digitalzoom::imageRect(image,view,z,pan);QPointF uv((mouse.x()-before.x())/before.width(),(mouse.y()-before.y())/before.height());digitalzoom::at(image,view,mouse,3,z,pan);auto after=digitalzoom::imageRect(image,view,z,pan);QPointF mapped=after.topLeft()+QPointF(uv.x()*after.width(),uv.y()*after.height());QVERIFY(QLineF(mouse,mapped).length()<.001);QVERIFY(after.center()!=view.center());digitalzoom::at(image,view,mouse,-10,z,pan);QCOMPARE(z,1.0);QCOMPARE(digitalzoom::imageRect(image,view,z,pan),before);}
 void bordersAndBars(){QRectF view(0,0,800,600);QSize image(1600,900);double z=1;QPointF pan;digitalzoom::at(image,view,QPointF(100,10),1,z,pan);QCOMPARE(z,1.0);digitalzoom::at(image,view,QPointF(790,400),50,z,pan);QCOMPARE(z,5.0);auto rect=digitalzoom::imageRect(image,view,z,pan);QVERIFY(rect.contains(view));auto smaller=digitalzoom::imageRect(image,QRectF(0,0,400,300),z,pan);QVERIFY(smaller.contains(QRectF(0,0,400,300)));}
 void recordingFrames(){QTemporaryFile file;QVERIFY(file.open());QByteArray payload=QByteArray::fromHex("000000016764001f");auto header=QByteArray::fromHex("000001fc021900000000000008000000");file.write(header+payload);file.flush();std::atomic_bool cancel{false};xm::Recording input(cancel);input.open(file.fileName());QCOMPARE(input.codec,2);QCOMPARE(input.fps,25);char buffer[20];QCOMPARE(xm::Recording::read(&input,reinterpret_cast<uint8_t*>(buffer),20),8);QCOMPARE(QByteArray(buffer,8),payload);QCOMPARE(xm::Recording::read(&input,reinterpret_cast<uint8_t*>(buffer),20),AVERROR_EOF);}
 void malformedRecording(){QTemporaryFile file;QVERIFY(file.open());file.write(QByteArray::fromHex("000001fc0219000000000000ffffff7f"));file.flush();std::atomic_bool cancel{false};xm::Recording input(cancel);QVERIFY_EXCEPTION_THROWN(input.open(file.fileName()),std::runtime_error);}
 void truncatedRecording(){QTemporaryFile file;QVERIFY(file.open());file.write(QByteArray::fromHex("000001fc0219000000000000080000001234"));file.flush();std::atomic_bool cancel{false};xm::Recording input(cancel);QVERIFY_EXCEPTION_THROWN(input.open(file.fileName()),std::runtime_error);}
};
QTEST_GUILESS_MAIN(ZoomRecordingTest)
#include "zoom_recording_test.moc"
