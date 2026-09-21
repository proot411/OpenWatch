#pragma once
#include <QtNetwork>
#include <atomic>
#include <stdexcept>
#include <memory>
namespace xm {
constexpr quint32 MaxPayload = 8 * 1024 * 1024;
QString digest(const QString &password);
QByteArray header(quint32 session, quint32 sequence, quint16 command, quint32 size);
quint32 payloadSize(const QByteArray &header);
int channelCount(const QJsonObject &login, const QJsonObject &system = {});
QVector<QUrl> channelUrls(const QUrl &recorder, int count, bool substream);
class MediaParser {
 QByteArray buffer;
public:
 int codec = 0, fps = 25;
 qint64 timestamp=-1, anchor=-1; int sinceAnchor=0;
 void append(const QByteArray &bytes);
 QByteArray take();
};
class Client {
 QTcpSocket socket;
 quint32 session = 0, sequence = 0;
 std::atomic_bool &stop;
 MediaParser parser;
 QElapsedTimer heartbeat;
 quint16 messageId=0;
 std::unique_ptr<Client> archiveControl;
 QJsonObject archiveRequest;
 bool archive=false, archiveEnded=false, download=false;
 QByteArray exact(qint64 size);
 QByteArray chunk();
 void send(quint16 command, QJsonObject body);
 QJsonObject response(bool fileSearch=false);
 QJsonObject login(const QUrl &url);
public:
 explicit Client(std::atomic_bool &cancel):stop(cancel){}
 ~Client();
 QJsonArray recordings(const QUrl &url,int channel,const QString &begin,const QString &end);
 void controlLogin(const QUrl &url){login(url);}
 QJsonObject control(quint16 command,const QString &name,const QJsonValue &parameters=QJsonValue());
 void ptz(int channel,const QString &command,int preset,int step=1);
 int scanChannels(const QUrl &url);
 void open(const QUrl &url);
 QByteArray readVideo();
 void keepArchiveAlive();
 qint64 timestamp()const{return parser.timestamp;}
 int codec() const { return parser.codec; }
 int fps() const { return parser.fps; }
};
}
