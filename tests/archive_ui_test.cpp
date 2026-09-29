#include <QtTest>
#include "archivetimeline.h"
#include "networkcoverage.h"
class ArchiveTest:public QObject {Q_OBJECT
private slots:
 void recorderRanges(){
  auto row=[](QString b,QString e,int ch){return QJsonObject{{"BeginTime",b},{"EndTime",e},{"Channel",ch}};};
  QJsonArray rows{row("2026-09-10 23:50:00","2026-09-11 00:10:00",1),row("2026-09-11 12:00:00","2026-09-11 13:00:00",1),row("2026-09-11 23:00:00","2026-09-12 01:00:00",1),row("2026-09-11 12:00:00","2026-09-11 13:00:00",1),row("2026-09-11 14:00:00","2026-09-11 15:00:00",0),row("bad","bad",1)};
  auto clips=archive::recordingRanges(rows,QDate(2026,9,11),1);QCOMPARE(clips.size(),3);QCOMPARE(clips[0].begin,0);QCOMPARE(clips[0].end,600);QCOMPARE(clips[1].begin,43200);QCOMPARE(clips[2].end,86400);QCOMPARE(archive::at(clips,1,3600),-1);
 }
 void rangeHighlights(){
  archive::Timeline t;t.resize(880,150);t.channels={1};
  t.clips=archive::recordingRanges(QJsonArray{QJsonObject{{"Channel",1},{"BeginTime","2026-09-11 12:00:00"},{"EndTime","2026-09-11 18:00:00"}}},QDate(2026,9,11),1);
  t.show();auto image=t.grab().toImage();
  QCOMPARE(image.pixelColor(550,80),QColor("#319e91"));
  QCOMPARE(image.pixelColor(350,80),QColor("#101b26"));
  t.channels={0};t.update();image=t.grab().toImage();
  QCOMPARE(image.pixelColor(550,80),QColor("#101b26"));
 }
 void selection(){QCOMPARE(archive::channels("1, 3,3,4"),QVector<int>({0,2,3}));QVERIFY(archive::channels("0,2").isEmpty());QVERIFY(archive::channels("1,2,3,4,5").isEmpty());QVERIFY(archive::channels("oops").isEmpty());}
 void gapsAndBoundaries(){QVector<archive::Clip> clips{{0,100,200,"a"},{0,200,300,"b"},{1,150,250,"c"}};QCOMPARE(archive::at(clips,0,99),-1);QCOMPARE(archive::at(clips,0,200),1);QCOMPARE(archive::at(clips,1,200),2);QCOMPARE(archive::at(clips,0,300),-1);}
 void dragCommitsOnce(){archive::Timeline t;t.resize(880,180);t.channels={0,1};t.clips={{0,0,43200,"a"}};int commits=0,value=-1;t.scrub=[&](int n,bool commit){value=n;if(commit)++commits;};t.show();QTest::mousePress(&t,Qt::LeftButton,Qt::NoModifier,QPoint(270,70));QCOMPARE(commits,0);QTest::mouseRelease(&t,Qt::LeftButton,Qt::NoModifier,QPoint(470,70));QCOMPARE(commits,1);QCOMPARE(value,43200);t.setPosition(999999);QCOMPARE(t.position(),86399);t.zoom(900);auto shot=t.grab();QVERIFY(!shot.isNull());}
};
QTEST_MAIN(ArchiveTest)
#include "archive_ui_test.moc"
