#include <QtCore>
#include "video.h"
#include "dvrip.h"
#include <vector>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);if(argc!=3 && argc!=4)return 2;
 if(QString::fromLocal8Bit(argv[2])=="--archive-query"){
  std::atomic_bool cancel{false};xm::Client client(cancel);
  try{auto files=client.recordings(QUrl(QString::fromLocal8Bit(argv[1])),2,"2026-09-10 00:00:00","2026-09-10 23:59:59");return files.size()==1&&files[0].toObject()["FileName"].toString()=="/disk/003/test[R][0].h264"?0:11;}catch(const std::exception &e){qWarning()<<e.what();return 12;}
 }
 if(QString::fromLocal8Bit(argv[2])=="--archive-play"){
  Video video;video.start(QUrl(QString::fromLocal8Bit(argv[1])));QElapsedTimer timer;timer.start();
  qint64 firstStamp=-1;while(timer.elapsed()<12000&&video.status()!="Playback complete"){auto sample=video.archiveFrame();if(firstStamp<0&&!sample.first.isNull())firstStamp=sample.second;QThread::msleep(10);}
  bool ok=!video.frame().isNull()&&video.status()=="Playback complete";auto stamp=video.archiveFrame().second;auto dt=QDateTime::fromMSecsSinceEpoch(stamp,Qt::UTC);
  ok=ok&&QDateTime::fromMSecsSinceEpoch(firstStamp,Qt::UTC).time()==QTime(12,22,30,QDateTime::fromMSecsSinceEpoch(firstStamp,Qt::UTC).time().msec())&&dt.date()==QDate(2026,9,11)&&dt.time().hour()==12&&dt.time().minute()==22&&dt.time().second()==31;
  qInfo()<<video.status()<<dt;
  QThread::msleep(250);ok=ok&&video.status()=="Playback complete";video.stop();return ok?0:13;
 }
 if(argc==4 && QString::fromLocal8Bit(argv[2])=="--reconnect"){
  Video video;video.record(QString::fromLocal8Bit(argv[3]));video.start(QUrl(QString::fromLocal8Bit(argv[1])));
  QElapsedTimer clock;clock.start();bool first=false,lost=false,recovered=false;
  while(clock.elapsed()<20000){auto state=video.status();if(!video.frame().isNull()){if(!lost)first=true;else if(state.startsWith("Recording")){recovered=true;break;}}if(first && state.contains("retrying"))lost=true;QThread::msleep(20);}
  video.stop();auto stopped=video.status();QThread::msleep(1500);
  if(!first || !lost || !recovered || video.status()!=stopped || stopped!="Stopped"){qWarning()<<"Reconnect failed"<<first<<lost<<recovered<<video.status();return 6;}
  return 0;
 }
 if(QString::fromLocal8Bit(argv[2])=="--recorder"){
  std::atomic_bool cancel{false};QUrl source(QString::fromLocal8Bit(argv[1]));xm::Client client(cancel);
  try {int count=client.scanChannels(source);if(count!=4)return 3;
   auto urls=xm::channelUrls(source,count,true);std::vector<std::unique_ptr<Video>> videos;
   for(const auto &url:urls){auto video=std::make_unique<Video>();video->start(url);videos.push_back(std::move(video));}
   QElapsedTimer clock;clock.start();bool all=false;
   while(clock.elapsed()<10000){all=true;for(auto &v:videos)all=all && !v->frame().isNull();if(all)break;QThread::msleep(20);}
   for(auto &v:videos)v->requestStop();for(auto &v:videos)v->stop();return all?0:4;
  }catch(const std::exception &e){qWarning()<<e.what();return 5;}
 }
 if(argc==4&&QString::fromLocal8Bit(argv[3])=="--audio"){
  Video video;video.record(QString::fromLocal8Bit(argv[2]));video.audio(true);video.start(QUrl::fromUserInput(QString::fromLocal8Bit(argv[1])));QElapsedTimer timer;timer.start();
  while(timer.elapsed()<7000&&(video.audioBytes()<10000||video.frame().isNull()))QThread::msleep(20);
  if(video.audioBytes()<10000||video.frame().isNull()){qWarning()<<video.audioStatus();return 7;}
  video.audio(false);auto muted=video.audioBytes();QThread::msleep(300);if(video.audioBytes()!=muted)return 8;
  video.audio(true);timer.restart();while(timer.elapsed()<3000&&video.audioBytes()==muted)QThread::msleep(20);
  if(video.audioBytes()==muted)return 9;video.stop();auto stopped=video.audioBytes();QThread::msleep(100);return video.audioBytes()==stopped?0:10;
 }
 Video video;video.record(QString::fromLocal8Bit(argv[2]));video.start(QUrl::fromUserInput(QString::fromLocal8Bit(argv[1])));QElapsedTimer deadline;deadline.start();bool frame=false;while(deadline.elapsed()<4500){if(!video.frame().isNull())frame=true;QThread::msleep(20);}qInfo()<<video.status()<<"frame"<<frame<<"bytes"<<QFileInfo(QString::fromLocal8Bit(argv[2])).size();video.stop();if(!frame || QFileInfo(QString::fromLocal8Bit(argv[2])).size()<1000)return 1;return 0;
}
