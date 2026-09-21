#include "recorderdialog.h"
RecorderDialog::RecorderDialog(QWidget *parent):QDialog(parent) {
 setWindowTitle("Connect XMEye NVR / DVR");setMinimumWidth(480);
 auto *form=new QFormLayout(this);
 auto *connection=new QComboBox;connection->addItem("Local network / VPN (IP address)");connection->addItem("XMEye cloud / serial number — not available yet");
 auto *model=qobject_cast<QStandardItemModel*>(connection->model());model->item(1)->setEnabled(false);
 form->addRow("Connection",connection);
 name=new QLineEdit;name->setPlaceholderText("Optional recorder name");form->addRow("Name",name);
 ip=new QLineEdit;ip->setObjectName("recorderIp");ip->setPlaceholderText("192.168.1.10");form->addRow("Recorder IP",ip);
 port=new QSpinBox;port->setObjectName("recorderPort");port->setRange(1,65535);port->setValue(34567);form->addRow("Port",port);
 user=new QLineEdit("admin");user->setObjectName("recorderUser");form->addRow("Username",user);
 password=new QLineEdit;password->setObjectName("recorderPassword");password->setEchoMode(QLineEdit::Password);form->addRow("Password",password);
 quality=new QComboBox;quality->addItem("Substream — lower bandwidth",true);quality->addItem("Main stream — full quality",false);form->addRow("Stream quality",quality);
 auto *note=new QLabel("Reads the recorder's channel count and opens every channel in the current grid.\nExisting grid streams and recordings will stop after a successful scan.\nUp to 64 channels can play at once. Credentials remain in memory only.");note->setWordWrap(true);form->addRow(note);
 status=new QLabel;status->setObjectName("scanStatus");status->setWordWrap(true);form->addRow(status);
 auto *buttons=new QDialogButtonBox;scan=buttons->addButton("Scan and play all",QDialogButtonBox::ActionRole);scan->setObjectName("scanRecorder");scan->setDefault(true);buttons->addButton(QDialogButtonBox::Cancel);form->addRow(buttons);
 connect(scan,&QPushButton::clicked,this,&RecorderDialog::startScan);connect(buttons,&QDialogButtonBox::rejected,this,&RecorderDialog::reject);
}
RecorderDialog::~RecorderDialog(){cancelled=true;if(worker){worker->wait();delete worker;}}
void RecorderDialog::reject(){cancelled=true;QDialog::reject();}
void RecorderDialog::startScan(){
 if(worker && worker->isRunning())return;
 QHostAddress address;
 if(!address.setAddress(ip->text().trimmed()) || address.isNull() || address.isMulticast() || address==QHostAddress::Any || address==QHostAddress::AnyIPv6 || address==QHostAddress::Broadcast){status->setText("Enter the recorder's IP address. Serial-number cloud access is not implemented yet.");return;}
 if(user->text().isEmpty()){status->setText("Enter a username.");return;}
 QUrl url;url.setScheme("dvrip");url.setHost(address.toString());url.setPort(port->value());url.setUserName(user->text());url.setPassword(password->text());
 if(worker){delete worker;worker=nullptr;}
 cancelled=false;found.clear();error.clear();scan->setEnabled(false);
 for(QWidget *field:QList<QWidget*>{ip,user,password,name,port,quality})field->setEnabled(false);
 status->setText("Connecting locally and reading recorder channels…");bool substream=quality->currentData().toBool();
 worker=QThread::create([this,url,substream]{try{xm::Client client(cancelled);int count=client.scanChannels(url);found=xm::channelUrls(url,count,substream);}catch(const std::exception &e){error=QString::fromUtf8(e.what());}});
 connect(worker,&QThread::finished,this,[this]{
  worker->wait();
  if(cancelled)return;
  scan->setEnabled(true);for(QWidget *field:QList<QWidget*>{ip,user,password,name,port,quality})field->setEnabled(true);
  if(!error.isEmpty()){status->setText(error);return;}
  accept();
 });
 worker->start();
}
