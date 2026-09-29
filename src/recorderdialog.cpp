#include "recorderdialog.h"
RecorderDialog::RecorderDialog(QWidget *parent):QDialog(parent) {
 setWindowTitle("Connect XMEye NVR / DVR");setMinimumWidth(480);
 auto *form=new QFormLayout(this);
 connection=new QComboBox;connection->setObjectName("connectionMode");connection->addItem("Local network / VPN (IP address)");connection->addItem("XMEye cloud / serial number — experimental relay");
 form->addRow("Connection",connection);
 name=new QLineEdit;name->setPlaceholderText("Optional recorder name");form->addRow("Name",name);
 ip=new QLineEdit;ip->setObjectName("recorderIp");ip->setPlaceholderText("192.168.1.10");addressLabel=new QLabel("Recorder IP");form->addRow(addressLabel,ip);
 port=new QSpinBox;port->setObjectName("recorderPort");port->setRange(1,65535);port->setValue(34567);form->addRow("Port",port);
 user=new QLineEdit("admin");user->setObjectName("recorderUser");form->addRow("Username",user);
 password=new QLineEdit;password->setObjectName("recorderPassword");password->setEchoMode(QLineEdit::Password);form->addRow("Password",password);
 quality=new QComboBox;quality->addItem("Substream — lower bandwidth",true);quality->addItem("Main stream — full quality",false);form->addRow("Stream quality",quality);
 note=new QLabel("Reads the recorder's channel count and opens every channel in the current grid.\nExisting grid streams and recordings will stop after a successful scan.\nUp to 64 channels can play at once. Credentials remain in memory only.");note->setWordWrap(true);form->addRow(note);
 cloudChannels=new QComboBox;cloudChannels->addItems({"Channel 1 — first connection test","All detected channels"});cloudChannels->setVisible(false);form->addRow(cloudChannels);
 connect(connection,&QComboBox::currentIndexChanged,this,[this](int index){bool cloud=index==1;addressLabel->setText(cloud?"Serial number":"Recorder IP");ip->setPlaceholderText(cloud?"Recorder CloudID / serial number":"192.168.1.10");port->setEnabled(!cloud);cloudChannels->setVisible(cloud);scan->setText(cloud?"Connect through cloud":"Scan and play all");note->setText(cloud?"Experimental XMEye relay connection. Sends your serial number to XMEye servers and uses encrypted device login. No local/VPN fallback. First test: channel 1, substream. Live video is experimental and still needs testing on your recorder.":"Reads the recorder's channel count and opens every channel in the current grid. Existing streams stop after a successful scan. Credentials remain in memory only.");});
 status=new QLabel;status->setObjectName("scanStatus");status->setWordWrap(true);form->addRow(status);
 auto *buttons=new QDialogButtonBox;scan=buttons->addButton("Scan and play all",QDialogButtonBox::ActionRole);scan->setObjectName("scanRecorder");scan->setDefault(true);buttons->addButton(QDialogButtonBox::Cancel);form->addRow(buttons);
 connect(scan,&QPushButton::clicked,this,&RecorderDialog::startScan);connect(buttons,&QDialogButtonBox::rejected,this,&RecorderDialog::reject);
}
RecorderDialog::~RecorderDialog(){cancelled=true;if(worker){worker->wait();delete worker;}}
void RecorderDialog::reject(){cancelled=true;QDialog::reject();}
void RecorderDialog::startScan(){
 if(worker && worker->isRunning())return;
 QHostAddress address;
 if(!isCloud()&&(!address.setAddress(ip->text().trimmed()) || address.isNull() || address.isMulticast() || address==QHostAddress::Any || address==QHostAddress::AnyIPv6 || address==QHostAddress::Broadcast)){status->setText("Enter the recorder's IP address. Serial-number access requires the experimental cloud connection option.");return;}
 if(isCloud()&&!xm::cloud::validSerial(ip->text().trimmed())){status->setText("Enter a valid serial number (8–64 letters, numbers, hyphens or underscores).");return;}
 if(user->text().isEmpty()){status->setText("Enter a username.");return;}
 QUrl url;url.setScheme("dvrip");url.setHost(isCloud()?"xmeye-cloud":address.toString());if(isCloud()){QUrlQuery q;q.addQueryItem("cloudId",ip->text().trimmed());url.setQuery(q);}url.setPort(port->value());url.setUserName(user->text());url.setPassword(password->text());
 if(worker){delete worker;worker=nullptr;}
 cancelled=false;found.clear();error.clear();scan->setEnabled(false);
 for(QWidget *field:QList<QWidget*>{ip,user,password,name,port,quality,connection,cloudChannels})field->setEnabled(false);
 status->setText(isCloud()?"Starting experimental cloud connection…":"Connecting locally and reading recorder channels…");bool substream=quality->currentData().toBool();bool firstOnly=isCloud()&&cloudChannels->currentIndex()==0;
 worker=QThread::create([this,url,substream,firstOnly]{try{xm::Client client(cancelled,[this](const QString &stage){QMetaObject::invokeMethod(this,[this,stage]{if(!cancelled)status->setText(stage);},Qt::QueuedConnection);});int count=client.scanChannels(url);found=xm::channelUrls(url,firstOnly?1:count,substream);}catch(const std::exception &e){error=QString::fromUtf8(e.what());}});
 connect(worker,&QThread::finished,this,[this]{
  worker->wait();
  if(cancelled)return;
  scan->setEnabled(true);for(QWidget *field:QList<QWidget*>{ip,user,password,name,port,quality,connection,cloudChannels})field->setEnabled(true);
  port->setEnabled(!isCloud());
  if(!error.isEmpty()){status->setText(error);return;}
  accept();
 });
 worker->start();
}
