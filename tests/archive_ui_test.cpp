#include <QtTest>
#include "archivetimeline.h"
class ArchiveTest:public QObject {Q_OBJECT
private slots:
 void selection(){QCOMPARE(archive::channels("1, 3,3,4"),QVector<int>({0,2,3}));QVERIFY(archive::channels("0,2").isEmpty());QVERIFY(archive::channels("1,2,3,4,5").isEmpty());QVERIFY(archive::channels("oops").isEmpty());}
 void gapsAndBoundaries(){QVector<archive::Clip> clips{{0,100,200,"a"},{0,200,300,"b"},{1,150,250,"c"}};QCOMPARE(archive::at(clips,0,99),-1);QCOMPARE(archive::at(clips,0,200),1);QCOMPARE(archive::at(clips,1,200),2);QCOMPARE(archive::at(clips,0,300),-1);}
 void dragCommitsOnce(){archive::Timeline t;t.resize(880,180);t.channels={0,1};t.clips={{0,0,43200,"a"}};int commits=0,value=-1;t.scrub=[&](int n,bool commit){value=n;if(commit)++commits;};t.show();QTest::mousePress(&t,Qt::LeftButton,Qt::NoModifier,QPoint(270,70));QCOMPARE(commits,0);QTest::mouseRelease(&t,Qt::LeftButton,Qt::NoModifier,QPoint(470,70));QCOMPARE(commits,1);QCOMPARE(value,43200);t.setPosition(999999);QCOMPARE(t.position(),86399);t.zoom(900);auto shot=t.grab();QVERIFY(!shot.isNull());}
};
QTEST_MAIN(ArchiveTest)
#include "archive_ui_test.moc"
