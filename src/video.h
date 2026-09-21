#pragma once
#include <QtCore>
#include <QtGui>
#include <atomic>
#include <thread>
#include "audio.h"
class Video {
 AudioOutput speakers;
 std::thread thread;
 std::atomic_bool cancelled{false};
 std::atomic<qint64> playbackPosition{0};
 QMutex mutex;
 QImage image;
 qint64 shownTimestamp=-1;
 QString state="Ready", recordPath;
 struct Result {bool retry=false;QString error;};
 Result runOnce(QUrl url, bool resumed);
 void run(QUrl url);
public:
 ~Video(){ stop(); }
 void start(const QUrl &url){ stop(); {QMutexLocker lock(&mutex);image=QImage();shownTimestamp=-1;state="Connecting…";} playbackPosition=0;cancelled=false; thread=std::thread([this,url]{run(url);}); }
 void clear(){stop();QMutexLocker lock(&mutex);recordPath.clear();image=QImage();state="Ready";}
 void requestStop(){cancelled=true;speakers.clear();}
 void stop(){ cancelled=true;speakers.clear(); if(thread.joinable()) thread.join();speakers.clear(); }
 void audio(bool enabled){speakers.enable(enabled);}
 void volume(int value){speakers.setVolume(value);}
 QString audioStatus(){return speakers.status();}
 qint64 playbackMilliseconds()const{return playbackPosition.load();}
 quint64 audioBytes()const{return speakers.bytes();}
 QPair<QImage,qint64> archiveFrame(){QMutexLocker lock(&mutex);return {image,shownTimestamp};}
 QImage frame(){QMutexLocker lock(&mutex);return image;}
 QString status(){QMutexLocker lock(&mutex);return state;}
 void record(const QString &path){QMutexLocker lock(&mutex);recordPath=path;}
};
