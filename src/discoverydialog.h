#pragma once
#include <QtWidgets>
#include "discovery.h"
class DiscoveryDialog:public QDialog {
 Q_OBJECT
 lan::Scanner scanner;
 QComboBox *interfaces,*kind;
 QTableWidget *devices;
 QLineEdit *address,*username,*password;
 QListWidget *profiles;
 QLabel *status;
 QPushButton *scan,*load,*open;
 QVector<lan::Device> discovered;
 QVector<lan::Profile> available;
 QVector<QUrl> streams;
 QStringList titles;
 QString error,warning,scanWarning;
 QUrl selectedDevice;
 qint64 timeOffset=0;
 QThread *worker=nullptr;
 std::atomic_bool cancelled{false};
 void busy(bool value);
 void launch(std::function<void()> job,std::function<void()> done);
 void loadProfiles();
 void openStreams();
public:
 explicit DiscoveryDialog(QWidget *parent=nullptr);
 ~DiscoveryDialog()override;
 void reject()override;
 QUrl deviceEndpoint()const{return selectedDevice;}
 QVector<QUrl> urls()const{return streams;}
 QStringList names()const{return titles;}
 QString warnings()const{return warning;}
};
