#pragma once
#include <QtCore>
#include <atomic>
struct AVCodecParameters;
struct AVPacket;
class AudioOutput {
 QMutex mutex;
 unsigned int device=0;
 bool active=false;
 int volume=75;
 QString message="Muted";
 std::atomic<quint64> delivered{0};
public:
 ~AudioOutput();
 void enable(bool enabled);
 bool enabled();
 void setVolume(int value);
 void clear();
 void error(const QString &text);
 QString status();
 void queue(QByteArray pcm);
 quint64 bytes()const{return delivered.load();}
};
class AudioDecoder {
 struct Impl;
 std::unique_ptr<Impl> impl;
public:
 explicit AudioDecoder(const AVCodecParameters *parameters);
 ~AudioDecoder();
 void consume(const AVPacket *packet,AudioOutput &output);
};
