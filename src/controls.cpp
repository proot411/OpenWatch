#include "controls.h"
#include "dvrip.h"
#include "ptzkeys.h"
namespace {
QString token(const QString &name,const QString &value){return "<m:"+name+">"+value.toHtmlEscaped()+"</m:"+name+">";}
bool normalized(const QDomDocument &doc,const QString &element,const QString &uri,bool two){
 for(auto e:lan::elements(doc,element)){
  if(lan::value(e,"URI")!=uri)continue;
  auto ranges=lan::elements(e,"XRange");if(two)ranges+=lan::elements(e,"YRange");
  if(ranges.size()!=(two?2:1))continue;bool valid=true;
  for(auto r:ranges){bool a=false,b=false;double min=lan::value(r,"Min").toDouble(&a),max=lan::value(r,"Max").toDouble(&b);if(!a||!b||min>-.2||max<.2)valid=false;}if(valid)return true;
 }return false;
}
}
CameraControls::CameraControls(const QString &name,const QUrl &url,const QString &endpoint,QWidget *parent):QDialog(parent),source(url),xmMode(url.scheme()=="dvrip"){
 loaded=xmMode;installPtzKeys(this,[this](QString action){keyboard(action);});
 setWindowTitle("Camera controls · "+name);resize(510,470);auto *layout=new QVBoxLayout(this);
 address=new QLineEdit(endpoint);if(address->text().isEmpty()){QUrl device;device.setScheme("http");device.setHost(url.host());device.setPath("/onvif/device_service");address->setText(device.toString());}
 connect(address,&QLineEdit::textEdited,this,[this]{loaded=false;});
 auto *connectButton=new QPushButton("Load ONVIF controls",this);profiles=new QComboBox(this);presets=new QComboBox(this);
 if(!xmMode){layout->addWidget(new QLabel("ONVIF device service URL (same camera host)"));layout->addWidget(address);layout->addWidget(connectButton);layout->addWidget(profiles);}else {address->hide();profiles->hide();connectButton->hide();}
 auto *note=new QLabel(xmMode?"XM controls use the selected stream's channel. Fixed cameras may reject PTZ.\nEach direction click makes one short movement; firmware support varies.":"Load profiles, then choose the camera profile to control.\nMovement is enabled only for advertised standard velocity spaces.");note->setWordWrap(true);layout->addWidget(note);
 compatibility=new QCheckBox("Compatibility mode: try PTZ with incomplete camera profiles",this);
 compatibility->setToolTip("Uses the selected profile and camera-default velocity space. Enable only for a camera known to support PTZ. Does not modify the camera configuration.");
 if(!xmMode)layout->addWidget(compatibility);else compatibility->hide();
 connect(compatibility,&QCheckBox::toggled,this,[this]{capabilities();});
 layout->addWidget(new QLabel("Keyboard: Alt + arrows · Alt + Page Up/Down: zoom · Alt + End: stop\nLoaded controls remain available in the workspace after closing this window."));
 actions=new QWidget;auto *grid=new QGridLayout(actions);layout->addWidget(actions);actions->setEnabled(xmMode);
 auto move=[&](QString label,QString cmd,int row,int col,int x,int y,int z){auto *b=new QPushButton(label);b->setProperty("axis",z?"zoom":"pan");grid->addWidget(b,row,col);connect(b,&QPushButton::clicked,this,[=]{command(cmd,x,y,z);});};
 move("↑","DirectionUp",0,1,0,1,0);move("←","DirectionLeft",1,0,-1,0,0);move("↓","DirectionDown",2,1,0,-1,0);move("→","DirectionRight",1,2,1,0,0);
 auto *stop=new QPushButton("Stop");grid->addWidget(stop,1,1);connect(stop,&QPushButton::clicked,this,[=]{command("DirectionUp",0,0,0,"stop");});
 move("Zoom +","ZoomWide",3,0,0,0,1);move("Zoom −","ZoomTile",3,2,0,0,-1);
 speed=new QSpinBox(this);speed->setRange(1,8);speed->setValue(2);if(xmMode){grid->addWidget(new QLabel("XM speed"),4,0);grid->addWidget(speed,4,1);
 for(auto pair:{qMakePair(QString("Focus near"),QString("FocusNear")),qMakePair(QString("Focus far"),QString("FocusFar")),qMakePair(QString("Iris close"),QString("IrisSmall")),qMakePair(QString("Iris open"),QString("IrisLarge"))}){auto *b=new QPushButton(pair.first);grid->addWidget(b,5+(pair.second.startsWith("Iris")?1:0),pair.second.endsWith("Far")||pair.second.endsWith("Large")?2:0);connect(b,&QPushButton::clicked,this,[=]{command(pair.second,1);});}
 }else speed->hide();
 presetNumber=new QSpinBox(this);presetNumber->setRange(1,255);grid->addWidget(new QLabel("Preset"),7,0);grid->addWidget(xmMode?static_cast<QWidget*>(presetNumber):presets,7,1);if(xmMode)presets->hide();else presetNumber->hide();
 int column=0;for(auto pair:{qMakePair(QString("Go to"),QString("GotoPreset")),qMakePair(QString("Save position"),QString("SetPreset")),qMakePair(QString("Delete preset"),QString("RemovePreset"))}){auto *b=new QPushButton(pair.first);grid->addWidget(b,8,column++);connect(b,&QPushButton::clicked,this,[=]{if(pair.second=="RemovePreset" && QMessageBox::question(this,"Delete preset","Delete this camera preset?")!=QMessageBox::Yes)return;command(pair.second=="RemovePreset"?"ClearPreset":pair.second,0,0,0,pair.second);});}
 status=new QLabel("Ready");status->setWordWrap(true);layout->addWidget(status);auto *close=new QPushButton("Close");layout->addWidget(close);connect(close,&QPushButton::clicked,this,&QDialog::accept);
 auto *info=new QPushButton("Device information");layout->insertWidget(layout->indexOf(status),info);
 connect(info,&QPushButton::clicked,this,[this]{auto result=std::make_shared<QString>();auto endpoint=QUrl(address->text().trimmed());job([=]{if(xmMode){xm::Client client(cancel);client.controlLogin(source);auto details=client.control(1020,"SystemInfo").value("SystemInfo").toObject();QStringList lines;for(auto name:{"DeviceType","HardWare","SoftWareVersion","SerialNo","VideoInChannel","DigChannel","BuildTime"})if(details.contains(name))lines.append(QString(name)+": "+details[name].toVariant().toString());*result=lines.isEmpty()?"No recognized device information returned":lines.join("\n");}else {lan::Onvif client(source,source.userName(),source.password(),cancel);client.setTimeOffset(offset);auto doc=client.deviceInfo(endpoint);QStringList lines;for(auto name:{"Manufacturer","Model","FirmwareVersion","SerialNumber","HardwareId"})lines.append(QString(name)+": "+lan::value(doc,name));*result=lines.join("\n");}},[=]{QMessageBox::information(this,"Device information",*result);});});
 if(xmMode){auto *sync=new QPushButton("Set recorder time from this computer");layout->insertWidget(layout->indexOf(status),sync);connect(sync,&QPushButton::clicked,this,[this]{if(QMessageBox::question(this,"Set recorder time","Set the whole recorder to this computer's local date and time? This changes timestamps for all channels. Use this only when both use the same time zone.")!=QMessageBox::Yes)return;job([this]{xm::Client client(cancel);client.controlLogin(source);client.control(1450,"OPTimeSetting",QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));});});}
 connect(connectButton,&QPushButton::clicked,this,[=]{QUrl device(address->text().trimmed());if(!device.isValid()||!QStringList{"http","https"}.contains(device.scheme())||device.host().compare(source.host(),Qt::CaseInsensitive)!=0||!device.userInfo().isEmpty()){status->setText("Enter an HTTP(S) device service URL on the stream's host, without credentials.");return;}
  loaded=false;actions->setEnabled(false);job([=]{lan::Onvif client(device,source.userName(),source.password(),cancel);available=client.profiles(device);service=client.ptzService(device);offset=client.timeOffset();},[=]{QSignalBlocker block(profiles);profiles->clear();for(auto p:available)profiles->addItem(p.name);QTimer::singleShot(0,this,[this]{capabilities();});});
 });
 connect(profiles,&QComboBox::currentIndexChanged,this,[=]{capabilities();});
}
CameraControls::~CameraControls(){cancel=true;if(worker){worker->wait();delete worker;}}
void CameraControls::job(std::function<void()> work,std::function<void()> done){
 if(worker)return;error.clear();setEnabled(false);status->setText("Contacting camera…");cancel=false;
 worker=QThread::create([this,work]{try{work();}catch(const std::exception &e){error=QString::fromUtf8(e.what());}});
 connect(worker,&QThread::finished,this,[this,done]{worker->wait();delete worker;worker=nullptr;setEnabled(true);status->setText(error.isEmpty()?"Command completed":error);if(error.isEmpty()&&done)done();if(report)report(error.isEmpty()?"PTZ command completed":error);});worker->start();
}
void CameraControls::capabilities(){
 int i=profiles->currentIndex();if(i<0||i>=available.size()||worker)return;loaded=false;auto p=available[i];bool legacy=compatibility->isChecked();actions->setEnabled(false);presets->clear();pan=zoom=false;moveTimeout=legacy?"PT1S":"PT0.2S";
 job([=]{if(legacy){pan=zoom=true;return;}lan::Onvif client(source,source.userName(),source.password(),cancel);client.setTimeOffset(offset);
 QString config=p.ptzConfig;
 if(config.isEmpty()){
  try{auto compatible=client.ptz(service,"GetCompatibleConfigurations",token("ProfileToken",p.token));QSet<QString> tokens;for(auto e:lan::elements(compatible,"PTZConfiguration"))if(!e.attribute("token").isEmpty())tokens.insert(e.attribute("token"));if(tokens.size()==1)config=*tokens.begin();}catch(const std::exception &){if(cancel)throw;}
  if(config.isEmpty())throw std::runtime_error("The profile has no usable PTZ configuration. Try another profile, or enable Compatibility mode if this camera supports PTZ in TinyCam.");
 }
 auto doc=client.ptz(service,"GetConfigurationOptions",token("ConfigurationToken",config));pan=normalized(doc,"ContinuousPanTiltVelocitySpace","http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace",true);zoom=normalized(doc,"ContinuousZoomVelocitySpace","http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace",false);
 auto ranges=lan::elements(doc,"PTZTimeout");
 if(!ranges.isEmpty()){
  auto seconds=[](QString value){auto match=QRegularExpression("^PT([0-9]+(?:\\.[0-9]+)?)S$").match(value);return match.hasMatch()?match.captured(1).toDouble():-1.;};
  double minimum=seconds(lan::value(ranges.first(),"Min")),maximum=seconds(lan::value(ranges.first(),"Max"));
  if(minimum<0||maximum<=0||minimum>maximum||minimum>10)throw std::runtime_error("Unsupported PTZ timeout range; use Compatibility mode only if this camera supports short moves.");
  moveTimeout=QString("PT%1S").arg(qBound(minimum,.2,maximum));
 }
 },[=]{loaded=true;actions->setEnabled(true);for(auto *b:actions->findChildren<QPushButton*>()){auto axis=b->property("axis").toString();if(!axis.isEmpty())b->setEnabled(axis=="pan"?pan:zoom);}QTimer::singleShot(0,this,[this]{command({},0,0,0,"GetPresets");});});
}
void CameraControls::command(QString xmCommand,int x,int y,int z,QString presetAction){
 int i=profiles->currentIndex();if(!xmMode&&(i<0||i>=available.size()))return;
 QString timeout=moveTimeout;bool legacy=!xmMode&&compatibility->isChecked();auto profile=xmMode?QString():available[i].token;auto chosen=presets->currentData().toString();int number=presetNumber->value(),step=speed->value();
 if(!xmMode&&(presetAction=="GotoPreset"||presetAction=="RemovePreset")&&chosen.isEmpty()){status->setText("Select a preset first.");return;}
 QString presetName;if(!xmMode&&presetAction=="SetPreset"){bool ok=false;presetName=QInputDialog::getText(this,"Save preset","New preset name",QLineEdit::Normal,{},&ok).trimmed();if(!ok||presetName.isEmpty())return;}
 auto result=std::make_shared<QVector<QPair<QString,QString>>>();
 job([=]{if(xmMode){std::atomic_bool keepRunning{false};xm::Client client(keepRunning);client.controlLogin(source);QUrlQuery query(source);bool valid=true;auto raw=query.queryItemValue("channel");int channel=raw.isEmpty()?0:raw.toInt(&valid);if(!valid||channel<0||channel>255)throw std::runtime_error("Invalid channel");
  if(presetAction=="stop"){client.ptz(channel,"DirectionUp",-1,step);return;}
  if(!presetAction.isEmpty()){client.ptz(channel,xmCommand,number,step);return;}
  std::exception_ptr failure;try{client.ptz(channel,xmCommand,65535,step);QThread::msleep(200);}catch(...){failure=std::current_exception();}
  // Always attempt stop, including when the move acknowledgement was lost.
  try{client.ptz(channel,xmCommand,-1,step);}catch(...){throw std::runtime_error("Camera stop was not confirmed. Check the camera and use its native controls if it is still moving.");}if(failure)std::rethrow_exception(failure);
 }else {std::atomic_bool keepRunning{false};lan::Onvif client(source,source.userName(),source.password(),keepRunning);client.setTimeOffset(offset);auto body=token("ProfileToken",profile);
  if(presetAction=="stop"){client.ptz(service,"Stop",body+"<m:PanTilt>true</m:PanTilt><m:Zoom>true</m:Zoom>");return;}
  if(presetAction.isEmpty()){
   QString velocity=z?QString("<tt:Zoom x=\"%1\" space=\"http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace\"/>").arg(z*.2):QString("<tt:PanTilt x=\"%1\" y=\"%2\" space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace\"/>").arg(x*.2).arg(y*.2);
   if(legacy)velocity.remove(QRegularExpression(" space=\"[^\"]*\""));
   std::exception_ptr failure;
   try{client.ptz(service,"ContinuousMove",body+"<m:Velocity>"+velocity+"</m:Velocity>"+token("Timeout",timeout));QThread::msleep(200);}catch(...){failure=std::current_exception();}
   // Cleanup also runs after a lost acknowledgement or closing the dialog.
   try{client.ptz(service,"Stop",body+"<m:PanTilt>true</m:PanTilt><m:Zoom>true</m:Zoom>");}catch(...){throw std::runtime_error("Camera stop was not confirmed. Check the camera and use its native controls if it is still moving.");}
   if(failure)std::rethrow_exception(failure);return;
  }
  if(presetAction=="SetPreset")client.ptz(service,presetAction,body+token("PresetName",presetName));
  else if(presetAction!="GetPresets")client.ptz(service,presetAction,body+token("PresetToken",chosen));
  auto doc=client.ptz(service,"GetPresets",body);for(auto e:lan::elements(doc,"Preset"))result->append({lan::value(e,"Name"),e.attribute("token")});
 }},[=]{if(!xmMode&&!presetAction.isEmpty()&&presetAction!="stop"){presets->clear();for(auto p:*result)presets->addItem(p.first.isEmpty()?p.second:p.first,p.second);}});
}

void CameraControls::keyboard(const QString &action){
 auto inform=[this](QString text){status->setText(text);if(report)report(text);};
 if(worker){inform("PTZ is busy; wait for the current short movement to finish.");return;}
 if(!loaded){inform("Load this camera's PTZ controls first.");return;}
 if(!xmMode&&action!="stop"&&((action.startsWith("zoom")&&!zoom)||(!action.startsWith("zoom")&&!pan))){inform("This camera does not advertise this movement.");return;}
 if(action=="up")command("DirectionUp",0,1);
 else if(action=="down")command("DirectionDown",0,-1);
 else if(action=="left")command("DirectionLeft",-1,0);
 else if(action=="right")command("DirectionRight",1,0);
 else if(action=="zoomIn")command("ZoomWide",0,0,1);
 else if(action=="zoomOut")command("ZoomTile",0,0,-1);
 else if(action=="stop")command("DirectionUp",0,0,0,"stop");
}
