#pragma once
#include <QtCore>
#include <algorithm>
namespace digitalzoom {
inline QRectF imageRect(QSize image,QRectF view,double zoom,QPointF pan){
 if(image.isEmpty() || view.isEmpty())return {};
 QSizeF size=QSizeF(image).scaled(view.size(),Qt::KeepAspectRatio)*zoom;
 QPointF centered=view.center()-QPointF(size.width()/2,size.height()/2);
 QPointF pos=centered+QPointF(pan.x()*view.width(),pan.y()*view.height());
 pos.setX(size.width()<=view.width()?centered.x():qBound(view.right()-size.width(),pos.x(),view.left()));
 pos.setY(size.height()<=view.height()?centered.y():qBound(view.bottom()-size.height(),pos.y(),view.top()));
 return {pos,size};
}
inline void at(QSize image,QRectF view,QPointF cursor,double steps,double &zoom,QPointF &pan){
 auto old=imageRect(image,view,zoom,pan);if(old.isEmpty() || !view.contains(cursor) || !old.contains(cursor) || steps==0)return;
 QPointF uv((cursor.x()-old.left())/old.width(),(cursor.y()-old.top())/old.height());
 double next=qBound(1.0,zoom+steps*.2,5.0);QSizeF size=old.size()*(next/zoom);
 QPointF pos=cursor-QPointF(uv.x()*size.width(),uv.y()*size.height());
 QPointF center=view.center()-QPointF(size.width()/2,size.height()/2);
 pan=QPointF((pos.x()-center.x())/view.width(),(pos.y()-center.y())/view.height());zoom=next;if(zoom==1)pan={};
}
}
