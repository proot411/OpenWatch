#include <QtCore>
#include "video.h"
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);if(argc!=2)return 2;QFile manifest(argv[1]);if(!manifest.open(QIODevice::ReadOnly))return 3;
 auto clips=QJsonDocument::fromJson(manifest.readAll()).object()["clips"].toArray();if(clips.size()!=4)return 4;
 for(auto value:clips){auto clip=value.toObject();auto url=QUrl::fromLocalFile(QFileInfo(manifest).dir().filePath(clip["file"].toString()));
  qint64 target=clip["duration_ms"].toInt()-3000;QUrlQuery query;query.addQueryItem("seekMs",QString::number(target));query.addQueryItem("speed","8");url.setQuery(query);
  Video video;video.start(url);QElapsedTimer timer;timer.start();bool frame=false;while(timer.elapsed()<10000&&video.status()!="Playback complete"){
   if(!video.frame().isNull()){frame=true;if(video.playbackMilliseconds()<target){qWarning()<<"Preroll frame displayed";return 5;}}QThread::msleep(10);
  }
  frame=frame||!video.frame().isNull();qInfo()<<clip["file"].toString()<<video.status()<<video.playbackMilliseconds()<<"elapsed"<<timer.elapsed();
  if(!frame||video.status()!="Playback complete"||video.playbackMilliseconds()<target)return 6;
  video.stop();
 }
 return 0;
}
