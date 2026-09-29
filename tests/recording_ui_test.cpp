#include "recordingui.h"
#include "networkplayback.h"
#include <QtTest>
int main(int argc,char **argv){
 QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);QApplication app(argc,argv);if(argc!=2)return 2;
 QTemporaryDir temporary;if(!temporary.isValid())return 3;qputenv("XDG_DATA_HOME",temporary.path().toUtf8());app.setApplicationName("recording-ui-test");
 QUrl source("dvrip://demo:example@xmeye-cloud?cloudId=test-recorder&channel=0&speed=1");
 {NetworkPlaybackDialog playback(source);if(playback.findChild<QComboBox*>("networkPlaybackSpeed")||!playback.findChild<QWidget*>("networkDayTimeline"))return 20;
  auto begin=QDateTime(QDate(2026,9,29),QTime(12,0),Qt::UTC);
  for(double oldRate:{.25,.5,1.,2.,4.,8.}){QUrl old=source;QUrlQuery oldQuery(old);oldQuery.addQueryItem("speed",QString::number(oldRate));old.setQuery(oldQuery);auto url=NetworkPlaybackDialog::playbackUrl(old,2,begin,begin.addSecs(300));QUrlQuery query(url);
   if(query.allQueryItemValues("channel")!=QStringList{"2"}||query.allQueryItemValues("speed")!=QStringList{"1"}||query.queryItemValue("cloudId")!="test-recorder"||url.password()!="example")return 21;
  }
 }
 QWidget window;Cell cell;cell.resize(400,240);cell.show();RecordingUi ui(&window);
 auto wait=[&](auto condition){QElapsedTimer timer;timer.start();while(!condition()&&timer.elapsed()<5000)QTest::qWait(20);return condition();};
 cell.video.start(QUrl::fromLocalFile(argv[1]));if(!wait([&]{return !cell.video.frame().isNull();}))return 4;
 QString destination=temporary.path()+"/saved.mkv";
 for(int pass=0;pass<2;++pass){
  ui.toggle(&cell);if(!ui.pending(&cell)||!wait([&]{return cell.video.isRecording();}))return 5;
  auto color=cell.grab().toImage().pixelColor(1,1);if(color.red()<=color.green()*2)return 6;
  QTest::qWait(700);ui.toggle(&cell);if(!ui.stopping(&cell)||!wait([&]{return cell.video.recordingFinished();}))return 7;
  bool handled=false;QTimer dialogTimer;
  QObject::connect(&dialogTimer,&QTimer::timeout,[&]{if(auto *dialog=qobject_cast<QFileDialog*>(app.activeModalWidget())){if(handled)return;handled=true;if(pass==0){dialog->selectFile(destination);QMetaObject::invokeMethod(dialog,"accept",Qt::QueuedConnection);}else dialog->reject();}else if(auto *box=qobject_cast<QMessageBox*>(app.activeModalWidget()))box->accept();});
  dialogTimer.start(20);ui.poll();dialogTimer.stop();if(!handled||ui.pending(&cell))return 8;
  if(pass==0&&QFileInfo(destination).size()<1000)return 9;
 }
 int retained=0;QDirIterator it(RecordingUi::pendingRoot(),{"*.mkv"},QDir::Files,QDirIterator::Subdirectories);while(it.hasNext()){it.next();if(it.fileInfo().size()>1000)++retained;}
 cell.video.stop();return retained==1?0:10;
}
