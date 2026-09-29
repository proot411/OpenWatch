#include <QtCore>
#include "wfsdisk.h"
#include "video.h"
int main(int argc,char **argv){QCoreApplication app(argc,argv);if(argc!=2)return 2;std::atomic_bool stop{false};
 try{auto index=wfs::Index::read(argv[1],stop);qInfo()<<"Full index scan"<<index.count<<"slots,"<<index.clips.size()<<"recordings,"<<index.rejected<<"rejected";if(index.count!=476053||index.clips.size()!=746)return 3;
  auto damaged=index;quint32 primary=16746;auto start=int(primary*32);qToLittleEndian<quint32>(primary,damaged.entries.data()+start+8);
  try{damaged.clip(primary);return 7;}catch(const std::exception&){}
  damaged=index;damaged.entries[start+31]=char(6);try{damaged.clip(primary);return 8;}catch(const std::exception&){}
  for(int id:{16746,16747,16745,52341}){auto clip=index.clip(id);auto url=QUrl::fromLocalFile(argv[1]);QUrlQuery q;q.addQueryItem("wfsIdentity",index.identity);q.addQueryItem("wfsPrimary",QString::number(id));q.addQueryItem("seekMs",QString::number(clip.end-clip.begin-5000));q.addQueryItem("speed","8");url.setQuery(q);
   Video video;video.start(url);QElapsedTimer timer;timer.start();while(timer.elapsed()<15000&&video.status()!="Playback complete")QThread::msleep(20);
   qInfo()<<id<<video.status()<<video.playbackMilliseconds()<<video.archiveFrame().second;
   if(video.frame().isNull()||video.status()!="Playback complete"||video.archiveFrame().second<0)return 4;video.stop();
  }
  stop=true;try{wfs::Index::read(argv[1],stop);return 5;}catch(const std::exception&){}
 }catch(const std::exception&e){qWarning()<<e.what();return 6;}return 0;}
