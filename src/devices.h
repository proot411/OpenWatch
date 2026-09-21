#pragma once
#include "cell.h"
namespace devices {
inline QUuid id(QListWidgetItem *item){
 auto value=item->data(Qt::UserRole).toUuid();if(value.isNull()){value=QUuid::createUuid();item->setData(Qt::UserRole,value);}return value;
}
inline int rowForId(QListWidget *list,const QUuid &identity){if(identity.isNull())return -1;for(int i=0;i<list->count();++i)if(id(list->item(i))==identity)return i;return -1;}
inline void remove(QListWidget *list,QVector<QUrl> &urls,const QVector<Cell*> &cells,int row){
 if(row<0 || row>=list->count() || row>=urls.size())return;
 auto identity=id(list->item(row));
 for(auto *cell:cells)if(cell->deviceId==identity)cell->video.requestStop();
 for(auto *cell:cells)if(cell->deviceId==identity){cell->video.clear();cell->deviceId={};cell->name.clear();cell->zoom=1;cell->zoomPan={};cell->update();}
 urls.removeAt(row);delete list->takeItem(row);
}
inline bool edit(QWidget *parent,QString &name,QUrl &source){
 QDialog dialog(parent);dialog.setWindowTitle("Edit device / stream");auto *form=new QFormLayout(&dialog);
 QLineEdit label(name),url(QString::fromUtf8(source.toEncoded(QUrl::RemoveUserInfo))),user(source.userName()),password(source.password());
 label.setObjectName("deviceName");url.setObjectName("deviceUrl");url.setMinimumWidth(470);password.setEchoMode(QLineEdit::Password);
 form->addRow("Name",&label);form->addRow("Stream URL",&url);form->addRow("Username",&user);form->addRow("Password",&password);
 QLabel note("Changing the URL or credentials restarts this entry's open views and stops their recordings.\nFor ONVIF, this edits the resolved stream URL; use Discover devices to reload profiles.");note.setWordWrap(true);form->addRow(&note);
 QLabel error;error.setWordWrap(true);form->addRow(&error);QDialogButtonBox buttons(QDialogButtonBox::Save|QDialogButtonBox::Cancel);form->addRow(&buttons);
 QUrl result;
 QObject::connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{
  result=QUrl(url.text().trimmed());
  if(label.text().trimmed().isEmpty() || !result.isValid() || result.host().isEmpty() || !QStringList{"dvrip","rtsp","rtsps","rtmp","rtmps","http","https"}.contains(result.scheme()) || !result.userInfo().isEmpty()){
   error.setText("Enter a name and supported stream URL. Put credentials in the separate fields.");return;
  }
  if(url.text().trimmed()==QString::fromUtf8(source.toEncoded(QUrl::RemoveUserInfo)) && user.text()==source.userName() && password.text()==source.password())result=source;
  else {result.setUserName(user.text());result.setPassword(password.text());}
  dialog.accept();
 });
 QObject::connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
 if(dialog.exec()!=QDialog::Accepted)return false;name=label.text().trimmed();source=result;return true;
}
}
