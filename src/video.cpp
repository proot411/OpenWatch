#include "video.h"
#include "dvrip.h"
#include "xmrecording.h"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#include <chrono>
#include <map>
namespace {
struct Input {
 xm::Client client;
 QByteArray pending;
 QString error;
 bool archive=false; qint64 offset=0;std::map<qint64,qint64> times;
 void fetch(){pending=client.readVideo();if(archive&&!pending.isEmpty()){if(times.size()>32768)throw std::runtime_error("Archive timestamp buffer limit");times[offset]=client.timestamp();offset+=pending.size();}}
 qint64 timestamp(qint64 position){if(position<0)return -1;auto it=times.upper_bound(position);if(it==times.begin())return -1;--it;auto value=it->second;times.erase(times.begin(),it);return value;}
 explicit Input(std::atomic_bool &stop):client(stop){}
 static int read(void *opaque,uint8_t *out,int size) {
  auto *self=static_cast<Input*>(opaque);
  try { if(self->pending.isEmpty()) self->fetch();
   if(self->pending.isEmpty()) return AVERROR_EOF;
   int count=qMin(size,int(self->pending.size())); memcpy(out,self->pending.constData(),count); self->pending.remove(0,count); return count;
  } catch(const std::exception &e) {self->error=QString::fromUtf8(e.what());return AVERROR(EIO);}
 }
};
struct Deadline {
 std::atomic_bool &stop;
 std::atomic<qint64> expires{0};
 static qint64 now(){return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
 void reset(){expires=now()+10000;}
 static int interrupt(void *p){auto &d=*static_cast<Deadline*>(p);return d.stop || now()>d.expires;}
};
}
void Video::run(QUrl url) {
 int delay=1;bool resumed=false;
 while(!cancelled){
  QElapsedTimer connection;connection.start();auto result=runOnce(url,resumed);
  if(cancelled || !result.retry || (url.isLocalFile() || QUrlQuery(url).hasQueryItem("archiveFile")))break;
  if(connection.elapsed()>30000)delay=1;
  {QMutexLocker lock(&mutex);state=QString("Connection lost · retrying in %1s · %2").arg(delay).arg(result.error);image=QImage();}
  for(int waited=0;waited<delay*1000 && !cancelled;waited+=50)QThread::msleep(50);
  delay=qMin(delay*2,30);resumed=true;
 }
 if(cancelled){QMutexLocker lock(&mutex);state="Stopped";}
}
Video::Result Video::runOnce(QUrl url,bool resumed) {
 Result outcome;std::unique_ptr<AudioDecoder> sound;bool soundFailed=false;
 AVFormatContext *format=nullptr,*output=nullptr;
 AVCodecContext *decoder=nullptr; AVIOContext *io=nullptr;
 AVPacket *packet=av_packet_alloc(); AVFrame *decoded=av_frame_alloc(),*cpu=av_frame_alloc(); SwsContext *scale=nullptr;
 AVBufferRef *device=nullptr; std::unique_ptr<Input> input;std::unique_ptr<xm::Recording> recording;
 bool recordHeader=false; QString activeRecord; int64_t firstDts=AV_NOPTS_VALUE,lastDts=-1,synthetic=0; AVRational timebase{1,25};
 auto status=[&](QString message){QMutexLocker lock(&mutex);state=message;};
 auto closeRecord=[&](){if(output){if(recordHeader)av_write_trailer(output);avio_closep(&output->pb);avformat_free_context(output);output=nullptr;} recordHeader=false; activeRecord.clear();};
 Deadline deadline{cancelled};
 try {
  status("Connecting…"); format=avformat_alloc_context(); deadline.reset(); format->interrupt_callback={Deadline::interrupt,&deadline};
  const AVInputFormat *demux=nullptr;
  if(url.scheme()=="dvrip") {
   input=std::make_unique<Input>(cancelled); input->archive=QUrlQuery(url).hasQueryItem("archiveFile");input->client.open(url); input->fetch();
   int code=input->client.codec();
   if(code==2 || code==0x12) demux=av_find_input_format("h264");
   else if(code==3 || code==0x13 || code==0x43 || code==0x53) demux=av_find_input_format("hevc");
   else throw std::runtime_error("Unsupported DVRIP video codec");
   timebase={1,input->client.fps()};
   io=avio_alloc_context(static_cast<unsigned char*>(av_malloc(32768)),32768,0,input.get(),Input::read,nullptr,nullptr);
   if(!io) throw std::runtime_error("Cannot allocate video input");
   format->pb=io; format->flags|=AVFMT_FLAG_CUSTOM_IO;
  }
  if(url.isLocalFile()){
   QFile file(url.toLocalFile());if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot open recording file");
   if(xm::Recording::recognizes(file.peek(4))){recording=std::make_unique<xm::Recording>(cancelled);recording->open(url.toLocalFile());
    demux=av_find_input_format(recording->codec==2?"h264":"hevc");
    io=avio_alloc_context(static_cast<unsigned char*>(av_malloc(32768)),32768,0,recording.get(),xm::Recording::read,nullptr,nullptr);
    if(!io)throw std::runtime_error("Cannot allocate recording input");format->pb=io;format->flags|=AVFMT_FLAG_CUSTOM_IO;
   }
  }
  AVDictionary *options=nullptr; av_dict_set(&options,"rtsp_transport","tcp",0); av_dict_set(&options,"rw_timeout","5000000",0);
  av_dict_set(&options,"probesize","1048576",0); av_dict_set(&options,"analyzeduration","1500000",0);
  if(recording)av_dict_set(&options,"framerate",QByteArray::number(recording->fps).constData(),0);
  auto source=url.isLocalFile()?url.toLocalFile().toUtf8():url.toEncoded();
  int opened=avformat_open_input(&format,(input||recording)?nullptr:source.constData(),demux,&options); av_dict_free(&options);
  if(opened<0) {if(input&&!input->error.isEmpty())throw std::runtime_error(input->error.toStdString());if(recording&&!recording->error.isEmpty())throw std::runtime_error(recording->error.toStdString());if(url.isLocalFile())throw std::runtime_error("Unrecognized recording format. Encrypted or other XM export variants are not supported yet.");if(opened==AVERROR(EACCES) || opened==AVERROR_HTTP_UNAUTHORIZED || opened==AVERROR_HTTP_FORBIDDEN)throw std::runtime_error("Video source rejected authentication");throw std::runtime_error("Cannot open video source");}
  deadline.reset(); if(avformat_find_stream_info(format,nullptr)<0){if(recording&&!recording->error.isEmpty())throw std::runtime_error(recording->error.toStdString());throw std::runtime_error("Cannot read stream information");}
  int stream=av_find_best_stream(format,AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0);
  if(stream<0) throw std::runtime_error("No video stream found");
  int audioStream=av_find_best_stream(format,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);
  auto *track=format->streams[stream]; if(!input) timebase=track->time_base;
  const AVCodec *codec=avcodec_find_decoder(track->codecpar->codec_id);
  if(!codec) throw std::runtime_error("Decoder unavailable");
  auto makeDecoder=[&](){decoder=avcodec_alloc_context3(codec); if(!decoder) throw std::runtime_error("Decoder allocation failed"); if(avcodec_parameters_to_context(decoder,track->codecpar)<0) throw std::runtime_error("Invalid codec parameters"); decoder->thread_count=2;};
  makeDecoder(); QString backend="CPU";
  for(int i=0;;++i){const auto *config=avcodec_get_hw_config(codec,i);if(!config)break;
   if(!(config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX))continue;
   if(config->device_type!=AV_HWDEVICE_TYPE_VAAPI && config->device_type!=AV_HWDEVICE_TYPE_DXVA2 && config->device_type!=AV_HWDEVICE_TYPE_D3D11VA && config->device_type!=AV_HWDEVICE_TYPE_CUDA && config->device_type!=AV_HWDEVICE_TYPE_QSV)continue;
   if(av_hwdevice_ctx_create(&device,config->device_type,nullptr,nullptr,0)==0){decoder->hw_device_ctx=av_buffer_ref(device);backend=av_hwdevice_get_type_name(config->device_type);break;}
  }
  if(avcodec_open2(decoder,codec,nullptr)<0){avcodec_free_context(&decoder);av_buffer_unref(&device);makeDecoder();backend="CPU";if(avcodec_open2(decoder,codec,nullptr)<0)throw std::runtime_error("Cannot initialize decoder");}
  status(((url.isLocalFile()||QUrlQuery(url).hasQueryItem("archiveFile"))?"Playback · ":"Live · ")+backend);
  QElapsedTimer playbackClock;playbackClock.start();int64_t firstPts=AV_NOPTS_VALUE; qint64 archiveFrames=0;
  const bool archive=input&&input->archive;
  double rate=QUrlQuery(url).queryItemValue("speed").toDouble();if(rate<0.25||rate>8)rate=1;
  std::map<qint64,qint64> packetTimes;
  qint64 previousTime=-1;QElapsedTimer pace;pace.start();
  auto render=[&](){
   while(avcodec_receive_frame(decoder,decoded)==0){
    AVFrame *frame=decoded;
    qint64 frameTime=-1;
    if(archive){
     auto it=packetTimes.find(decoded->pts);if(it!=packetTimes.end()){frameTime=it->second;packetTimes.erase(it);}
     double interval=1000.0/qMax(1,input->client.fps());
     if(frameTime>=0&&previousTime>=0&&frameTime>previousTime&&frameTime-previousTime<2000)interval=frameTime-previousTime;
     if(archiveFrames){while(!cancelled&&pace.elapsed()<interval/rate){input->client.keepArchiveAlive();QThread::msleep(5);}}
     if(cancelled){av_frame_unref(decoded);return;}pace.restart();previousTime=frameTime;
     playbackPosition=archiveFrames++*1000/qMax(1,input->client.fps());
    }
    if(!decoded->hw_frames_ctx && backend!="CPU"){backend="CPU";status((output?"Recording · ":(QUrlQuery(url).hasQueryItem("archiveFile")||url.isLocalFile()?"Playback · ":"Live · "))+backend);}
    if(decoded->hw_frames_ctx){av_frame_unref(cpu);if(av_hwframe_transfer_data(cpu,decoded,0)<0)throw std::runtime_error("Hardware frame transfer failed");frame=cpu;}
    if(url.isLocalFile() && decoded->best_effort_timestamp!=AV_NOPTS_VALUE){
     if(firstPts==AV_NOPTS_VALUE)firstPts=decoded->best_effort_timestamp;
     auto due=int64_t((decoded->best_effort_timestamp-firstPts)*av_q2d(timebase)*1000);
     while(!cancelled && playbackClock.elapsed()<due)QThread::msleep(5);
    }
    if(frame->width<1 || frame->height<1 || frame->width>8192 || frame->height>8192)throw std::runtime_error("Unsupported frame dimensions");
    QImage next(frame->width,frame->height,QImage::Format_RGB888);
    scale=sws_getCachedContext(scale,frame->width,frame->height,AVPixelFormat(frame->format),frame->width,frame->height,AV_PIX_FMT_RGB24,SWS_BILINEAR,nullptr,nullptr,nullptr);
    if(!scale || next.isNull())throw std::runtime_error("Frame allocation failed");
    uint8_t *dest[]={next.bits()};int stride[]={int(next.bytesPerLine())};sws_scale(scale,frame->data,frame->linesize,0,frame->height,dest,stride);
    {QMutexLocker lock(&mutex);image=next;shownTimestamp=frameTime;}
    av_frame_unref(decoded);
   }
  };
  while(!cancelled){
   if(!speakers.enabled()){sound.reset();soundFailed=false;}
   else if(audioStream<0)speakers.error(input?"DVRIP audio: use this channel's RTSP stream":"This stream has no audio track");
   deadline.reset();int result=av_read_frame(format,packet);
   if(result<0){if(input&&!input->error.isEmpty())throw std::runtime_error(input->error.toStdString());if(recording&&!recording->error.isEmpty())throw std::runtime_error(recording->error.toStdString());if(result==AVERROR_EOF){avcodec_send_packet(decoder,nullptr);render();if(url.isLocalFile()||QUrlQuery(url).hasQueryItem("archiveFile"))status("Playback complete");else throw std::runtime_error("Stream disconnected or timed out");break;}throw std::runtime_error("Stream disconnected or timed out");}
   if(packet->stream_index==audioStream&&speakers.enabled()&&!soundFailed){
    try{if(!sound)sound=std::make_unique<AudioDecoder>(format->streams[audioStream]->codecpar);sound->consume(packet,speakers);}catch(const std::exception &e){speakers.error(e.what());soundFailed=true;}
   }
   if(packet->stream_index!=stream){av_packet_unref(packet);continue;}
   if(input){if(archive){if(packetTimes.size()>512)throw std::runtime_error("Archive decoder timestamp buffer limit");packetTimes[synthetic]=input->timestamp(packet->pos);}packet->pts=packet->dts=synthetic++;packet->duration=1;}
   if(recording){packet->pts=packet->dts=av_rescale_q(synthetic++,AVRational{1,recording->fps},timebase);packet->duration=av_rescale_q(1,AVRational{1,recording->fps},timebase);}
   QString requested;{QMutexLocker lock(&mutex);requested=recordPath;}
   if(output && requested!=activeRecord){closeRecord();status(((url.isLocalFile()||QUrlQuery(url).hasQueryItem("archiveFile"))?"Playback · ":"Live · ")+backend);}
   bool timestamped=packet->dts!=AV_NOPTS_VALUE || packet->pts!=AV_NOPTS_VALUE;
   if(!requested.isEmpty() && !output && timestamped && (packet->flags&AV_PKT_FLAG_KEY)){
    if(url.isLocalFile() && QFileInfo(requested).exists() && QFileInfo(requested).canonicalFilePath()==QFileInfo(url.toLocalFile()).canonicalFilePath())throw std::runtime_error("Recording output must not overwrite the source footage");
    if(avformat_alloc_output_context2(&output,nullptr,"matroska",nullptr)<0 || !output)throw std::runtime_error("Cannot create recording");
    auto *outTrack=avformat_new_stream(output,nullptr);if(!outTrack || avcodec_parameters_copy(outTrack->codecpar,track->codecpar)<0)throw std::runtime_error("Cannot create recording track");
    outTrack->codecpar->codec_tag=0;outTrack->time_base=timebase;
    QString target=requested;
    if(resumed){QFileInfo file(requested);target=file.dir().filePath(file.completeBaseName()+"-reconnect-"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+"-"+QUuid::createUuid().toString(QUuid::Id128).left(8)+".mkv");}
    auto path=target.toUtf8();if(avio_open(&output->pb,path.constData(),AVIO_FLAG_WRITE)<0)throw std::runtime_error("Cannot write recording file");
    if(avformat_write_header(output,nullptr)<0)throw std::runtime_error("Cannot write recording header");
    recordHeader=true;activeRecord=requested;firstDts=AV_NOPTS_VALUE;lastDts=-1;status("Recording · "+backend);
   }
   if(output && timestamped){
    AVPacket *copy=av_packet_clone(packet);if(!copy)throw std::runtime_error("Packet allocation failed");
    if(copy->dts==AV_NOPTS_VALUE)copy->dts=copy->pts;
    if(copy->dts==AV_NOPTS_VALUE){av_packet_free(&copy);throw std::runtime_error("Source has no recording timestamps");}
    if(firstDts==AV_NOPTS_VALUE)firstDts=copy->dts;
    copy->dts-=firstDts;if(copy->pts!=AV_NOPTS_VALUE)copy->pts-=firstDts;
    copy->stream_index=0;copy->pos=-1;av_packet_rescale_ts(copy,timebase,output->streams[0]->time_base);
    if(copy->dts<=lastDts){av_packet_free(&copy);throw std::runtime_error("Non-monotonic recording timestamps");}lastDts=copy->dts;
    int written=av_interleaved_write_frame(output,copy);av_packet_free(&copy);if(written<0)throw std::runtime_error("Recording write failed");
   }
   int sent=avcodec_send_packet(decoder,packet);if(sent==AVERROR(EAGAIN)){render();sent=avcodec_send_packet(decoder,packet);}if(sent<0)throw std::runtime_error("Video decode failed");render();av_packet_unref(packet);
  }
 } catch(const std::exception &error){
  outcome.error=QString::fromUtf8(error.what());
  const QStringList transient={"Cannot connect to DVRIP device","DVRIP disconnected","DVRIP read cancelled or timed out","DVRIP write failed","DVRIP write timed out","Cannot open video source","Cannot read stream information","Stream disconnected or timed out","Video decode failed"};
  outcome.retry=!url.isLocalFile() && transient.contains(outcome.error);
  if(!cancelled)status(outcome.error);
 }
 speakers.clear();sound.reset();closeRecord();av_packet_free(&packet);av_frame_free(&decoded);av_frame_free(&cpu);sws_freeContext(scale);avcodec_free_context(&decoder);av_buffer_unref(&device);
 avformat_close_input(&format);if(io){av_freep(&io->buffer);avio_context_free(&io);}if(cancelled)status("Stopped");
 return outcome;
}
