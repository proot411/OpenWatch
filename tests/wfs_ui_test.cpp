#include <QtWidgets>
#include <QtTest>
#include "wfstestdialog.h"
int main(int argc,char **argv){
 QApplication app(argc,argv);if(argc!=2&&argc!=3)return 2;WfsTestDialog dialog;
 if(argc==3){dialog.loadDisk(argv[1]);dialog.show();auto *list=dialog.findChild<QListWidget*>();auto *calendar=dialog.findChild<QCalendarWidget*>();QElapsedTimer wait;wait.start();while(list->count()==0&&wait.elapsed()<10000)QTest::qWait(50);
  if(list->count()==0)return 20;
  for(int day=11;day<=21;++day)if(calendar->dateTextFormat(QDate(2026,9,day)).background().style()==Qt::NoBrush)return 21;
  calendar->setSelectedDate(QDate(2026,9,15));bool found=false;for(int n=0;n<list->count();++n)if(list->item(n)->text().startsWith("CH 1 · 01:00:02")){list->setCurrentRow(n);found=true;break;}
  if(!found)return 22;QTest::qWait(1500);Cell *cell=nullptr;for(auto *widget:dialog.findChildren<QWidget*>())if(auto *candidate=dynamic_cast<Cell*>(widget))cell=candidate;
  if(!cell||cell->video.frame().isNull())return 23;
  auto button=[&](QString text)->QPushButton*{for(auto *b:dialog.findChildren<QPushButton*>())if(b->text()==text)return b;return nullptr;};
  for(auto delta:{10000,-10000}){
   auto *pause=button("Pause");if(!pause)return 40;QTest::mouseClick(pause,Qt::LeftButton);qint64 before=cell->video.playbackMilliseconds();
   auto *skip=button(delta>0?"+10 s":"−10 s");if(!skip)return 41;QTest::mouseClick(skip,Qt::LeftButton);
   QElapsedTimer deadline;deadline.start();while(cell->video.frame().isNull()&&deadline.elapsed()<10000)QTest::qWait(10);
   qint64 actual=cell->video.playbackMilliseconds()-before;qInfo()<<"Skip delta"<<actual<<"expected"<<delta;
   if(cell->video.frame().isNull()||qAbs(actual-delta)>300)return 42;
  }
  auto *time=dialog.findChild<QTimeEdit*>();time->setTime(QTime(1,0,20));QTest::mouseClick(button("Go"),Qt::LeftButton);
  wait.restart();while(cell->video.frame().isNull()&&wait.elapsed()<10000)QTest::qWait(10);
  auto expected=QDateTime(QDate(2026,9,15),QTime(1,0,20),Qt::UTC).toMSecsSinceEpoch();
  if(cell->video.frame().isNull()||qAbs(cell->video.archiveFrame().second-expected)>300)return 43;
  qInfo()<<"Disk UI, relative skip and absolute media-time seek checks passed";return 0;
 }
 if(!dialog.load(argv[1]))return 3;dialog.show();QTest::qWait(1200);
 auto *list=dialog.findChild<QListWidget*>();auto *calendar=dialog.findChild<QCalendarWidget*>();auto *slider=dialog.findChild<QSlider*>();
 if(!list||!calendar||!slider||list->count()!=3)return 4;
 if(calendar->dateTextFormat(QDate(2026,9,15)).background().style()==Qt::NoBrush || calendar->dateTextFormat(QDate(2026,9,20)).background().style()==Qt::NoBrush || calendar->dateTextFormat(QDate(2026,9,16)).background().style()!=Qt::NoBrush)return 11;
 auto *filter=dialog.findChild<QComboBox*>("wfsChannels");if(!filter)return 12;filter->setCurrentIndex(2);if(list->count()!=1)return 13;filter->setCurrentIndex(0);if(list->count()!=3)return 14;
 QPushButton *play=nullptr;for(auto *button:dialog.findChildren<QPushButton*>())if(button->text()=="Pause")play=button;
 if(!play)return 5;QTest::mouseClick(play,Qt::LeftButton);if(play->text()!="Play")return 6;
 QTest::mouseClick(play,Qt::LeftButton);if(play->text()!="Pause")return 7;
 list->setCurrentRow(1);QTest::qWait(600);slider->setValue(30000);QMetaObject::invokeMethod(slider,"sliderReleased");QTest::qWait(800);
 if(slider->value()<30000)return 8;
 auto *dayline=dynamic_cast<archive::Timeline*>(dialog.findChild<QWidget*>("wfsDayTimeline"));if(!dayline||dayline->channels.size()!=3)return 30;
 dayline->selectChannel(1);dayline->scrub(3630,true);QTest::qWait(800);if(!list->currentItem()||!list->currentItem()->text().startsWith("CH 2"))return 31;
 dayline->scrub(43200,true);QTest::qWait(100);if(list->currentRow()!=-1||dayline->position()!=43200)return 32;
 calendar->setSelectedDate(QDate(2026,9,20));QTest::qWait(500);if(list->count()!=1||slider->maximum()<199000)return 9;
 calendar->setSelectedDate(QDate(2026,9,1));if(list->count()!=0||play->text()!="Play")return 10;
 qInfo()<<"Library load, pause/resume, channel switch, seek, calendar and empty-day checks passed";return 0;
}
