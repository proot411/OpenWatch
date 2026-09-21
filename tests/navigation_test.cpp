#include <QtTest>
#include "navigation.h"
class NavigationTest:public QObject {
 Q_OBJECT
private slots:
 void focusAndNavigate(){
  QWidget window;auto *grid=new QGridLayout(&window);QVector<Cell*> cells;Cell *selected=nullptr;
  auto choose=[&](Cell *c){for(auto *cell:cells)cell->selected=cell==c;selected=c;};
  for(int i=0;i<9;++i){auto *c=new Cell;c->select=choose;cells.append(c);}
  cells[0]->name="CH 1";cells[2]->name="CH 3";cells[3]->name="CH 4";
  Navigation navigation(grid,cells,choose);navigation.setGridSize(4);window.show();
  QTest::mouseDClick(cells[2],Qt::LeftButton);QVERIFY(navigation.isFocused());QCOMPARE(selected,cells[2]);QVERIFY(cells[2]->isVisible());QVERIFY(!cells[0]->isVisible());
  navigation.step(1);QCOMPARE(selected,cells[3]);QVERIFY(cells[3]->isVisible());
  navigation.step(1);QCOMPARE(selected,cells[0]);
  navigation.step(-1);QCOMPARE(selected,cells[3]);
  QTest::mouseDClick(cells[3],Qt::LeftButton);QVERIFY(!navigation.isFocused());for(int i=0;i<4;++i)QVERIFY(cells[i]->isVisible());
  navigation.step(-1);QCOMPARE(selected,cells[2]);QVERIFY(!navigation.isFocused());
  navigation.toggle(cells[2]);navigation.restore();QVERIFY(!navigation.isFocused());
  navigation.toggle(cells[2]);navigation.setGridSize(1);QVERIFY(!navigation.isFocused());QCOMPARE(selected,cells[0]);
  for(int i=4;i<9;++i)delete cells[i];
 }
};
QTEST_MAIN(NavigationTest)
#include "navigation_test.moc"
