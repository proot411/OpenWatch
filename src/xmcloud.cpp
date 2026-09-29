#include "xmcloud.h"
#include <QtEndian>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/rand.h>
#include <memory>
namespace xm::cloud {
namespace {
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
quint16 u16(const QByteArray &b,int i){require(i>=0&&i+2<=b.size(),"Truncated cloud packet");return qFromLittleEndian<quint16>(b.constData()+i);}
quint32 u32(const QByteArray &b,int i){require(i>=0&&i+4<=b.size(),"Truncated cloud packet");return qFromLittleEndian<quint32>(b.constData()+i);}
QByteArray textAt(const QByteArray &b,int i,int n){require(i>=0&&i+n<=b.size(),"Truncated cloud field");auto s=b.mid(i,n);int end=s.indexOf('\0');require(end>=0,"Unterminated cloud field");return s.left(end);}
bool same(const Endpoint &a,const Endpoint &b){return a.address==b.address&&a.port==b.port;}
constexpr int Limit=8*1024*1024;
}
bool validSerial(const QString &s){return QRegularExpression("^[A-Za-z0-9_-]{8,64}$").match(s).hasMatch();}
QByteArray packet(quint16 cmd,int length,quint16 magic){require(length>=4&&length<=65507,"Invalid cloud packet length");QByteArray b(length,0);qToLittleEndian(magic,b.data());qToLittleEndian(cmd,b.data()+2);return b;}
void putText(QByteArray &b,int i,int n,const QByteArray &s){require(i>=0&&i+n<=b.size()&&s.size()<n&&!s.contains('\0'),"Invalid cloud text field");b.replace(i,s.size(),s);}
Endpoint endpoint(const QByteArray &b,int i,int width,int po){Endpoint e;require(e.address.setAddress(QString::fromLatin1(textAt(b,i,width))),"Invalid cloud endpoint address");e.port=u16(b,po);require(e.address.protocol()==QAbstractSocket::IPv4Protocol&&!e.address.isNull()&&!e.address.isMulticast()&&e.address!=QHostAddress::Broadcast&&e.port!=0,"Invalid cloud endpoint");return e;}
QByteArray crypt(QByteArray b,quint32 key,int stride){require(stride>=1&&stride<=1024,"Unsupported XMIP obfuscation stride");for(int word=0;word<b.size()/4;word+=(word<16?1:stride)){auto p=b.data()+word*4;qToLittleEndian(qFromLittleEndian<quint32>(p)^key,p);}return b;}
QByteArray wrapperKey(){return QByteArray("dashoiahfarqdasr\0",16);}
QByteArray loginKey(){QByteArray random(16,0);require(RAND_bytes(reinterpret_cast<unsigned char*>(random.data()),random.size())==1,"Cannot generate cloud session key");return random.toHex().left(16);}
QByteArray aesEncode(const QByteArray &plain,const QByteArray &key,bool extra){require(key.size()==16&&plain.size()<=Limit,"Invalid cloud AES input");auto data=plain;int size=extra?(data.size()/16+1)*16:((data.size()+15)/16)*16;data.append(QByteArray(size-data.size(),0));QByteArray out(size+16,0);unsigned char iv[16]={};int n=0,end=0;
 std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);
 require(ctx&&EVP_EncryptInit_ex(ctx.get(),EVP_aes_128_cbc(),nullptr,reinterpret_cast<const unsigned char*>(key.constData()),iv)==1,"Cloud AES initialization failed");EVP_CIPHER_CTX_set_padding(ctx.get(),0);
 require(EVP_EncryptUpdate(ctx.get(),reinterpret_cast<unsigned char*>(out.data()),&n,reinterpret_cast<const unsigned char*>(data.constData()),data.size())==1&&EVP_EncryptFinal_ex(ctx.get(),reinterpret_cast<unsigned char*>(out.data())+n,&end)==1,"Cloud AES encryption failed");out.resize(n+end);return out.toBase64();}
QByteArray aesDecode(const QByteArray &encoded,const QByteArray &key){require(key.size()==16&&encoded.size()<=Limit,"Invalid cloud AES input");auto input=encoded;while(input.endsWith('\0')||input.endsWith('\n')||input.endsWith('#'))input.chop(1);auto result=QByteArray::fromBase64Encoding(input,QByteArray::AbortOnBase64DecodingErrors);require(bool(result)&&!result.decoded.isEmpty()&&result.decoded.size()%16==0,"Invalid encrypted cloud response");auto data=result.decoded;QByteArray out(data.size()+16,0);unsigned char iv[16]={};int n=0,end=0;
 std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);
 require(ctx&&EVP_DecryptInit_ex(ctx.get(),EVP_aes_128_cbc(),nullptr,reinterpret_cast<const unsigned char*>(key.constData()),iv)==1,"Cloud AES initialization failed");EVP_CIPHER_CTX_set_padding(ctx.get(),0);
 require(EVP_DecryptUpdate(ctx.get(),reinterpret_cast<unsigned char*>(out.data()),&n,reinterpret_cast<const unsigned char*>(data.constData()),data.size())==1&&EVP_DecryptFinal_ex(ctx.get(),reinterpret_cast<unsigned char*>(out.data())+n,&end)==1,"Cloud AES decryption failed");out.resize(n+end);while(out.endsWith('\0'))out.chop(1);return out;}
QByteArray rsaEncode(const QByteArray &plain,const QString &pub){auto parts=pub.split(',');require(parts.size()==2&&QRegularExpression("^[0-9A-Fa-f]{256,1024}$").match(parts[0]).hasMatch()&&QRegularExpression("^[0-9A-Fa-f]{1,8}$").match(parts[1]).hasMatch(),"Unsupported recorder RSA public key");
 BIGNUM *n=nullptr,*e=nullptr;BN_hex2bn(&n,parts[0].toLatin1().constData());BN_hex2bn(&e,parts[1].toLatin1().constData());
 std::unique_ptr<RSA,decltype(&RSA_free)> rsa(RSA_new(),RSA_free);
 if(!rsa||!n||!e){BN_free(n);BN_free(e);throw std::runtime_error("RSA allocation failed");}
 if(RSA_set0_key(rsa.get(),n,e,nullptr)!=1){BN_free(n);BN_free(e);throw std::runtime_error("Invalid RSA public key");}
 require(BN_num_bits(n)>=1024&&BN_num_bits(n)<=4096&&BN_is_odd(n)&&BN_is_odd(e)&&BN_cmp(e,BN_value_one())>0,"Invalid RSA public key");require(plain.size()<=RSA_size(rsa.get())-11,"Cloud credentials are too long");QByteArray out(RSA_size(rsa.get()),0);
 require(RSA_public_encrypt(plain.size(),reinterpret_cast<const unsigned char*>(plain.constData()),reinterpret_cast<unsigned char*>(out.data()),rsa.get(),RSA_PKCS1_PADDING)==out.size(),"Cloud RSA encryption failed");return out.toHex().toUpper();}
QByteArray Receiver::accept(const QByteArray &b){require(b.size()>=12&&b.size()<=1280&&b.left(4)=="XMIP","Invalid XMIP slice");require(!(quint8(b[9])&1)&&quint8(b[8])==1,"Unsupported XMIP data slice");auto seq=u32(b,4);if(seq<next)return {};require(quint64(seq)-next<256,"XMIP receive window exceeded");if(waiting.contains(seq)){require(waiting[seq]==b,"Conflicting XMIP retransmission");return {};}
 waiting.insert(seq,b);QByteArray completed;
 while(waiting.contains(next)){auto part=waiting.take(next++);auto flags=quint8(part[9]);if(flags&4){require(!started,"Unexpected XMIP message start");started=true;assembling.clear();}require(started,"XMIP continuation without start");require(assembling.size()+part.size()-12<=Limit,"XMIP message too large");assembling+=part.mid(12);if(flags&8){require(completed.size()+assembling.size()<=Limit,"XMIP receive batch too large"); // Preserve application-message boundaries after slice decoding.
 qint32 n=assembling.size();QByteArray len(4,0);qToLittleEndian<quint32>(n,len.data());completed+=len+assembling;assembling.clear();started=false;}}
 return completed;}
QByteArray Receiver::ack()const{QByteArray b(12,0);b.replace(0,4,"XMIP");quint32 count=qMin<quint32>(16,next);qToLittleEndian(next-count,b.data()+4);b[8]=char(count);b[9]=1;return b;}
Transport::~Transport(){if(connected)socket.writeDatagram(packet(2004,4),relay.address,relay.port);}
void Transport::check()const{if(cancel)throw std::runtime_error("Cloud connection cancelled");}
void Transport::sendTo(const QByteArray &b,const Endpoint &to){check();require(socket.writeDatagram(b,to.address,to.port)==b.size(),"Cloud UDP send failed");}
QByteArray Transport::receive(const Endpoint &from,int milliseconds){QElapsedTimer t;t.start();while(t.elapsed()<milliseconds){check();if(!socket.hasPendingDatagrams()){socket.waitForReadyRead(qMin(50,milliseconds-int(t.elapsed())));continue;}auto d=socket.receiveDatagram(65507);if(same(from,{d.senderAddress(),d.senderPort()}))return d.data();}return {};}
QByteArray Transport::exchange(const QByteArray &request,const Endpoint &to,quint16 reply,int minimum){for(int attempt=0;attempt<4;++attempt){sendTo(request,to);QElapsedTimer t;t.start();while(t.elapsed()<1000){auto b=receive(to,100);if(b.size()>=minimum&&u16(b,0)==u16(request,0)&&u16(b,2)==reply)return b;}}throw std::runtime_error("Cloud server did not respond at the current connection stage");}
QByteArray Transport::sync(quint16 cmd,quint32 tag)const{auto p=packet(cmd,112);qToLittleEndian(localId,p.data()+4);putText(p,8,100,uuid);qToLittleEndian(tag,p.data()+108);return p;}
void Transport::open(const QString &id,const QString &bootstrap){require(validSerial(id),"Enter a valid XMEye serial number (8–64 letters, numbers, '-' or '_')");Endpoint boot;require(boot.address.setAddress(bootstrap)&&boot.address.protocol()==QAbstractSocket::IPv4Protocol,"Cloud bootstrap must be an IPv4 address");boot.port=7999;
 serial=id.toLatin1();uuid="!xmnatuuid-IEClient-"+QUuid::createUuid().toString(QUuid::Id128).toLatin1();localId=QRandomGenerator::system()->bounded(1000u,0x7fffffffu);
 require(socket.bind(QHostAddress::AnyIPv4,0),"Cannot open cloud UDP socket");socket.setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,2*1024*1024);auto stage=[&](const QString &s){if(progress)progress(s);};stage("Contacting XMEye bootstrap…");auto p=packet(1524,104,0x2014);putText(p,4,100,"1234567890ab");auto b=exchange(p,boot,1525,32);Endpoint directory=endpoint(b,4,24,28);
 stage("Looking up recorder serial number…");p=packet(0xb000,5+serial.size(),0x2015);p.replace(4,serial.size(),serial);b=exchange(p,{directory.address,8777},0xb001,140);require(u32(b,4)==1&&textAt(b,8,100)==serial,"Recorder not found by XMEye directory");auto rendezvous=endpoint(b,108,20,128);
 stage("Negotiating recorder connection…");b=exchange(packet(1000,4),rendezvous,1001,24);auto publicAddress=endpoint(b,4,16,20);
 p=packet(1004,364);qToLittleEndian(localId,p.data()+4);putText(p,8,100,uuid);putText(p,108,100,serial);putText(p,208,16,publicAddress.address.toString().toLatin1());qToLittleEndian(publicAddress.port,p.data()+224);qToLittleEndian(socket.localPort(),p.data()+226);
 // Relay-first: no private/LAN addresses are advertised or dialled.
 qToLittleEndian<quint32>(15,p.data()+352);stride=4;qToLittleEndian<quint32>(stride,p.data()+360);
 bool gotKey=false,gotPeer=false;QElapsedTimer timer;timer.start();qint64 nextSend=0;
 while(timer.elapsed()<8000&&(!gotKey||!gotPeer)){if(timer.elapsed()>=nextSend){sendTo(p,rendezvous);nextSend=timer.elapsed()+1000;}b=receive(rendezvous,100);if(b.size()<4||u16(b,0)!=0x2012)continue;
  if(u16(b,2)==1005&&b.size()>=24&&u32(b,4)==localId){require(u32(b,8)==1,"Recorder rejected cloud rendezvous");key=u32(b,20);gotKey=true;}
  if(u16(b,2)==1004&&b.size()>=360&&textAt(b,8,100)==serial&&textAt(b,108,100)==uuid&&u32(b,328)==localId){remoteId=u32(b,4);relay=endpoint(b,332,16,348);gotPeer=true;}}
 require(gotKey&&gotPeer,"Recorder rendezvous timed out (offline or unsupported cloud service)");stage("Connecting through XMEye relay…");p=packet(3001,212);putText(p,4,100,uuid);qToLittleEndian(localId,p.data()+104);putText(p,108,100,serial);qToLittleEndian(remoteId,p.data()+208);exchange(p,relay,3002,8);
 timer.restart();nextSend=0;bool synSeen=false,ackSeen=false;
 while(timer.elapsed()<8000&&(!synSeen||!ackSeen)){if(timer.elapsed()>=nextSend){sendTo(sync(2000,1),relay);nextSend=timer.elapsed()+500;}b=receive(relay,100);if(b.size()!=112||u16(b,0)!=0x2012||u32(b,4)!=remoteId||textAt(b,8,100)!=serial)continue;
  if(u16(b,2)==2000){synSeen=true;sendTo(sync(2001,u32(b,108)),relay);}
  if(u16(b,2)==2001){ackSeen=true;}}
 require(synSeen&&ackSeen,"Relay association timed out");connected=true;alive.start();stage("Relay connected; authenticating recorder…");}
QByteArray Transport::envelope(const QByteArray &s)const{auto p=packet(2003,12+s.size());qToLittleEndian<quint16>(s.size(),p.data()+6);p.replace(12,s.size(),s);return p;}
void Transport::consume(const QByteArray &b){if(b.size()<4||u16(b,0)!=0x2012)return;auto cmd=u16(b,2);
 if(cmd==2000&&b.size()==112&&u32(b,4)==remoteId&&textAt(b,8,100)==serial){sendTo(sync(2001,u32(b,108)),relay);return;}
 if(cmd!=2003)return;require(b.size()>=24&&u16(b,6)==b.size()-12,"Invalid cloud data length");auto slice=b.mid(12);if(quint8(slice[9])&1)return;slice.replace(12,slice.size()-12,crypt(slice.mid(12),key,stride));auto batch=receiver.accept(slice);sendTo(envelope(receiver.ack()),relay);int offset=0;while(offset<batch.size()){auto n=u32(batch,offset);offset+=4;auto msg=batch.mid(offset,n);require(buffered.size()+msg.size()<=Limit,"Cloud byte stream buffer exceeded");buffered+=msg;offset+=n;}}
void Transport::write(const QByteArray &bytes){require(connected&&!bytes.isEmpty()&&bytes.size()<=Limit,"Invalid cloud write");const auto &data=bytes;
 for(int offset=0;offset<data.size();offset+=1268){auto n=qMin(1268,int(data.size())-offset);QByteArray s(12+n,0);s.replace(0,4,"XMIP");quint32 seq=sendSequence++;qToLittleEndian(seq,s.data()+4);s[8]=1;s[9]=char(2|(offset==0?4:0)|(offset+n==data.size()?8:0));s.replace(12,n,crypt(data.mid(offset,n),key,stride));auto p=envelope(s);bool ack=false;
  for(int retry=0;retry<8&&!ack;++retry){sendTo(p,relay);QElapsedTimer t;t.start();while(t.elapsed()<400&&!ack){auto b=receive(relay,50);if(b.size()==24&&u16(b,0)==0x2012&&u16(b,2)==2003&&u16(b,6)==12&&b.mid(12,4)=="XMIP"&&(quint8(b[21])&1)){auto base=u32(b,16);ack=seq>=base&&quint64(seq)-base<quint8(b[20]);}else consume(b);}}
  require(ack,"Relay did not acknowledge data; connection lost");}}
// One best-effort Stop packet during teardown. Never wait or ignore cancellation elsewhere.
void Transport::writeClosing(const QByteArray &bytes) noexcept {try{if(!connected||bytes.isEmpty()||bytes.size()>1268)return;QByteArray s(12,0);s.replace(0,4,"XMIP");qToLittleEndian(sendSequence++,s.data()+4);s[8]=1;s[9]=14;s+=crypt(bytes,key,stride);socket.writeDatagram(envelope(s),relay.address,relay.port);}catch(...) {}}
void Transport::heartbeat(){check();if(connected&&alive.elapsed()>=2000){auto p=packet(2002,8);qToLittleEndian<quint16>(quint16(pingSequence++),p.data()+4);sendTo(p,relay);alive.restart();}}
void Transport::service(){heartbeat();for(int n=0;n<64&&socket.hasPendingDatagrams();++n){check();auto d=socket.receiveDatagram(65507);if(same(relay,{d.senderAddress(),d.senderPort()}))consume(d.data());}}
QByteArray Transport::read(qint64 size,const std::function<void()> &idle){require(size>=0&&size<=Limit,"Invalid cloud read size");QElapsedTimer t;t.start();heartbeat();if(idle)idle();while(buffered.size()<size){check();require(t.elapsed()<10000,"Cloud stream timed out");heartbeat();if(idle)idle();consume(receive(relay,100));}auto out=buffered.left(size);buffered.remove(0,size);return out;}
}
