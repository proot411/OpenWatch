#pragma once
#include <QtWidgets>
#include "video.h"
#include "zoom.h"
#include "theme.h"
class Cell:public QWidget {
public:
 Video video; QUuid deviceId; QString name; bool selected=false; double zoom=1; QPointF zoomPan; std::function<void(Cell*)> select, activate; std::function<void(Cell*,const QUuid&)> drop;
 Cell(){setMinimumSize(100,70);setAcceptDrops(true);}
 void paintEvent(QPaintEvent*) override {
  QPainter p(this);p.fillRect(rect(),QColor(theme::cellBg));auto frame=video.frame();
  if(!frame.isNull()) {QRectF view(0,0,width(),qMax(1,height()-32));p.setClipRect(view);p.drawImage(digitalzoom::imageRect(frame.size(),view,zoom,zoomPan),frame);p.setClipping(false);}
  else {p.setPen(QColor(theme::muted));p.drawText(rect(),Qt::AlignCenter,name.isEmpty()?"＋\nDrop a camera here":"Waiting for video…");}
  p.fillRect(0,height()-32,width(),32,QColor(theme::cellStrip));p.setPen(QColor(theme::cellText));p.drawText(QRect(12,height()-32,width()-24,32),Qt::AlignVCenter,(name.isEmpty()?"Empty channel":name)+"   ·   "+QString(video.status()).replace("Recording · ","Live · "));
  bool recording=video.isRecording();bool flash=(QDateTime::currentMSecsSinceEpoch()/500)%2==0;
  if(recording)p.setPen(QPen(QColor(flash?theme::danger:theme::dangerDim),4));else p.setPen(QPen(QColor(selected?theme::accent:theme::border),selected?2:1));p.drawRect(rect().adjusted(1,1,-1,-1));
 }
 void mousePressEvent(QMouseEvent*)override{if(select)select(this);}
 void mouseDoubleClickEvent(QMouseEvent*)override{if(activate)activate(this);}
 void wheelEvent(QWheelEvent *e)override{auto frame=video.frame();if(frame.isNull()){e->ignore();return;}digitalzoom::at(frame.size(),QRectF(0,0,width(),qMax(1,height()-32)),e->position(),e->angleDelta().y()/120.0,zoom,zoomPan);e->accept();update();}
 void dragEnterEvent(QDragEnterEvent *e)override{if(e->mimeData()->hasFormat("application/x-openwatch-camera"))e->acceptProposedAction();}
 void dropEvent(QDropEvent *e)override{QUuid id(QString::fromUtf8(e->mimeData()->data("application/x-openwatch-camera")));if(!id.isNull() && drop)drop(this,id);}
};
