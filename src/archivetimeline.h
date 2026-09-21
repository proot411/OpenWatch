#pragma once
#include <QtWidgets>
#include <functional>
namespace archive {
struct Clip {int channel;int begin,end;QString name;};
inline int at(const QVector<Clip>& clips,int channel,int second){for(int i=0;i<clips.size();++i)if(clips[i].channel==channel&&clips[i].begin<=second&&second<clips[i].end)return i;return -1;}
inline QVector<int> channels(QString text){QVector<int> result;for(auto part:text.split(',')){bool ok;int n=part.trimmed().toInt(&ok);if(!ok||n<1||n>256)return {};if(!result.contains(n-1))result.append(n-1);}return result.size()<=4?result:QVector<int>{};}
class Timeline:public QWidget {
 int origin=0,span=86400,cursor=0;bool dragging=false;
 int second(double x)const{return qBound(0,origin+int((x-70)/qMax(1,width()-80)*span),86399);}
public:
 QVector<Clip> clips;QVector<int> channels;
 std::function<void(int,bool)> scrub;
 explicit Timeline(QWidget *parent=nullptr):QWidget(parent){setMinimumHeight(150);setMouseTracking(true);setToolTip("Drag to seek · Mouse wheel to zoom around the pointer");}
 int position()const{return cursor;}
 void setPosition(int n){cursor=qBound(0,n,86399);if(!dragging&&(cursor<origin||cursor>origin+span))origin=qBound(0,cursor-span/2,86400-span);update();}
 void zoom(int seconds){span=qBound(300,seconds,86400);origin=qBound(0,cursor-span/2,86400-span);update();}
protected:
 void paintEvent(QPaintEvent*)override{QPainter p(this);p.fillRect(rect(),QColor("#101b26"));int w=qMax(1,width()-80);auto x=[&](int s){return 70.0+(s-origin)*double(w)/span;};
  p.setPen(QColor("#a5bacb"));for(int i=0;i<=6;++i){int t=origin+span*i/6;double px=x(t);p.drawText(QRectF(qBound(0.0,px-28,double(width()-60)),5,60,20),Qt::AlignCenter,(t==86400?QString("24:00"):QTime(0,0).addSecs(t).toString("HH:mm")));p.setPen(QColor("#263747"));p.drawLine(QPointF(px,28),QPointF(px,height()));p.setPen(QColor("#a5bacb"));}
  int h=qMax(22,(height()-32)/qMax(1,int(channels.size())));for(int row=0;row<channels.size();++row){int y=32+row*h;p.setPen(QColor("#a5bacb"));p.drawText(QRect(4,y,65,h),Qt::AlignVCenter,QString("CH %1").arg(channels[row]+1));for(auto c:clips)if(c.channel==channels[row]&&c.end>origin&&c.begin<origin+span){double left=qMax(70.0,x(c.begin)),right=qMin(double(width()-10),x(c.end));p.fillRect(QRectF(left,y+5,qMax(1.0,right-left),h-10),QColor("#319e91"));}}
  p.setPen(QPen(QColor("#ffd27c"),2));p.drawLine(QPointF(x(cursor),25),QPointF(x(cursor),height()));
 }
 void mousePressEvent(QMouseEvent*e)override{if(e->button()!=Qt::LeftButton)return;dragging=true;cursor=second(e->position().x());update();if(scrub)scrub(cursor,false);}
 void mouseMoveEvent(QMouseEvent*e)override{if(!dragging)return;cursor=second(e->position().x());update();if(scrub)scrub(cursor,false);}
 void mouseReleaseEvent(QMouseEvent*e)override{if(!dragging||e->button()!=Qt::LeftButton)return;cursor=second(e->position().x());dragging=false;update();if(scrub)scrub(cursor,true);}
 void wheelEvent(QWheelEvent*e)override{int anchor=second(e->position().x());double ratio=qBound(0.0,(e->position().x()-70)/qMax(1,width()-80),1.0);span=qBound(300,int(span*(e->angleDelta().y()>0?0.5:2)),86400);origin=qBound(0,anchor-int(ratio*span),86400-span);update();e->accept();}
};
}
