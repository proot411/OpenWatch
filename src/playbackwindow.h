#pragma once
#include "networkplayback.h"
#include "wfstestdialog.h"
class PlaybackWindow:public QDialog {
 QTabWidget *tabs;NetworkPlaybackDialog *network;WfsTestDialog *disk;int previous=0;
public:
 PlaybackWindow(const QUrl &initial,QWidget *parent=nullptr):QDialog(parent){
  setWindowTitle("Playback");auto available=QApplication::primaryScreen()->availableGeometry().size();resize(qMin(1150,available.width()),qMin(760,available.height()-40));auto *root=new QVBoxLayout(this);root->setContentsMargins(8,8,8,8);
  tabs=new QTabWidget;tabs->setObjectName("playbackTabs");root->addWidget(tabs);
  network=new NetworkPlaybackDialog(initial,this);network->setWindowFlags(Qt::Widget);
  disk=new WfsTestDialog(this);disk->setWindowFlags(Qt::Widget);
  tabs->addTab(network,"Network playback");tabs->addTab(disk,"Disk playback");
  connect(tabs,&QTabWidget::currentChanged,this,[this](int index){if(previous==0)network->suspend();else disk->suspend();previous=index;});
 }
 void reject()override{network->suspend();disk->suspend();QDialog::reject();}
 void closeEvent(QCloseEvent *event)override{network->suspend();disk->suspend();QDialog::closeEvent(event);}
};
