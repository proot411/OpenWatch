#pragma once
#include <QtWidgets>
#include <functional>
inline void installPtzKeys(QWidget *window,std::function<void(QString)> action){
 const QList<QPair<QString,QString>> bindings{{"Alt+Up","up"},{"Alt+Down","down"},{"Alt+Left","left"},{"Alt+Right","right"},{"Alt+PgUp","zoomIn"},{"Alt+PgDown","zoomOut"},{"Alt+End","stop"}};
 for(const auto &binding:bindings){auto *key=new QShortcut(QKeySequence(binding.first),window);key->setAutoRepeat(false);QObject::connect(key,&QShortcut::activated,window,[=]{if(!QApplication::activeModalWidget())action(binding.second);});}
}
