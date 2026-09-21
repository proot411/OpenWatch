#pragma once
#include <QtNetwork>
#include <QtXml>
#include <atomic>
namespace lan {
struct Device {QString kind,name,host,id;QUrl endpoint;int port=34567;};
struct Profile {QString name,token,source,ptzConfig;QUrl endpoint,uri;bool media2=false;};
QByteArray probe(const QString &id);
QVector<Device> parseOnvif(const QByteArray &xml,const QHostAddress &sender,const QSet<QString> &requests);
Device parseXm(const QByteArray &bytes,const QHostAddress &sender);
QDomDocument xmlDocument(const QByteArray &data);
QList<QDomElement> elements(const QDomNode &node,const QString &name);
QString value(const QDomNode &node,const QString &name);
class Scanner:public QObject {
 Q_OBJECT
 QUdpSocket xmSocket;
 QList<QUdpSocket*> onvifSockets;
 QTimer repeat,finish;
 QSet<QString> requests,seen;
 QList<QNetworkInterface> interfaces;
 void transmit();
 void publish(const Device &device);
public:
 explicit Scanner(QObject *parent=nullptr);
 void start(int interfaceIndex=0);
 void stop();
signals:
 void found(lan::Device device);
 void status(QString text);
 void finished();
};
class Onvif {
 QNetworkAccessManager manager;
 std::atomic_bool &cancel;
 QString user,password,host;
 qint64 clockOffset=0;
 QDomDocument request(QUrl endpoint,QString space,QString operation,QString body={},bool authentication=true);
 QUrl endpoint(QUrl url) const;
public:
 Onvif(const QUrl &device,QString username,QString secret,std::atomic_bool &cancelled);
 qint64 timeOffset()const{return clockOffset;}
 void setTimeOffset(qint64 value){clockOffset=value;}
 QVector<Profile> profiles(const QUrl &device);
 QUrl stream(const Profile &profile);
 QDomDocument deviceInfo(const QUrl &device){return request(device,"http://www.onvif.org/ver10/device/wsdl","GetDeviceInformation");}
 QUrl ptzService(const QUrl &device);
 QDomDocument ptz(const QUrl &service,const QString &operation,const QString &body){return request(service,"http://www.onvif.org/ver20/ptz/wsdl",operation,body);}

};
}
Q_DECLARE_METATYPE(lan::Device)
