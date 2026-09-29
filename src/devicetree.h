#pragma once
#include <QtWidgets>
#include "devices.h"
#include "registry.h"

// The encrypted registry keeps its existing format; tree leaves reference stable IDs.
class DeviceTree : public QTreeWidget {
public:
 explicit DeviceTree(QWidget *parent=nullptr):QTreeWidget(parent){
  setHeaderHidden(true);setDragEnabled(true);setIndentation(18);
  setSelectionMode(QAbstractItemView::SingleSelection);setMinimumHeight(160);
 }
 static QString groupKey(const QUrl &url){if(url.scheme()!="dvrip")return "streams";auto cloud=QUrlQuery(url).queryItemValue("cloudId");return cloud.isEmpty()?QString("local:%1:%2").arg(url.host()).arg(url.port(34567)):"cloud:"+cloud;}
 QUuid deviceId(QTreeWidgetItem *item)const{return item?item->data(0,Qt::UserRole).toUuid():QUuid();}
 void refresh(QListWidget *model,const QVector<QUrl> &urls){
  QSet<QString> collapsed;for(int i=0;i<topLevelItemCount();++i){auto *p=topLevelItem(i);if(!p->isExpanded())collapsed.insert(p->data(0,Qt::UserRole+1).toString());}
  auto selected=deviceId(currentItem());QSignalBlocker block(this);clear();QMap<QString,QTreeWidgetItem*> parents;
  QSignalBlocker modelBlock(model);
  for(int i=0;i<model->count()&&i<urls.size();++i){
   auto *source=model->item(i);if(source->isHidden())continue;const auto &url=urls[i];QString key,title,label=source->text();
   if(url.scheme()=="dvrip"){
    QUrlQuery query(url);auto cloud=query.queryItemValue("cloudId");
    key=groupKey(url);
    title=label.section(" · CH ",0,0);if(title==label)title="Recorder · "+(cloud.isEmpty()?url.host():QString("Cloud"));
    if(label.contains(" · CH "))label="CH "+label.section(" · CH ",1);
   }else{key="streams";title="ONVIF / manual streams";}
   auto *parent=parents.value(key);if(!parent){parent=new QTreeWidgetItem(this,QStringList{title});parent->setData(0,Qt::UserRole+1,key);parent->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);parent->setExpanded(!collapsed.contains(key));parents.insert(key,parent);}
   auto *leaf=new QTreeWidgetItem(parent,QStringList{label});auto id=devices::id(source);leaf->setData(0,Qt::UserRole,id);leaf->setToolTip(0,source->text());leaf->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsDragEnabled);if(id==selected)setCurrentItem(leaf);
  }
  for(auto *parent:parents)parent->setText(0,parent->text(0)+QString(" (%1)").arg(parent->childCount()));
 }
protected:
 void startDrag(Qt::DropActions)override{auto id=deviceId(currentItem());if(id.isNull())return;auto *mime=new QMimeData;mime->setData("application/x-openwatch-camera",id.toString().toUtf8());auto *drag=new QDrag(this);drag->setMimeData(mime);drag->exec(Qt::CopyAction);}
};
