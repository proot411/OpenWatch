#pragma once
#include <QtWidgets>
#include "dvrip.h"
class RecorderDialog : public QDialog {
 Q_OBJECT
 QLineEdit *ip,*user,*password,*name;
 QSpinBox *port;
 QComboBox *quality,*connection,*cloudChannels;
 QLabel *addressLabel,*note;
 QLabel *status;
 QPushButton *scan;
 QThread *worker=nullptr;
 std::atomic_bool cancelled{false};
 QVector<QUrl> found;
 QString error;
 void startScan();
public:
 explicit RecorderDialog(QWidget *parent=nullptr);
 ~RecorderDialog() override;
 QVector<QUrl> channels() const {return found;}
 bool isCloud()const{return connection->currentIndex()==1;}
 QString recorderName() const {return name->text().trimmed().isEmpty()?ip->text().trimmed():name->text().trimmed();}
 void reject() override;
};
