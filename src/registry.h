#pragma once
#include "devices.h"
#include "vault.h"
namespace registry {
constexpr int GroupRole=Qt::UserRole+1,EndpointRole=Qt::UserRole+2;
inline QJsonArray serialize(QListWidget *list,const QVector<QUrl> &urls){QJsonArray rows;for(int i=0;i<list->count();++i){auto *item=list->item(i);rows.append(QJsonObject{{"id",devices::id(item).toString()},{"name",item->text()},{"url",QString::fromUtf8(urls[i].toEncoded())},{"group",item->data(GroupRole).toString()},{"onvif",item->data(EndpointRole).toString()}});}return rows;}
inline QJsonArray validate(const QByteArray &bytes){
 QJsonParseError error;auto doc=QJsonDocument::fromJson(bytes,&error);if(error.error!=QJsonParseError::NoError||!doc.isArray()||doc.array().size()>4096)throw std::runtime_error("Invalid device list");QSet<QUuid> ids;
 for(auto row:doc.array()){auto o=row.toObject();QUrl url(o["url"].toString());QUuid id(o["id"].toString());QUrl control(o["onvif"].toString());
  if(id.isNull()||ids.contains(id)||o["name"].toString().trimmed().isEmpty()||!url.isValid()||url.host().isEmpty()||!QStringList{"dvrip","rtsp","rtsps","rtmp","rtmps","http","https"}.contains(url.scheme())||(!control.isEmpty()&&(!control.isValid()||!QStringList{"http","https"}.contains(control.scheme())||!control.userInfo().isEmpty()||control.host().compare(url.host(),Qt::CaseInsensitive)!=0)))throw std::runtime_error("Invalid device entry");ids.insert(id);
 }return doc.array();
}
inline bool passphrase(QWidget *parent,QString &secret,bool saving){QDialog dialog(parent);dialog.setWindowTitle(saving?"Protect device list":"Unlock device list");QFormLayout form(&dialog);QLabel note("The file includes camera credentials, encrypted with your passphrase.\nThe passphrase is not saved and cannot be recovered.");form.addRow(&note);QLineEdit first,second;first.setEchoMode(QLineEdit::Password);second.setEchoMode(QLineEdit::Password);form.addRow("Passphrase",&first);if(saving)form.addRow("Confirm passphrase",&second);QLabel error;form.addRow(&error);QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form.addRow(&buttons);QObject::connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);QObject::connect(&buttons,&QDialogButtonBox::accepted,&dialog,[&]{if(first.text().isEmpty()||(saving&&(first.text().size()<12||first.text()!=second.text()))){error.setText("Use at least 12 characters and matching passphrases when saving.");return;}dialog.accept();});if(dialog.exec()!=QDialog::Accepted)return false;secret=first.text();return true;}
}
