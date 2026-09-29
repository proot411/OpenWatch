#include "devicetree.h"
#include <QtTest>
int main(int argc,char **argv){
 QApplication app(argc,argv);QListWidget model;QVector<QUrl> urls;DeviceTree tree;
 auto add=[&](QString name,QString url){urls.append(QUrl(url));model.addItem(name);};
 add("Recorder 1 · CH 1","dvrip://192.0.2.1?channel=0");add("Recorder 1 · CH 2","dvrip://192.0.2.1?channel=1");
 add("Recorder 2 · CH 1","dvrip://xmeye-cloud?cloudId=example-one&channel=0");
 add("Recorder 3 · CH 1","dvrip://xmeye-cloud?cloudId=example-two&channel=0");
 add("Garden","rtsp://192.0.2.9/live");
 tree.refresh(&model,urls);
 if(tree.topLevelItemCount()!=4||tree.topLevelItem(0)->childCount()!=2)return 1;
 auto id=devices::id(model.item(1));tree.setCurrentItem(tree.topLevelItem(0)->child(1));tree.topLevelItem(0)->setExpanded(false);
 tree.refresh(&model,urls);if(tree.deviceId(tree.currentItem())!=id||tree.topLevelItem(0)->isExpanded())return 2;
 urls.removeAt(0);delete model.takeItem(0);tree.refresh(&model,urls);
 if(tree.deviceId(tree.currentItem())!=id||devices::rowForId(&model,id)!=0)return 3;
 model.item(0)->setHidden(true);tree.refresh(&model,urls);if(tree.topLevelItemCount()!=3)return 4;
 for(int i=0;i<tree.topLevelItemCount();++i)if(!tree.deviceId(tree.topLevelItem(i)).isNull())return 5;
 return 0;
}
