#pragma once
#include <QtCore>
#include <QtEndian>
#include <atomic>
#include <climits>
#include <algorithm>
#include <map>
#include <stdexcept>
#ifdef Q_OS_LINUX
#include <sys/ioctl.h>
#include <linux/fs.h>
#endif
extern "C" {
#include <libavutil/error.h>
}
namespace wfs {
inline void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
inline quint32 u32(const QByteArray &b,int o){return qFromLittleEndian<quint32>(b.constData()+o);}
inline quint16 u16(const QByteArray &b,int o){return qFromLittleEndian<quint16>(b.constData()+o);}
inline qint64 stamp(quint32 v){QDate d(2000+(v>>26),(v>>22)&15,(v>>17)&31);QTime t((v>>12)&31,(v>>6)&63,v&63);return d.isValid()&&t.isValid()?QDateTime(d,t,Qt::UTC).toMSecsSinceEpoch():-1;}
inline QByteArray exact(QFile &file,qint64 offset,qint64 length){require(file.seek(offset),"Cannot seek recorder disk");auto b=file.read(length);require(b.size()==length,"Short disk read; disk disconnected or truncated image");return b;}
inline qint64 diskSize(QFile &file){
#ifdef Q_OS_LINUX
 quint64 size=0;if(ioctl(file.handle(),BLKGETSIZE64,&size)==0){require(size<=quint64(LLONG_MAX),"Disk too large");return qint64(size);}
#endif
 return file.size();
}
struct Clip {quint32 primary=0;int channel=0;qint64 begin=0,end=0,length=0;QVector<quint32> fragments;};
struct Index {
 QString path,identity;QByteArray entries;QVector<Clip> clips;
 qint64 size=0,data=0,fragment=0;quint32 count=0;int rejected=0;
 Clip clip(quint32 primary)const{
  require(primary<count,"Recording outside index");auto b=entries.mid(primary*32,32);Clip c;c.primary=primary;
  require(quint8(b[1])==2||quint8(b[1])==3,"Not a primary recording entry");int code=quint8(b[31]);require(code>=2&&(code-2)%4==0,"Unknown channel encoding");c.channel=(code-2)/4+1;
  c.begin=stamp(u32(b,12));c.end=stamp(u32(b,16));require(c.begin>=0&&c.end>=c.begin&&c.end-c.begin<=86400000,"Invalid recording times");
  quint32 n=u16(b,2)+1,cur=primary;require(n<=count,"Invalid recording fragment count");QSet<quint32> seen;
  for(quint32 ordinal=0;ordinal<n;++ordinal){require(cur<count&&!seen.contains(cur),"Invalid or cyclic recording chain");auto r=entries.mid(cur*32,32);
   require(r[31]==b[31]&&u32(r,24)==primary,"Recording chain owner/channel mismatch");
   if(ordinal)require(quint8(r[1])==1&&u16(r,2)==ordinal&&u32(r,4)==c.fragments.last(),"Broken recording continuation");
   seen.insert(cur);c.fragments.append(cur);cur=u32(r,8);
  }
  qint64 tail=qint64(u16(b,22))*512;require(tail>0&&tail<=fragment,"Invalid recording tail size");c.length=(n-1)*fragment+tail;return c;
 }
 static Index read(const QString &path,const std::atomic_bool &cancel){
  QFile f(path);require(f.open(QIODevice::ReadOnly),"Cannot read disk. Select the correct disk/image and grant your user read access; do not run the app as root.");
  Index i;i.path=path;i.size=diskSize(f);auto header=exact(f,0,65536);
  require(header.left(6)=="WFS0.4"&&header.mid(510,2)=="XM","Unsupported disk signature (expected WFS0.4 / XM)");
  auto sb=header.mid(0x3000,128);quint32 block=u32(sb,0x2c);i.fragment=qint64(block)*u32(sb,0x30);i.count=u32(sb,0x4c);qint64 index=qint64(u32(sb,0x44))*block;i.data=qint64(u32(sb,0x48))*block;
  require(block==512&&i.fragment==2097152&&i.count>0&&i.count<=1048576,"Unsupported WFS geometry");
  qint64 bytes=qint64(i.count)*32;require(index>=65536&&index+bytes<=i.data&&i.data<i.size&&qint64(i.count)*i.fragment<=i.size-i.data,"WFS index or data area outside disk bounds");
  require(!cancel,"Cancelled");i.entries=exact(f,index,bytes);QCryptographicHash hash(QCryptographicHash::Sha256);hash.addData(header);hash.addData(i.entries);i.identity=QString::fromLatin1(hash.result().toHex());
  for(quint32 p=0;p<i.count;++p){if((p&1023)==0)require(!cancel,"Cancelled");int type=quint8(i.entries[int(p*32+1)]);if(type!=2&&type!=3)continue;try{i.clips.append(i.clip(p));}catch(const std::exception&){++i.rejected;}}
  std::sort(i.clips.begin(),i.clips.end(),[](const Clip &a,const Clip &b){return a.begin==b.begin?a.channel<b.channel:a.begin<b.begin;});return i;
 }
};
// The source stays read-only. At most one 2 MiB fragment is cached.
class Stream {
 QFile file;Index index;Clip selected;std::atomic_bool &cancel;
 qint64 cursor=0,cacheNumber=-1,outputOffset=0,lastStamp=-1,anchor=-1;int sinceAnchor=0;
 qint64 frameStart=0;bool keyframe=false;
 QByteArray cache,pending;std::map<qint64,qint64> times;
 QByteArray read(qint64 n){require(n>=0&&n<=8388608&&cursor+n<=selected.length,"Truncated WFS frame");QByteArray result;while(n){require(!cancel,"Cancelled");qint64 part=cursor/index.fragment,offset=cursor%index.fragment;
   if(cacheNumber!=part){require(part<selected.fragments.size(),"Missing WFS fragment");cache=exact(file,index.data+qint64(selected.fragments[part])*index.fragment,index.fragment);cacheNumber=part;}
   qint64 take=qMin(n,index.fragment-offset);result+=cache.mid(offset,take);cursor+=take;n-=take;
  }return result;
 }
 QByteArray next(){while(cursor<selected.length&&!cancel){frameStart=cursor;auto h=read(qMin<qint64>(4,selected.length-cursor));
   if(!h.isEmpty()&&h==QByteArray(h.size(),char(255))){require(selected.length-cursor<4096,"Unexpected padding inside recording");auto rest=read(selected.length-cursor);for(char c:rest)require(quint8(c)==255,"Invalid recording padding");return {};}
   require(h.size()==4&&h.left(3)==QByteArray::fromHex("000001"),"Unsupported WFS frame marker");int type=quint8(h[3]);require(type==252||type==254||type==253||type==250||type==249,"Unsupported WFS frame type");int hs=(type==252||type==254)?16:8;h+=read(hs-4);quint32 n=hs==16?u32(h,12):(type==253?u32(h,4):u16(h,6));require(n<=8388608,"WFS frame too large");auto payload=read(n);
   if(type==250||type==249)continue;keyframe=(hs==16);
   if(hs==16){int code=quint8(h[4])&15;require((code==2||code==3)&&(!codec||codec==code),"Unsupported WFS codec change");codec=code;fps=quint8(h[5])&31;require(fps>0,"Invalid WFS frame rate");anchor=stamp(u32(h,8));require(anchor>=0,"Invalid media timestamp");sinceAnchor=0;if(firstStamp<0)firstStamp=anchor;}
   require(codec&&anchor>=0,"Recording does not begin with a keyframe");require(payload.startsWith(QByteArray::fromHex("000001"))||payload.startsWith(QByteArray::fromHex("00000001")),"Unsupported/encrypted WFS media");
   qint64 time=anchor+qint64(sinceAnchor++)*1000/fps;lastStamp=qMax(time,lastStamp+1);require(times.size()<32768,"WFS timestamp buffer limit");times[outputOffset]=lastStamp;outputOffset+=payload.size();return payload;
  }return {};}
public:
 int codec=0,fps=25;QString error;qint64 firstStamp=-1;
 explicit Stream(std::atomic_bool &stop):cancel(stop){}
 void open(const QUrl &url){index=Index::read(url.toLocalFile(),cancel);QUrlQuery q(url);require(index.identity==q.queryItemValue("wfsIdentity"),"Disk index changed. Reopen the disk to refresh recordings.");bool ok=false;auto p=q.queryItemValue("wfsPrimary").toUInt(&ok);require(ok,"Invalid WFS recording identifier");selected=index.clip(p);file.setFileName(index.path);require(file.open(QIODevice::ReadOnly),"Cannot reopen disk for playback");pending=next();require(!pending.isEmpty(),"Recording contains no video");
  qint64 seek=q.queryItemValue("seekMs").toLongLong();if(q.hasQueryItem("wfsTime")){bool valid=false;auto target=q.queryItemValue("wfsTime").toLongLong(&valid);require(valid,"Invalid wall-clock seek");seek=qMax<qint64>(0,target-firstStamp);}
  if(seek>0){qint64 key=0;int frames=0;while(lastStamp<firstStamp+seek){require(++frames<2000000,"Recording scan limit");auto bytes=next();if(bytes.isEmpty())break;if(keyframe&&lastStamp<=firstStamp+seek)key=frameStart;times.clear();}
   cursor=key;outputOffset=0;times.clear();lastStamp=-1;anchor=-1;sinceAnchor=0;pending=next();
  }
 }
 qint64 timestamp(qint64 offset){auto it=times.upper_bound(offset);if(it==times.begin())return -1;--it;auto value=it->second;times.erase(times.begin(),it);return value;}
 static int readPacket(void *opaque,uint8_t *out,int size){auto *s=static_cast<Stream*>(opaque);try{if(s->pending.isEmpty())s->pending=s->next();if(s->pending.isEmpty())return AVERROR_EOF;int n=qMin(size,int(s->pending.size()));memcpy(out,s->pending.constData(),n);s->pending.remove(0,n);return n;}catch(const std::exception &e){s->error=QString::fromUtf8(e.what());return AVERROR(EIO);}}
};
}
