#include <QtWidgets>
#include <cmath>
#include "cell.h"
#include "devices.h"
#include "registry.h"
#include "controls.h"
#include "ptzkeys.h"
#include "navigation.h"
#include "recorderdialog.h"
#include "discoverydialog.h"
extern "C" {
#include <libavutil/log.h>
}
class CameraList:public QListWidget {
 void startDrag(Qt::DropActions)override{if(currentRow()<0)return;auto *mime=new QMimeData;mime->setData("application/x-openwatch-camera",devices::id(currentItem()).toString().toUtf8());auto *drag=new QDrag(this);drag->setMimeData(mime);drag->exec(Qt::CopyAction);}
};
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setApplicationName("OpenWatch");app.setOrganizationName("OpenWatch");av_log_set_level(AV_LOG_QUIET);
 QMainWindow window;window.setWindowTitle("OpenWatch • Video workspace");window.resize(1360,860);
 app.setStyle("Fusion");app.setStyleSheet("QWidget{background:#0c131d;color:#dbe5ef;font-family:'Sans Serif';font-size:13px;} QPushButton,QComboBox{background:#1c2b3d;border:1px solid #304259;border-radius:6px;padding:8px 12px;} QPushButton:hover{border-color:#38d8b2;} QLineEdit,QListWidget{background:#111b28;border:1px solid #29394b;padding:8px;} QToolBar{spacing:8px;padding:12px;border-bottom:1px solid #263447;} QStatusBar{color:#90a5bb;} QLabel#brand{font-size:23px;font-weight:700;color:#54dfba;}");
 auto *toolbar=window.addToolBar("Workspace");toolbar->setMovable(false);auto *brand=new QLabel("◉  OpenWatch   ");brand->setObjectName("brand");toolbar->addWidget(brand);
 auto button=[&](QString text,auto callback){auto *b=new QPushButton(text);toolbar->addWidget(b);QObject::connect(b,&QPushButton::clicked,callback);return b;};
 auto *central=new QWidget;auto *horizontal=new QHBoxLayout(central);horizontal->setContentsMargins(16,16,16,16);horizontal->setSpacing(16);window.setCentralWidget(central);
 auto *side=new QWidget;side->setFixedWidth(225);auto *sidebar=new QVBoxLayout(side);sidebar->addWidget(new QLabel("DEVICES"));auto *list=new CameraList;list->setDragEnabled(true);sidebar->addWidget(list);auto *hint=new QLabel("Save devices to keep an encrypted list.\n\nDouble-click a device to connect.\nDrag it into any grid cell.\n\nMouse wheel: digital zoom\nF11: fullscreen workspace\nDouble-click video: focus / grid\nLeft / Right: previous / next\nEsc: return to grid / exit fullscreen");hint->setWordWrap(true);hint->setStyleSheet("color:#8195aa;");sidebar->addWidget(hint);horizontal->addWidget(side);
 auto *gridWidget=new QWidget;auto *grid=new QGridLayout(gridWidget);grid->setContentsMargins(0,0,0,0);grid->setSpacing(8);horizontal->addWidget(gridWidget,1);
 QVector<Cell*> cells;QVector<QUrl> urls;Cell *selected=nullptr;bool listen=false;int volume=75;QHash<QUuid,QPointer<CameraControls>> controls;
 auto choose=[&](Cell *cell){for(auto *c:cells)c->selected=(c==cell);selected=cell;for(auto *c:cells){c->video.audio(false);c->video.volume(volume);}if(listen)cell->video.audio(true);};
 auto connectCell=[&](Cell *cell,int index){if(index<0||index>=urls.size())return;choose(cell);cell->deviceId=devices::id(list->item(index));cell->name=list->item(index)->text();cell->zoom=1;cell->zoomPan={};cell->video.record({});cell->video.start(urls[index]);};
 for(int i=0;i<64;++i){auto *cell=new Cell;cell->select=choose;cell->drop=[&](Cell *target,const QUuid &id){connectCell(target,devices::rowForId(list,id));};cells.append(cell);cell->setParent(gridWidget);cell->hide();}choose(cells[0]);
 auto stopHidden=[&](int count){for(int i=count;i<64;++i)cells[i]->video.requestStop();for(int i=count;i<64;++i)cells[i]->video.stop();};
 Navigation navigation(grid,cells,choose);
 auto arrange=[&](int count){navigation.setGridSize(count);stopHidden(count);};

 auto *deviceControls=new QWidget;auto *deviceRow=new QHBoxLayout(deviceControls);deviceRow->setContentsMargins(0,0,0,0);
 auto *editDevice=new QPushButton("Edit");auto *removeDevice=new QPushButton("Remove");deviceRow->addWidget(editDevice);deviceRow->addWidget(removeDevice);sidebar->insertWidget(sidebar->indexOf(list)+1,deviceControls);
 auto selectionChanged=[&]{bool valid=list->currentRow()>=0;editDevice->setEnabled(valid);removeDevice->setEnabled(valid);};selectionChanged();
 QObject::connect(list,&QListWidget::currentRowChanged,[&](int){selectionChanged();});
 auto removeSelected=[&]{int row=list->currentRow();if(row<0)return;QString label=list->item(row)->text();auto removedId=devices::id(list->item(row));if(controls.contains(removedId)){delete controls.take(removedId);}bool restore=navigation.isFocused() && selected->deviceId==devices::id(list->item(row));devices::remove(list,urls,cells,row);if(restore)navigation.restore();selectionChanged();window.statusBar()->showMessage("Removed "+label+". Saved footage was kept.",5000);};
 auto editSelected=[&]{int row=list->currentRow();if(row<0)return;auto *item=list->item(row);QString label=item->text();QUrl source=urls[row];if(!devices::edit(&window,label,source))return;
  bool changed=source!=urls[row];if(changed){auto key=devices::id(item);if(controls.contains(key))delete controls.take(key);}if(source.host().compare(urls[row].host(),Qt::CaseInsensitive)!=0)item->setData(registry::EndpointRole,QString());auto id=devices::id(item);urls[row]=source;item->setText(label);
  if(changed)for(auto *cell:cells)if(cell->deviceId==id)cell->video.requestStop();
  for(auto *cell:cells)if(cell->deviceId==id){cell->name=label;if(changed){cell->video.stop();cell->video.record({});cell->video.start(source);}cell->update();}
  window.statusBar()->showMessage("Updated "+label,5000);
 };
 QObject::connect(removeDevice,&QPushButton::clicked,removeSelected);QObject::connect(editDevice,&QPushButton::clicked,editSelected);
 auto *deleteKey=new QShortcut(QKeySequence(Qt::Key_Delete),list);deleteKey->setContext(Qt::WidgetShortcut);QObject::connect(deleteKey,&QShortcut::activated,removeSelected);
 list->setContextMenuPolicy(Qt::CustomContextMenu);QObject::connect(list,&QListWidget::customContextMenuRequested,[&](QPoint position){auto *item=list->itemAt(position);if(!item)return;list->setCurrentItem(item);QMenu menu;auto *edit=menu.addAction("Edit device / stream");auto *remove=menu.addAction("Remove device / stream");auto *action=menu.exec(list->viewport()->mapToGlobal(position));if(action==remove)removeSelected();else if(action==edit)editSelected();});
 auto *search=new QLineEdit;search->setPlaceholderText("Search name or address");sidebar->insertWidget(sidebar->indexOf(list),search);
 auto *groups=new QComboBox;groups->addItem("All groups",QString());sidebar->insertWidget(sidebar->indexOf(list),groups);
 auto filter=[&]{for(int i=0;i<list->count();++i){auto *item=list->item(i);auto group=item->data(registry::GroupRole).toString();item->setHidden((!groups->currentData().toString().isEmpty() && groups->currentData().toString()!=group)||(!item->text().contains(search->text(),Qt::CaseInsensitive)&&!urls[i].host().contains(search->text(),Qt::CaseInsensitive)));}};
 auto refreshGroups=[&]{auto current=groups->currentData().toString();QSignalBlocker block(groups);groups->clear();groups->addItem("All groups",QString());QSet<QString> names;for(int i=0;i<list->count();++i){auto group=list->item(i)->data(registry::GroupRole).toString();if(!group.isEmpty())names.insert(group);}QStringList sorted=names.values();sorted.sort(Qt::CaseInsensitive);for(auto name:sorted)groups->addItem(name,name);int index=groups->findData(current);groups->setCurrentIndex(index<0?0:index);filter();};
 QObject::connect(search,&QLineEdit::textChanged,[&]{filter();});QObject::connect(groups,&QComboBox::currentIndexChanged,[&]{filter();});
 QObject::connect(list->model(),&QAbstractItemModel::rowsInserted,[&]{refreshGroups();});QObject::connect(list->model(),&QAbstractItemModel::rowsRemoved,[&]{refreshGroups();});
 QObject::connect(list,&QListWidget::itemChanged,[&]{refreshGroups();});
 auto *groupButton=new QPushButton("Assign group");sidebar->insertWidget(sidebar->indexOf(deviceControls)+1,groupButton);
 QObject::connect(groupButton,&QPushButton::clicked,[&]{auto *item=list->currentItem();if(!item)return;bool ok=false;auto name=QInputDialog::getText(&window,"Device group","Group name (empty removes group)",QLineEdit::Normal,item->data(registry::GroupRole).toString(),&ok);if(ok)item->setData(registry::GroupRole,name.trimmed());});
 auto *files=new QWidget;auto *fileRow=new QHBoxLayout(files);fileRow->setContentsMargins(0,0,0,0);auto *save=new QPushButton("Save devices"),*load=new QPushButton("Load devices");fileRow->addWidget(save);fileRow->addWidget(load);sidebar->insertWidget(sidebar->indexOf(groupButton)+1,files);
 QObject::connect(save,&QPushButton::clicked,[&]{auto path=QFileDialog::getSaveFileName(&window,"Save encrypted devices",{},"OpenWatch devices (*.owv)");if(path.isEmpty())return;if(!path.endsWith(".owv",Qt::CaseInsensitive)){path+=".owv";if(QFileInfo::exists(path)&&QMessageBox::question(&window,"Replace device file","Replace the existing device file at "+path+"?")!=QMessageBox::Yes)return;}QString secret;if(!registry::passphrase(&window,secret,true))return;try{auto bytes=vault::seal(QJsonDocument(registry::serialize(list,urls)).toJson(QJsonDocument::Compact),secret);QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())throw std::runtime_error("Cannot save device file");window.statusBar()->showMessage("Encrypted device list saved",5000);}catch(const std::exception &e){QMessageBox::warning(&window,"Save devices",e.what());}});
 QObject::connect(load,&QPushButton::clicked,[&]{auto path=QFileDialog::getOpenFileName(&window,"Load devices (replaces current list)",{},"OpenWatch devices (*.owv)");if(path.isEmpty())return;QString secret;if(!registry::passphrase(&window,secret,false))return;try{QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>4*1024*1024+48)throw std::runtime_error("Cannot read device file or file too large");auto rows=registry::validate(vault::open(file.readAll(),secret));for(auto dialog:controls)if(dialog)delete dialog;controls.clear();for(auto *cell:cells)cell->video.requestStop();while(list->count())devices::remove(list,urls,cells,list->count()-1);navigation.restore();for(auto row:rows){auto o=row.toObject();urls.append(QUrl(o["url"].toString()));auto *item=new QListWidgetItem(o["name"].toString());item->setData(Qt::UserRole,QUuid(o["id"].toString()));item->setData(registry::GroupRole,o["group"].toString());item->setData(registry::EndpointRole,o["onvif"].toString());list->addItem(item);}refreshGroups();window.statusBar()->showMessage("Devices loaded. Double-click a device or drag it into the grid to connect.",8000);}catch(const std::exception &e){QMessageBox::warning(&window,"Load devices",e.what());}});
 auto controlFor=[&](int row,bool create)->CameraControls*{if(row<0||row>=urls.size())return nullptr;auto *item=list->item(row);auto id=devices::id(item);auto dialog=controls.value(id);
  if(dialog&&!dialog->matchesSource(urls[row])){delete dialog;dialog=nullptr;controls.remove(id);}
  if(!dialog&&create){dialog=new CameraControls(item->text(),urls[row],item->data(registry::EndpointRole).toString(),&window);controls[id]=dialog;dialog->report=[&](QString message){window.statusBar()->showMessage(message,5000);};
   QObject::connect(dialog,&QDialog::finished,&window,[&,id,dialog]{int current=devices::rowForId(list,id);if(current<0)return;QUrl endpoint(dialog->endpoint());if(endpoint.isValid()&&QStringList{"http","https"}.contains(endpoint.scheme())&&endpoint.userInfo().isEmpty()&&endpoint.host().compare(urls[current].host(),Qt::CaseInsensitive)==0)list->item(current)->setData(registry::EndpointRole,dialog->endpoint());});
  }return dialog;
 };
 button("Camera controls",[&]{int row=devices::rowForId(list,selected->deviceId);if(row<0)row=list->currentRow();auto *dialog=controlFor(row,true);if(!dialog){window.statusBar()->showMessage("Select a connected camera or a device first",5000);return;}dialog->show();dialog->raise();dialog->activateWindow();});
 installPtzKeys(&window,[&](QString action){int row=devices::rowForId(list,selected->deviceId);auto *dialog=controlFor(row,false);if(!dialog){window.statusBar()->showMessage("Select a channel and load Camera controls before using PTZ keys",5000);return;}dialog->keyboard(action);});
 auto *audioButton=new QPushButton("Audio: off");QObject::connect(audioButton,&QPushButton::clicked,[&]{listen=!listen;for(auto *cell:cells)cell->video.audio(listen&&cell==selected);});auto *audioRow=new QWidget;auto *audioLayout=new QHBoxLayout(audioRow);audioLayout->setContentsMargins(0,0,0,0);audioLayout->addWidget(audioButton);sidebar->insertWidget(sidebar->indexOf(hint),audioRow);
 auto *volumeSlider=new QSlider(Qt::Horizontal);volumeSlider->setRange(0,100);volumeSlider->setValue(volume);volumeSlider->setFixedWidth(70);volumeSlider->setToolTip("Live audio volume");audioLayout->addWidget(volumeSlider);QObject::connect(volumeSlider,&QSlider::valueChanged,[&](int value){volume=value;for(auto *cell:cells)cell->video.volume(value);});
 auto *audioLabel=new QLabel("Audio muted");audioLabel->setWordWrap(true);sidebar->insertWidget(sidebar->indexOf(hint),audioLabel);

 button("Reset zoom",[&]{selected->zoom=1;selected->zoomPan={};selected->update();});
 button("＋ Add device",[&]{QDialog dialog(&window);dialog.setWindowTitle("Add video source");auto *form=new QFormLayout(&dialog);QLineEdit label("Camera"),url,user,password;url.setPlaceholderText("dvrip://192.168.1.10:34567?channel=0&subtype=0");url.setMinimumWidth(500);password.setEchoMode(QLineEdit::Password);form->addRow("Name",&label);form->addRow("Stream URL",&url);form->addRow("Username",&user);form->addRow("Password",&password);form->addRow(new QLabel("DVRIP · RTSP · RTMP · HTTP(S) HLS\nDVRIP channels start at 0. Use Save devices to keep an encrypted list."));QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(&buttons);QObject::connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);if(dialog.exec()!=QDialog::Accepted)return;QUrl source(url.text().trimmed());if(!QStringList{"dvrip","rtsp","rtsps","rtmp","rtmps","http","https"}.contains(source.scheme())||source.host().isEmpty()){QMessageBox::warning(&window,"Invalid source","Enter a supported URL with a host.");return;}if(!user.text().isEmpty())source.setUserName(user.text());if(!password.text().isEmpty())source.setPassword(password.text());urls.append(source);list->addItem(label.text().isEmpty()?"Camera":label.text());});
 button("Record",[&]{auto path=QFileDialog::getSaveFileName(&window,"Record selected channel",{},"Matroska (*.mkv)");if(path.isEmpty())return;if(!path.endsWith(".mkv",Qt::CaseInsensitive))path+=".mkv";selected->video.record(path);window.statusBar()->showMessage("Recording requested; starts at the next keyframe.",5000);});
 button("Stop recording",[&]{selected->video.record({});});
 button("Snapshot",[&]{auto frame=selected->video.frame();if(frame.isNull())return;auto path=QFileDialog::getSaveFileName(&window,"Save snapshot",{},"PNG (*.png)");if(!path.isEmpty()&&!frame.save(path,"PNG"))QMessageBox::warning(&window,"Snapshot","Could not save image.");});
 button("Disconnect",[&]{selected->video.stop();});
 auto *layout=new QComboBox;layout->setObjectName("gridLayout");for(int n:{1,4,9,16,25,36,64})layout->addItem(QString::number(n)+" channels",n);layout->setCurrentIndex(1);toolbar->addWidget(layout);QObject::connect(layout,&QComboBox::currentIndexChanged,[&](int){arrange(layout->currentData().toInt());});

 auto *xmButton=new QPushButton("XMEye recorder");xmButton->setObjectName("xmRecorder");sidebar->insertWidget(1,xmButton);
 QObject::connect(xmButton,&QPushButton::clicked,[&]{
  RecorderDialog dialog(&window);
  if(dialog.exec()!=QDialog::Accepted)return;
  for(auto control:controls)if(control)delete control;controls.clear();
  auto channels=dialog.channels();int count=qMin(64,int(channels.size()));
  // Stop existing grid sessions together before replacing them with the recorder.
  for(auto *cell:cells)cell->video.requestStop();
  for(auto *cell:cells)cell->video.stop();
  int gridSize=1;for(int n:{1,4,9,16,25,36,64})if(n>=count){gridSize=n;break;}
  layout->setCurrentIndex(layout->findData(gridSize));arrange(gridSize);
  for(int i=0;i<count;++i){
   int index=-1;
   for(int j=0;j<urls.size();++j)if(urls[j].matches(channels[i],QUrl::RemoveUserInfo)){index=j;break;}
   QString label=dialog.recorderName()+" · CH "+QString::number(i+1);
   if(index<0){index=urls.size();urls.append(channels[i]);list->addItem(label);}
   else {urls[index]=channels[i];list->item(index)->setText(label);}
   connectCell(cells[i],index);
  }
  choose(cells[0]);
  QString message=QString("XMEye local • %1 channels detected • opening %2 streams").arg(channels.size()).arg(count);
  if(channels.size()>64)message+=" • Grid limit: first 64 channels only";
  window.statusBar()->showMessage(message);
 });
 auto *discoverButton=new QPushButton("Discover devices");sidebar->insertWidget(1,discoverButton);
 QObject::connect(discoverButton,&QPushButton::clicked,[&]{
  DiscoveryDialog dialog(&window);if(dialog.exec()!=QDialog::Accepted)return;
  for(auto control:controls)if(control)delete control;controls.clear();
  auto found=dialog.urls();auto names=dialog.names();if(found.isEmpty())return;
  for(auto *cell:cells)cell->video.requestStop();for(auto *cell:cells)cell->video.stop();
  int count=qMin(64,int(found.size())),gridSize=1;for(int n:{1,4,9,16,25,36,64})if(n>=count){gridSize=n;break;}
  layout->setCurrentIndex(layout->findData(gridSize));arrange(gridSize);
  for(int i=0;i<count;++i){int index=-1;for(int j=0;j<urls.size();++j)if(urls[j].matches(found[i],QUrl::RemoveUserInfo)){index=j;break;}
   if(index<0){index=urls.size();urls.append(found[i]);list->addItem(names[i]);}else{urls[index]=found[i];list->item(index)->setText(names[i]);}if(QStringList{"http","https"}.contains(dialog.deviceEndpoint().scheme()))list->item(index)->setData(registry::EndpointRole,dialog.deviceEndpoint().toString(QUrl::RemoveUserInfo));connectCell(cells[i],index);
  }
  choose(cells[0]);window.statusBar()->showMessage(QString("Discovery • Opening %1 streams. ").arg(count)+dialog.warnings());
 });
 QObject::connect(list,&QListWidget::itemDoubleClicked,[&](QListWidgetItem *item){connectCell(selected,list->row(item));});
 auto *full=new QShortcut(QKeySequence(Qt::Key_F11),&window);QObject::connect(full,&QShortcut::activated,[&]{if(window.isFullScreen())window.showNormal();else window.showFullScreen();});auto *esc=new QShortcut(QKeySequence(Qt::Key_Escape),&window);QObject::connect(esc,&QShortcut::activated,[&]{navigation.restore();window.showNormal();});
 auto *left=new QShortcut(QKeySequence(Qt::Key_Left),&window);QObject::connect(left,&QShortcut::activated,[&]{navigation.step(-1);});
 auto *right=new QShortcut(QKeySequence(Qt::Key_Right),&window);QObject::connect(right,&QShortcut::activated,[&]{navigation.step(1);});
 auto *timer=new QTimer(&window);QObject::connect(timer,&QTimer::timeout,[&]{for(auto *cell:cells)if(cell->isVisible())cell->update();audioButton->setText(listen?"Audio: on":"Audio: off");audioLabel->setText(selected->video.audioStatus());});timer->start(40);
 arrange(4);window.statusBar()->showMessage("LOCAL WORKSPACE   •   Experimental v0.11.3   •   No cloud connection   •   Select a cell to control it");window.show();
 if(app.arguments().contains("--screenshot")){int i=app.arguments().indexOf("--screenshot");if(i+1<app.arguments().size())QTimer::singleShot(200,[&,i]{window.grab().save(app.arguments()[i+1]);});}
 if(app.arguments().contains("--smoke-test"))QTimer::singleShot(500,&app,&QApplication::quit);
 int result=app.exec();for(auto dialog:controls)if(dialog)delete dialog;controls.clear();for(auto *cell:cells)cell->video.requestStop();for(auto *cell:cells)cell->video.stop();return result;
}
