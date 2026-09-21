#pragma once
#include <QtCore>
#include <QtEndian>
#include <atomic>
#include <stdexcept>
extern "C" {
#include <libavutil/error.h>
}
#include "dvrip.h"
namespace xm {
// Sequential reader for headerless Sofia frame-container exports. Never modifies the source.
class Recording {
 QFile file;
 std::atomic_bool &cancel;
 QByteArray pending;
 QByteArray exact(qint64 count){auto bytes=file.read(count);if(bytes.size()!=count)throw std::runtime_error("Truncated XM recording frame");return bytes;}
public:
 int codec=0,fps=25;QString error;
 explicit Recording(std::atomic_bool &stop):cancel(stop){}
 static bool recognizes(const QByteArray &prefix){return prefix.size()>=4 && prefix.left(3)==QByteArray::fromHex("000001") && QList<quint8>{0xfc,0xfe,0xfd,0xfa,0xf9}.contains(quint8(prefix[3]));}
 void open(const QString &path){file.setFileName(path);if(!file.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot open XM recording");pending=next();if(pending.isEmpty() || !codec)throw std::runtime_error("XM recording has no supported keyframe");}
 QByteArray next(){
  while(!cancel){if(file.atEnd())return {};auto header=exact(4);if(!recognizes(header))throw std::runtime_error("Unsupported XM recording block (possibly another format or encryption)");
   int type=quint8(header[3]);int size=(type==0xfc||type==0xfe)?16:8;header+=exact(size-4);
   quint32 length=size==16?qFromLittleEndian<quint32>(header.constData()+12):(type==0xfd?qFromLittleEndian<quint32>(header.constData()+4):qFromLittleEndian<quint16>(header.constData()+6));
   if(length>MaxPayload)throw std::runtime_error("XM recording frame exceeds size limit");auto payload=exact(length);
   if(type==0xfa||type==0xf9)continue;
   if(size==16){int nextCodec=quint8(header[4])&15;if(nextCodec!=2&&nextCodec!=3)throw std::runtime_error("Unsupported XM recording codec or encryption");if(codec&&codec!=nextCodec)throw std::runtime_error("XM recording changes codec mid-file");codec=nextCodec;fps=qMax(1,int(quint8(header[5])&31));}
   if(!codec)continue;
   if(!payload.startsWith(QByteArray::fromHex("000001")) && !payload.startsWith(QByteArray::fromHex("00000001")))throw std::runtime_error("XM recording payload is not clear H.264/H.265; encrypted variants are not supported");
   return payload;
  }return {};
 }
 static int read(void *opaque,uint8_t *out,int size){auto *self=static_cast<Recording*>(opaque);try{if(self->pending.isEmpty())self->pending=self->next();if(self->pending.isEmpty())return AVERROR_EOF;int count=qMin(size,int(self->pending.size()));memcpy(out,self->pending.constData(),count);self->pending.remove(0,count);return count;}catch(const std::exception &e){self->error=e.what();return AVERROR(EIO);}}
};
}
