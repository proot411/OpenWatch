#pragma once
#include <QtWidgets>
#include "discovery.h"
class CameraControls:public QDialog {
 QUrl source,service;
 QLineEdit *address;
 QCheckBox *compatibility;
 QComboBox *profiles,*presets;
 QSpinBox *presetNumber,*speed;
 QLabel *status;
 QWidget *actions;
 QVector<lan::Profile> available;
 QThread *worker=nullptr;
 std::atomic_bool cancel{false};
 QString moveTimeout="PT0.2S";
 bool loaded=false;
 QString error;bool xmMode,pan=false,zoom=false;
 qint64 offset=0;
 void job(std::function<void()> work,std::function<void()> done={});
 void capabilities();
 void command(QString xmCommand,int x=0,int y=0,int z=0,QString presetAction={});
public:
 CameraControls(const QString &name,const QUrl &url,const QString &endpoint,QWidget *parent=nullptr);
 ~CameraControls();
 std::function<void(QString)> report;
 bool matchesSource(const QUrl &url)const{return source==url;}
 void keyboard(const QString &action);
 QString endpoint()const{return address->text().trimmed();}
};
