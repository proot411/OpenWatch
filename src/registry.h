#pragma once
#include "devices.h"
#include "vault.h"
namespace registry {
constexpr int GroupRole=Qt::UserRole+1,EndpointRole=Qt::UserRole+2;
inline QJsonArray serialize(QListWidget *list,const QVector<QUrl> &urls){QJsonArray rows;for(int i=0;i<list->count();++i){auto *item=list->item(i);rows.append(QJsonObject{{"id",devices::id(item).toString()},{"name",item->text()},{"url",QString::fromUtf8(urls[i].toEncoded())},{"group",item->data(GroupRole).toString()},{"onvif",item->data(EndpointRole).toString()}});}return rows;}
// Envelope: devices plus the camera grid (size + cell -> device id), so a load can restore the layout.
inline QJsonObject pack(QListWidget *list,const QVector<QUrl> &urls,int grid,const QVector<QUuid> &cells){
 QJsonArray assigned;for(const auto &id:cells)assigned.append(id.toString());
 return QJsonObject{{"devices",serialize(list,urls)},{"layout",QJsonObject{{"grid",grid},{"cells",assigned}}}};
}
// A decrypted .owv payload: the device rows plus the saved camera grid.
// Older files are a bare JSON array; newer files wrap rows in {"devices":...,"layout":...}.
struct Snapshot{QJsonArray devices;int grid=0;QVector<QUuid>cells;};
inline Snapshot unpack(const QByteArray &bytes){
 QJsonParseError error;auto doc=QJsonDocument::fromJson(bytes,&error);Snapshot snapshot;
 if(error.error!=QJsonParseError::NoError)throw std::runtime_error("Invalid device list");
 if(doc.isArray())snapshot.devices=doc.array();
 else if(doc.isObject()&&doc.object()["devices"].isArray()){auto object=doc.object();snapshot.devices=object["devices"].toArray();
  auto layout=object["layout"].toObject();int grid=layout["grid"].toInt();
  if(grid!=0&&!QVector<int>{1,4,9,16,25,36,64}.contains(grid))grid=0;snapshot.grid=grid;
  for(auto entry:layout["cells"].toArray()){QUuid id(entry.toString());snapshot.cells.append(id);if(snapshot.cells.size()>=64)break;}
 }else throw std::runtime_error("Invalid device list");
 if(snapshot.devices.size()>4096)throw std::runtime_error("Invalid device list");
 QSet<QUuid> ids;
 for(auto row:snapshot.devices){auto o=row.toObject();QUrl url(o["url"].toString());QUuid id(o["id"].toString());QUrl control(o["onvif"].toString());
  if(id.isNull()||ids.contains(id)||o["name"].toString().trimmed().isEmpty()||!url.isValid()||url.host().isEmpty()||!QStringList{"dvrip","rtsp","rtsps","rtmp","rtmps","http","https"}.contains(url.scheme())||(!control.isEmpty()&&(!control.isValid()||!QStringList{"http","https"}.contains(control.scheme())||!control.userInfo().isEmpty()||control.host().compare(url.host(),Qt::CaseInsensitive)!=0)))throw std::runtime_error("Invalid device entry");ids.insert(id);
 }
 // A saved cell pointing at a device that is no longer present invalidates the layout restore.
 bool dangling=false;for(const auto &id:snapshot.cells)if(!id.isNull()&&!ids.contains(id)){dangling=true;break;}
 if(dangling){snapshot.grid=0;snapshot.cells.clear();}
 return snapshot;
}
inline QJsonArray validate(const QByteArray &bytes){return unpack(bytes).devices;}
inline bool passphrase(QWidget *parent,QString &secret,bool saving){QDialog dialog(parent);dialog.setWindowTitle(saving?"Protect device list":"Unlock device list");QFormLayout form(&dialog);QLabel note("The file includes camera credentials, encrypted with your passphrase.\nThe passphrase is not saved and cannot be recovered.");form.addRow(&note);QLineEdit first,second;first.setEchoMode(QLineEdit::Password);second.setEchoMode(QLineEdit::Password);form.addRow("Passphrase",&first);if(saving)form.addRow("Confirm passphrase",&second);QLabel error;form.addRow(&error);QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form.addRow(&buttons);QObject::connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);QObject::connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(first.text().isEmpty()||(saving&&(first.text().size()<12||first.text()!=second.text()))){error.setText("Use at least 12 characters and matching passphrases when saving.");return;}dialog.accept();});if(dialog.exec()!=QDialog::Accepted)return false;secret=first.text();return true;}
}
