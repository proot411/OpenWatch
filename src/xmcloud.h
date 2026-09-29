#pragma once
#include <QtNetwork>
#include <atomic>
#include <functional>
#include <stdexcept>

// Independently implemented from the owner's capture and protocol observations.
// No vendor code, DLL, account token or captured session is required.
namespace xm::cloud {
using Progress = std::function<void(const QString &)>;
struct Endpoint { QHostAddress address; quint16 port=0; };
bool validSerial(const QString &serial);
QByteArray packet(quint16 command,int length,quint16 magic=0x2012);
void putText(QByteArray &packet,int offset,int width,const QByteArray &text);
Endpoint endpoint(const QByteArray &packet,int offset,int width,int portOffset);
QByteArray crypt(QByteArray data,quint32 key,int stride=1);
QByteArray aesEncode(const QByteArray &plain,const QByteArray &key,bool extraBlock=false);
QByteArray aesDecode(const QByteArray &encoded,const QByteArray &key);
QByteArray rsaEncode(const QByteArray &plain,const QString &publicKey);
QByteArray loginKey();
QByteArray wrapperKey();

// Bounded, ordered XMIP receiver. ACKs describe a consecutive received window.
class Receiver {
 QMap<quint32,QByteArray> waiting;
 QByteArray assembling;
 quint32 next=0; bool started=false;
public:
 QByteArray accept(const QByteArray &slice);
 QByteArray ack()const;
};
class Transport {
 QUdpSocket socket;
 std::atomic_bool &cancel;
 Progress progress;
 Endpoint relay;
 QByteArray serial,uuid;
 quint32 localId=0,remoteId=0,key=0; int stride=1;
 quint32 sendSequence=0,pingSequence=0;
 Receiver receiver;
 QByteArray buffered;
 QElapsedTimer alive;
 bool connected=false;
 void check() const;
 void heartbeat();
 void sendTo(const QByteArray &data,const Endpoint &to);
 QByteArray receive(const Endpoint &from,int milliseconds);
 QByteArray exchange(const QByteArray &request,const Endpoint &to,quint16 reply,int minimum);
 QByteArray envelope(const QByteArray &slice)const;
 QByteArray sync(quint16 command,quint32 tag)const;
 void consume(const QByteArray &datagram);
public:
 explicit Transport(std::atomic_bool &stop,Progress callback={}):cancel(stop),progress(std::move(callback)){}
 ~Transport();
 void open(const QString &cloudId,const QString &bootstrap="159.138.1.83");
 void write(const QByteArray &bytes);
 // Called on the owning worker thread to keep an idle control association alive.
 void service();
 void writeClosing(const QByteArray &bytes) noexcept;
 QByteArray read(qint64 size,const std::function<void()> &idle={});
};
}
