#include "discoverydialog.h"
#include "dvrip.h"
DiscoveryDialog::DiscoveryDialog(QWidget *parent):QDialog(parent),scanner(this){
 setWindowTitle("Discover cameras and recorders");resize(820,720);
 auto *layout=new QVBoxLayout(this);auto *row=new QHBoxLayout;
 interfaces=new QComboBox;interfaces->addItem("All active IPv4 interfaces",0);
 for(auto iface:QNetworkInterface::allInterfaces())if(iface.flags().testFlag(QNetworkInterface::IsUp)&&!iface.flags().testFlag(QNetworkInterface::IsLoopBack))interfaces->addItem(iface.humanReadableName(),iface.index());
 scan=new QPushButton("Scan LAN (6 seconds)");scan->setObjectName("scanLan");row->addWidget(interfaces,1);row->addWidget(scan);layout->addLayout(row);
 devices=new QTableWidget(0,3);devices->setHorizontalHeaderLabels({"Protocol","Device","Address"});devices->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);devices->setSelectionBehavior(QAbstractItemView::SelectRows);devices->setSelectionMode(QAbstractItemView::SingleSelection);devices->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(devices);
 auto *form=new QFormLayout;kind=new QComboBox;kind->addItems({"ONVIF","XMEye"});address=new QLineEdit;address->setObjectName("discoveryAddress");address->setPlaceholderText("ONVIF service URL, or dvrip://IP:34567 for XM");username=new QLineEdit("admin");username->setObjectName("discoveryUser");password=new QLineEdit;password->setObjectName("discoveryPassword");password->setEchoMode(QLineEdit::Password);
 form->addRow("Protocol",kind);form->addRow("Device address",address);form->addRow("Username",username);form->addRow("Password",password);layout->addLayout(form);
 auto *note=new QLabel("Select a discovered device or enter its address manually. ONVIF may require a separate account enabled on the recorder. Credentials stay in memory.");note->setWordWrap(true);layout->addWidget(note);
 load=new QPushButton("Load channels / profiles");load->setObjectName("loadDiscoveryProfiles");layout->addWidget(load);
 profiles=new QListWidget;profiles->setObjectName("discoveryProfiles");layout->addWidget(profiles);
 auto *selection=new QHBoxLayout;for(bool checked:{true,false}){auto *b=new QPushButton(checked?"Select all":"Clear selection");selection->addWidget(b);connect(b,&QPushButton::clicked,this,[this,checked]{if(worker&&worker->isRunning())return;for(int i=0;i<profiles->count();++i)profiles->item(i)->setCheckState(checked?Qt::Checked:Qt::Unchecked);});}layout->addLayout(selection);
 status=new QLabel("Scan finds responding XM and ONVIF devices on your local IPv4 network.");status->setObjectName("discoveryStatus");status->setWordWrap(true);layout->addWidget(status);
 auto *buttons=new QDialogButtonBox;open=buttons->addButton("Open selected streams",QDialogButtonBox::ActionRole);open->setObjectName("openDiscoveryStreams");open->setEnabled(false);buttons->addButton(QDialogButtonBox::Cancel);layout->addWidget(buttons);
 connect(scan,&QPushButton::clicked,this,[this]{devices->setRowCount(0);discovered.clear();scanWarning.clear();scan->setEnabled(false);status->setText("Scanning XM broadcast and ONVIF multicast…");scanner.start(interfaces->currentData().toInt());});
 connect(&scanner,&lan::Scanner::found,this,[this](lan::Device d){int row=devices->rowCount();devices->insertRow(row);devices->setItem(row,0,new QTableWidgetItem(d.kind));devices->setItem(row,1,new QTableWidgetItem(d.name));devices->setItem(row,2,new QTableWidgetItem(d.endpoint.toString()));discovered.append(d);});
 connect(&scanner,&lan::Scanner::status,this,[this](QString message){scanWarning=message;status->setText(message);});
 connect(&scanner,&lan::Scanner::finished,this,[this]{scan->setEnabled(true);status->setText(QString("Scan finished: %1 service(s). If missing, check ONVIF/discovery settings, network isolation and firewall, or enter the service address manually.").arg(discovered.size())+" "+scanWarning);});
 connect(devices,&QTableWidget::currentCellChanged,this,[this](int row,int,int,int){if(row<0||row>=discovered.size())return;kind->setCurrentText(discovered[row].kind);address->setText(discovered[row].endpoint.toString());});
 auto invalidate=[this]{available.clear();profiles->clear();open->setEnabled(false);};
 for(auto *field:{address,username,password})connect(field,&QLineEdit::textChanged,this,invalidate);connect(kind,&QComboBox::currentIndexChanged,this,invalidate);
 connect(load,&QPushButton::clicked,this,&DiscoveryDialog::loadProfiles);connect(open,&QPushButton::clicked,this,&DiscoveryDialog::openStreams);connect(buttons,&QDialogButtonBox::rejected,this,&DiscoveryDialog::reject);
}
DiscoveryDialog::~DiscoveryDialog(){cancelled=true;scanner.stop();if(worker){worker->wait();delete worker;}}
void DiscoveryDialog::reject(){cancelled=true;scanner.stop();QDialog::reject();}
void DiscoveryDialog::busy(bool value){for(QWidget *w:QList<QWidget*>{interfaces,devices,kind,address,username,password,load,scan,profiles})w->setEnabled(!value);open->setEnabled(!value&&!available.isEmpty());}
void DiscoveryDialog::launch(std::function<void()> job,std::function<void()> done){
 if(worker&&worker->isRunning())return;scanner.stop();if(worker){delete worker;worker=nullptr;}cancelled=false;error.clear();busy(true);
 worker=QThread::create([this,job]{try{job();}catch(const std::exception &e){error=e.what();}});
 connect(worker,&QThread::finished,this,[this,done]{worker->wait();if(cancelled)return;busy(false);if(!error.isEmpty()){status->setText(error);return;}done();});worker->start();
}
void DiscoveryDialog::loadProfiles(){
 auto url=QUrl(address->text().trimmed());bool xm=kind->currentText()=="XMEye";
 if(!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty() || (xm?url.scheme()!="dvrip":!QStringList{"http","https"}.contains(url.scheme()))){status->setText("Enter dvrip://IP:port for XMEye or http(s)://IP:port/onvif/device_service for ONVIF. Use the separate credential fields.");return;}
 available.clear();profiles->clear();selectedDevice=url;QString user=username->text(),pass=password->text();status->setText("Authenticating and loading available channels / profiles…");
 launch([this,url,user,pass,xm]{if(xm){auto source=url;source.setUserName(user);source.setPassword(pass);xm::Client client(cancelled);auto urls=xm::channelUrls(source,client.scanChannels(source),true);for(int i=0;i<urls.size();++i){lan::Profile p;p.name="CH "+QString::number(i+1)+" · substream";p.uri=urls[i];available.append(p);auto main=p;main.name="CH "+QString::number(i+1)+" · main stream";QUrlQuery query(main.uri);query.removeAllQueryItems("subtype");query.addQueryItem("subtype","0");main.uri.setQuery(query);available.append(main);}}
  else {lan::Onvif client(url,user,pass,cancelled);available=client.profiles(url);timeOffset=client.timeOffset();}},[this,xm]{QSet<QString> sources;for(int i=0;i<available.size();++i){auto p=available[i];auto *item=new QListWidgetItem(p.name,profiles);bool checked=xm?(i%2==0&&i<128):(!sources.contains(p.source)&&sources.size()<64);sources.insert(p.source);item->setCheckState(checked?Qt::Checked:Qt::Unchecked);}open->setEnabled(!available.isEmpty());status->setText(QString("%1 profiles loaded. Select up to 64 streams; opening replaces the current grid and stops its recordings.").arg(available.size()));});
}
void DiscoveryDialog::openStreams(){
 QVector<lan::Profile> chosen;for(int i=0;i<profiles->count();++i)if(profiles->item(i)->checkState()==Qt::Checked)chosen.append(available[i]);
 if(chosen.isEmpty()||chosen.size()>64){status->setText("Select between 1 and 64 streams.");return;}
 streams.clear();titles.clear();warning.clear();QString user=username->text(),pass=password->text();auto url=selectedDevice;status->setText("Resolving selected stream addresses…");
 launch([this,chosen,user,pass,url]{lan::Onvif client(url,user,pass,cancelled);client.setTimeOffset(timeOffset);int failed=0;QSet<QString> seen;
  for(auto p:chosen){if(cancelled)throw std::runtime_error("Cancelled");try{auto stream=p.uri.isEmpty()?client.stream(p):p.uri;auto key=stream.toString();if(seen.contains(key))continue;seen.insert(key);streams.append(stream);titles.append(url.host()+" · "+p.name);}catch(const std::exception &e){if(cancelled)throw;++failed;warning=e.what();}}
  if(streams.isEmpty())throw std::runtime_error(warning.isEmpty()?"No playable stream addresses returned":warning.toStdString());
  if(failed)warning=QString("%1 profile(s) failed: ").arg(failed)+warning;
 },[this]{if(!warning.isEmpty())QMessageBox::warning(this,"Some profiles could not be opened",warning);accept();});
}
