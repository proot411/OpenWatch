#include "audio.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <mutex>
#include <stdexcept>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}
AudioOutput::~AudioOutput(){QMutexLocker lock(&mutex);if(device)SDL_CloseAudioDevice(device);}
void AudioOutput::enable(bool value){QMutexLocker lock(&mutex);active=value;if(device){SDL_ClearQueuedAudio(device);if(!value){SDL_CloseAudioDevice(device);device=0;}}message=value?"Waiting for audio":"Muted";}
bool AudioOutput::enabled(){QMutexLocker lock(&mutex);return active;}
void AudioOutput::setVolume(int value){QMutexLocker lock(&mutex);volume=qBound(0,value,100);}
void AudioOutput::clear(){QMutexLocker lock(&mutex);if(device)SDL_ClearQueuedAudio(device);}
void AudioOutput::error(const QString &text){QMutexLocker lock(&mutex);if(active)message=text;}
QString AudioOutput::status(){QMutexLocker lock(&mutex);return message;}
void AudioOutput::queue(QByteArray pcm){
 QMutexLocker lock(&mutex);if(!active)return;
 if(!device){
  static std::once_flag init;static bool ready=false;std::call_once(init,[]{SDL_SetMainReady();ready=SDL_InitSubSystem(SDL_INIT_AUDIO)==0;});
  if(!ready)throw std::runtime_error("Audio output initialization failed");
  SDL_AudioSpec wanted{};wanted.freq=48000;wanted.format=AUDIO_S16SYS;wanted.channels=2;wanted.samples=1024;
  device=SDL_OpenAudioDevice(nullptr,0,&wanted,nullptr,0);if(!device)throw std::runtime_error("Cannot open the default speaker output");SDL_PauseAudioDevice(device,0);
 }
 // Limit live latency instead of accumulating audio from stalled video/network reads.
 if(SDL_GetQueuedAudioSize(device)>48000)SDL_ClearQueuedAudio(device);
 auto *samples=reinterpret_cast<qint16*>(pcm.data());for(int i=0;i<pcm.size()/2;++i)samples[i]=qint16(int(samples[i])*volume/100);
 if(SDL_QueueAudio(device,pcm.constData(),pcm.size())<0)throw std::runtime_error("Speaker output failed");
 delivered+=pcm.size();message="Live audio";
}
struct AudioDecoder::Impl {
 AVCodecContext *codec=nullptr;AVFrame *frame=av_frame_alloc();SwrContext *swr=nullptr;
 int rate=0,format=-1,channels=0;
 ~Impl(){swr_free(&swr);av_frame_free(&frame);avcodec_free_context(&codec);}
};
AudioDecoder::AudioDecoder(const AVCodecParameters *parameters):impl(std::make_unique<Impl>()){
 auto *codec=avcodec_find_decoder(parameters->codec_id);if(!codec)throw std::runtime_error("Audio codec is unavailable");
 impl->codec=avcodec_alloc_context3(codec);if(!impl->codec||!impl->frame||avcodec_parameters_to_context(impl->codec,parameters)<0||avcodec_open2(impl->codec,codec,nullptr)<0)throw std::runtime_error("Cannot initialize audio decoder");
}
AudioDecoder::~AudioDecoder()=default;
void AudioDecoder::consume(const AVPacket *packet,AudioOutput &output){
 auto drain=[&]{while(avcodec_receive_frame(impl->codec,impl->frame)==0){auto *frame=impl->frame;
#if LIBAVUTIL_VERSION_MAJOR >= 57
 int channels=frame->ch_layout.nb_channels;
#else
 int channels=frame->channels;
#endif
 if(frame->sample_rate<=0||channels<=0||channels>32)throw std::runtime_error("Invalid audio format");
 if(!impl->swr||impl->rate!=frame->sample_rate||impl->format!=frame->format||impl->channels!=channels){
  swr_free(&impl->swr);
#if LIBAVUTIL_VERSION_MAJOR >= 57
  AVChannelLayout stereo=AV_CHANNEL_LAYOUT_STEREO;
  if(swr_alloc_set_opts2(&impl->swr,&stereo,AV_SAMPLE_FMT_S16,48000,&frame->ch_layout,AVSampleFormat(frame->format),frame->sample_rate,0,nullptr)<0)throw std::runtime_error("Audio conversion allocation failed");
#else
  impl->swr=swr_alloc_set_opts(nullptr,AV_CH_LAYOUT_STEREO,AV_SAMPLE_FMT_S16,48000,frame->channel_layout?frame->channel_layout:av_get_default_channel_layout(channels),AVSampleFormat(frame->format),frame->sample_rate,0,nullptr);
#endif
  if(!impl->swr||swr_init(impl->swr)<0)throw std::runtime_error("Cannot convert audio for speakers");
  impl->rate=frame->sample_rate;impl->format=frame->format;impl->channels=channels;
 }
 int count=swr_get_out_samples(impl->swr,frame->nb_samples);if(count<0||count>480000)throw std::runtime_error("Audio frame too large");
 QByteArray pcm(count*4,0);auto *dest=reinterpret_cast<uint8_t*>(pcm.data());
 int made=swr_convert(impl->swr,&dest,count,const_cast<const uint8_t**>(frame->extended_data),frame->nb_samples);if(made<0)throw std::runtime_error("Audio conversion failed");pcm.resize(made*4);output.queue(pcm);av_frame_unref(frame);
 }};
 int sent=avcodec_send_packet(impl->codec,packet);if(sent==AVERROR(EAGAIN)){drain();sent=avcodec_send_packet(impl->codec,packet);}if(sent<0)throw std::runtime_error("Audio decoding failed");drain();
}
