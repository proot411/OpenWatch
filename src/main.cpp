#include <QtWidgets>
#include <QSettings>
#include <cmath>
#include "cell.h"
#include "wfstestdialog.h"
#include "networkplayback.h"
#include "devices.h"
#include "devicetree.h"
#include "recordingui.h"
#include "playbackwindow.h"
#include "registry.h"
#include "theme.h"
#include "controls.h"
#include "ptzkeys.h"
#include "navigation.h"
#include "recorderdialog.h"
#include "discoverydialog.h"
extern "C" {
#include <libavutil/log.h>
}
class WorkspaceWindow:public QMainWindow {public:std::function<void()> beforeClose;void closeEvent(QCloseEvent *e)override{if(beforeClose)beforeClose();QMainWindow::closeEvent(e);}};
class CameraList:public QListWidget {
 void startDrag(Qt::DropActions)override{if(currentRow()<0)return;auto *mime=new QMimeData;mime->setData("application/x-openwatch-camera",devices::id(currentItem()).toString().toUtf8());auto *drag=new QDrag(this);drag->setMimeData(mime);drag->exec(Qt::CopyAction);}
};
class EmptyState:public QObject { // keeps a centred hint pinned inside the device tree viewport
 QWidget *viewport;QLabel *hint;
 public:
  EmptyState(QWidget *viewport,QLabel *hint):viewport(viewport),hint(hint){viewport->installEventFilter(this);place();}
  void place(){hint->setGeometry(viewport->rect().adjusted(12,12,-12,-12));}
  bool eventFilter(QObject *watched,QEvent *event)override{if(watched==viewport&&event->type()==QEvent::Resize)place();return QObject::eventFilter(watched,event);}
};
int main(int argc,char **argv){
 QApplication app(argc,argv);app.setApplicationName("OpenWatch");app.setOrganizationName("OpenWatch");av_log_set_level(AV_LOG_QUIET);
 QSettings settings; // startup/configuration preferences (never stores the passphrase)
 WorkspaceWindow window;window.setWindowTitle("OpenWatch • Video workspace");auto available=app.primaryScreen()->availableGeometry().size();window.resize(qMin(1360,available.width()),qMin(860,available.height()-40));
 app.setStyle("Fusion");app.setStyleSheet(theme::stylesheet());
 auto *toolbar=window.addToolBar("Workspace");toolbar->setMovable(false);auto *brand=new QLabel("◉  OpenWatch   ");brand->setObjectName("brand");toolbar->addWidget(brand);
 auto *central=new QWidget;auto *horizontal=new QHBoxLayout(central);horizontal->setContentsMargins(12,12,12,12);horizontal->setSpacing(12);window.setCentralWidget(central);
 auto *side=new QWidget;side->setFixedWidth(290);auto *sidebar=new QVBoxLayout(side);sidebar->setContentsMargins(0,0,0,0);sidebar->setSpacing(8);horizontal->addWidget(side);
 auto *toggle=new QPushButton("☰");toggle->setToolTip("Show / hide navigation");toolbar->insertWidget(toolbar->actions().first(),toggle);QObject::connect(toggle,&QPushButton::clicked,[&]{side->setVisible(!side->isVisible());});
 auto *deviceMenu=new QMenu(&window);auto *manage=new QPushButton("Devices  ▾");manage->setMenu(deviceMenu);sidebar->addWidget(manage);
 auto *navPlayback=new QPushButton("Playback");navPlayback->setObjectName("nvrPlayback");sidebar->addWidget(navPlayback);QPointer<PlaybackWindow> playbackWindow;
 auto *cameraMenu=new QMenu(&window);auto *cameraActions=new QPushButton("Others  ▾");cameraActions->setMenu(cameraMenu);toolbar->addWidget(cameraActions);
 auto button=[&](QString text,auto callback){auto *b=new QPushButton(text,&window);QObject::connect(b,&QPushButton::clicked,callback);if(text=="Playback"){b->hide();QObject::connect(navPlayback,&QPushButton::clicked,b,&QPushButton::click);}else if(QStringList{"Camera controls","Snapshot","Record","Reset zoom"}.contains(text)){auto *action=toolbar->insertWidget(toolbar->actions().last(),b);action->setVisible(true);}else{b->hide();auto *menu=text.contains("Add device")?deviceMenu:cameraMenu;auto *action=menu->addAction(text);QObject::connect(action,&QAction::triggered,b,&QPushButton::click);}return b;};
 auto *list=new CameraList;list->setParent(side);list->hide();auto *tree=new DeviceTree;sidebar->addWidget(tree,1);
 auto *hint=new QLabel("Double-click to connect · Drag into a view");hint->setObjectName("hint");hint->setWordWrap(true);sidebar->addWidget(hint);
 auto *emptyHint=new QLabel(tree->viewport());emptyHint->setObjectName("emptyHint");emptyHint->setAlignment(Qt::AlignCenter);emptyHint->setWordWrap(true);
 emptyHint->setText("No cameras yet\n\nAdd one with Devices ▾ → Add device,\nor use Discover devices to find cameras on this network.\n\nOnce added, double-click an entry or drag it into a view.");
 emptyHint->setAttribute(Qt::WA_TransparentForMouseEvents);new EmptyState(tree->viewport(),emptyHint);
 auto updateEmpty=[&]{emptyHint->setVisible(list->count()==0);};
 QObject::connect(list->model(),&QAbstractItemModel::rowsInserted,updateEmpty);QObject::connect(list->model(),&QAbstractItemModel::rowsRemoved,updateEmpty);updateEmpty();
 auto *gridWidget=new QWidget;auto *grid=new QGridLayout(gridWidget);grid->setContentsMargins(0,0,0,0);grid->setSpacing(8);horizontal->addWidget(gridWidget,1);
 QVector<Cell*> cells;QVector<QUrl> urls;Cell *selected=nullptr;bool listen=false;int volume=75;QHash<QUuid,QPointer<CameraControls>> controls;
 RecordingUi recordingUi(&window);window.beforeClose=[&]{recordingUi.closeAll();};
 auto choose=[&](Cell *cell){for(auto *c:cells)c->selected=(c==cell);selected=cell;for(auto *c:cells){c->video.audio(false);c->video.volume(volume);}if(listen)cell->video.audio(true);};
 auto connectCell=[&](Cell *cell,int index){if(index<0||index>=urls.size())return;choose(cell);cell->deviceId=devices::id(list->item(index));cell->name=list->item(index)->text();cell->zoom=1;cell->zoomPan={};cell->video.record({});cell->video.start(urls[index]);};
 for(int i=0;i<64;++i){auto *cell=new Cell;cell->select=choose;cell->drop=[&](Cell *target,const QUuid &id){connectCell(target,devices::rowForId(list,id));};cells.append(cell);cell->setParent(gridWidget);cell->hide();}choose(cells[0]);
 auto stopHidden=[&](int count){for(int i=count;i<64;++i)cells[i]->video.requestStop();for(int i=count;i<64;++i)cells[i]->video.stop();};
 Navigation navigation(grid,cells,choose);
 auto arrange=[&](int count){navigation.setGridSize(count);stopHidden(count);};

 auto *deviceControls=new QWidget;auto *deviceRow=new QHBoxLayout(deviceControls);deviceRow->setContentsMargins(0,0,0,0);
 auto *editDevice=new QPushButton("Edit");auto *removeDevice=new QPushButton("Remove");deviceRow->addWidget(editDevice);deviceRow->addWidget(removeDevice);sidebar->insertWidget(sidebar->indexOf(tree)+1,deviceControls);
 auto selectionChanged=[&]{bool valid=list->currentRow()>=0;editDevice->setEnabled(valid);removeDevice->setEnabled(valid||(tree->currentItem()&&tree->currentItem()->childCount()));};selectionChanged();
 QObject::connect(list,&QListWidget::currentRowChanged,[&](int){selectionChanged();});
 auto removeSelected=[&]{
  auto *item=tree->currentItem();if(!item)return;QVector<QUuid> ids;auto key=item->data(0,Qt::UserRole+1).toString();
  if(!key.isEmpty()){for(int i=0;i<urls.size();++i)if(DeviceTree::groupKey(urls[i])==key)ids.append(devices::id(list->item(i)));}else ids.append(tree->deviceId(item));
  bool restore=false;for(const auto &id:ids){int row=devices::rowForId(list,id);if(row<0)continue;if(controls.contains(id))delete controls.take(id);restore|=navigation.isFocused()&&selected->deviceId==id;devices::remove(list,urls,cells,row);}
  if(restore)navigation.restore();tree->refresh(list,urls);list->setCurrentRow(devices::rowForId(list,tree->deviceId(tree->currentItem())));selectionChanged();window.statusBar()->showMessage(QString("Removed %1 camera entries. Saved footage was kept.").arg(ids.size()),5000);
 };
 auto editSelected=[&]{int row=list->currentRow();if(row<0)return;auto *item=list->item(row);QString label=item->text();QUrl source=urls[row];if(!devices::edit(&window,label,source))return;
  bool changed=source!=urls[row];if(changed){auto key=devices::id(item);if(controls.contains(key))delete controls.take(key);}if(source.host().compare(urls[row].host(),Qt::CaseInsensitive)!=0)item->setData(registry::EndpointRole,QString());auto id=devices::id(item);urls[row]=source;item->setText(label);
  if(changed)for(auto *cell:cells)if(cell->deviceId==id)cell->video.requestStop();
  for(auto *cell:cells)if(cell->deviceId==id){cell->name=label;if(changed){cell->video.stop();cell->video.record({});cell->video.start(source);}cell->update();}
  window.statusBar()->showMessage("Updated "+label,5000);
 };
 QObject::connect(removeDevice,&QPushButton::clicked,removeSelected);QObject::connect(editDevice,&QPushButton::clicked,editSelected);
 auto *deleteKey=new QShortcut(QKeySequence(Qt::Key_Delete),tree);deleteKey->setContext(Qt::WidgetShortcut);QObject::connect(deleteKey,&QShortcut::activated,removeSelected);
 tree->setContextMenuPolicy(Qt::CustomContextMenu);QObject::connect(tree,&QTreeWidget::customContextMenuRequested,[&](QPoint position){auto *item=tree->itemAt(position);auto id=tree->deviceId(item);if(!item)return;tree->setCurrentItem(item);list->setCurrentRow(devices::rowForId(list,id));QMenu menu;auto *edit=menu.addAction("Edit device / stream");edit->setEnabled(!id.isNull());auto *remove=menu.addAction(id.isNull()?"Remove all cameras in this section":"Remove device / stream");auto *action=menu.exec(tree->viewport()->mapToGlobal(position));if(action==remove)removeSelected();else if(action==edit)editSelected();});
 QObject::connect(tree,&QTreeWidget::currentItemChanged,[&](QTreeWidgetItem *item){list->setCurrentRow(devices::rowForId(list,tree->deviceId(item)));selectionChanged();});
 QObject::connect(tree,&QTreeWidget::itemDoubleClicked,[&](QTreeWidgetItem *item){connectCell(selected,devices::rowForId(list,tree->deviceId(item)));});
 auto *treeRefresh=new QTimer(&window);treeRefresh->setSingleShot(true);QObject::connect(treeRefresh,&QTimer::timeout,[&]{tree->refresh(list,urls);list->setCurrentRow(devices::rowForId(list,tree->deviceId(tree->currentItem())));selectionChanged();});
 auto *search=new QLineEdit;search->setPlaceholderText("Search name or address");sidebar->insertWidget(sidebar->indexOf(tree),search);
 auto *groups=new QComboBox;groups->addItem("All groups",QString());sidebar->insertWidget(sidebar->indexOf(tree),groups);
 auto filter=[&]{for(int i=0;i<list->count();++i){auto *item=list->item(i);auto group=item->data(registry::GroupRole).toString();item->setHidden((!groups->currentData().toString().isEmpty() && groups->currentData().toString()!=group)||(!item->text().contains(search->text(),Qt::CaseInsensitive)&&!urls[i].host().contains(search->text(),Qt::CaseInsensitive)));}treeRefresh->start(0);};
 auto refreshGroups=[&]{auto current=groups->currentData().toString();QSignalBlocker block(groups);groups->clear();groups->addItem("All groups",QString());QSet<QString> names;for(int i=0;i<list->count();++i){auto group=list->item(i)->data(registry::GroupRole).toString();if(!group.isEmpty())names.insert(group);}QStringList sorted=names.values();sorted.sort(Qt::CaseInsensitive);for(auto name:sorted)groups->addItem(name,name);int index=groups->findData(current);groups->setCurrentIndex(index<0?0:index);filter();};
 QObject::connect(search,&QLineEdit::textChanged,[&]{filter();});QObject::connect(groups,&QComboBox::currentIndexChanged,[&]{filter();});
 QObject::connect(list->model(),&QAbstractItemModel::rowsInserted,[&]{refreshGroups();});QObject::connect(list->model(),&QAbstractItemModel::rowsRemoved,[&]{refreshGroups();});
 QObject::connect(list,&QListWidget::itemChanged,[&]{refreshGroups();});
 auto *groupButton=new QPushButton("Assign group");groupButton->setParent(side);groupButton->hide();deviceMenu->addAction("Assign selected camera to group",groupButton,&QPushButton::click);
 QObject::connect(groupButton,&QPushButton::clicked,[&]{auto *item=list->currentItem();if(!item)return;bool ok=false;auto name=QInputDialog::getText(&window,"Device group","Group name (empty removes group)",QLineEdit::Normal,item->data(registry::GroupRole).toString(),&ok);if(ok)item->setData(registry::GroupRole,name.trimmed());});
 auto *files=new QWidget;auto *fileRow=new QHBoxLayout(files);fileRow->setContentsMargins(0,0,0,0);auto *save=new QPushButton("Save devices"),*load=new QPushButton("Load devices");fileRow->addWidget(save);fileRow->addWidget(load);files->setParent(side);files->hide();deviceMenu->addSeparator();deviceMenu->addAction("Save encrypted devices…",save,&QPushButton::click);deviceMenu->addAction("Load encrypted devices…",load,&QPushButton::click);
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
 auto *audioLabel=new QLabel("Audio muted");audioLabel->setObjectName("audioState");audioLabel->setWordWrap(true);sidebar->insertWidget(sidebar->indexOf(hint),audioLabel);

 button("Reset zoom",[&]{selected->zoom=1;selected->zoomPan={};selected->update();});
 auto *addDeviceButton=button("＋ Add device",[&]{QDialog dialog(&window);dialog.setWindowTitle("Add video source");auto *form=new QFormLayout(&dialog);QLineEdit label("Camera"),url,user,password;url.setPlaceholderText("dvrip://192.168.1.10:34567?channel=0&subtype=0");url.setMinimumWidth(500);password.setEchoMode(QLineEdit::Password);form->addRow("Name",&label);form->addRow("Stream URL",&url);form->addRow("Username",&user);form->addRow("Password",&password);form->addRow(new QLabel("DVRIP · RTSP · RTMP · HTTP(S) HLS\nDVRIP channels start at 0. Use Save devices to keep an encrypted list."));QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(&buttons);QObject::connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);if(dialog.exec()!=QDialog::Accepted)return;QUrl source(url.text().trimmed());if(!QStringList{"dvrip","rtsp","rtsps","rtmp","rtmps","http","https"}.contains(source.scheme())||source.host().isEmpty()){QMessageBox::warning(&window,"Invalid source","Enter a supported URL with a host.");return;}if(!user.text().isEmpty())source.setUserName(user.text());if(!password.text().isEmpty())source.setPassword(password.text());urls.append(source);list->addItem(label.text().isEmpty()?"Camera":label.text());});
 auto *recordButton=button("Record",[&]{recordingUi.toggle(selected);});recordButton->setObjectName("recordToggle");
 cameraMenu->addAction("Unsaved recordings",[&]{QDir().mkpath(RecordingUi::pendingRoot());QDesktopServices::openUrl(QUrl::fromLocalFile(RecordingUi::pendingRoot()));});
 button("Snapshot",[&]{auto frame=selected->video.frame();if(frame.isNull())return;auto path=QFileDialog::getSaveFileName(&window,"Save snapshot",{},"PNG (*.png)");if(!path.isEmpty()&&!frame.save(path,"PNG"))QMessageBox::warning(&window,"Snapshot","Could not save image.");});
 button("Disconnect",[&]{selected->video.stop();});
 auto *layout=new QComboBox;layout->setObjectName("gridLayout");for(int n:{1,4,9,16,25,36,64})layout->addItem(QString::number(n)+" channels",n);layout->setCurrentIndex(1);toolbar->addWidget(layout);QObject::connect(layout,&QComboBox::currentIndexChanged,[&](int){arrange(layout->currentData().toInt());});
 QObject::connect(save,&QPushButton::clicked,[&]{auto path=QFileDialog::getSaveFileName(&window,"Save encrypted devices",settings.value("deviceFile").toString(),"OpenWatch devices (*.owv)");if(path.isEmpty())return;if(!path.endsWith(".owv",Qt::CaseInsensitive)){path+=".owv";if(QFileInfo::exists(path)&&QMessageBox::question(&window,"Replace device file","Replace the existing device file at "+path+"?")!=QMessageBox::Yes)return;}QString secret;if(!registry::passphrase(&window,secret,true))return;try{QVector<QUuid> assigned;for(auto *cell:cells)assigned.append(cell->deviceId);auto bytes=vault::seal(QJsonDocument(registry::pack(list,urls,layout->currentData().toInt(),assigned)).toJson(QJsonDocument::Compact),secret);QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())throw std::runtime_error("Cannot save device file");settings.setValue("deviceFile",path);settings.sync();window.statusBar()->showMessage("Encrypted device list and camera layout saved",5000);}catch(const std::exception &e){QMessageBox::warning(&window,"Save devices",e.what());}});
  // Returns 0 = user cancelled, 1 = unlock failed (safe to retry), 2 = loaded.
  auto loadConfiguration=[&](const QString &path)->int{
   QString secret;if(!registry::passphrase(&window,secret,false))return 0;
   QByteArray payload;
   try{QFile file(path);
    if(!file.open(QIODevice::ReadOnly)||file.size()>4*1024*1024+48)throw std::runtime_error("Cannot read device file or file too large");
    payload=vault::open(file.readAll(),secret);
   }catch(const std::exception &e){QMessageBox::warning(&window,"Load devices",e.what());return 1;} // wrong passphrase: no state was touched
   try{
    auto snapshot=registry::unpack(payload); // decrypt + validate before touching any state
    for(auto dialog:controls)if(dialog)delete dialog;controls.clear();
    for(auto *cell:cells)cell->video.requestStop();
    while(list->count())devices::remove(list,urls,cells,list->count()-1);
    navigation.restore();
    for(auto row:snapshot.devices){auto o=row.toObject();urls.append(QUrl(o["url"].toString()));auto *item=new QListWidgetItem(o["name"].toString());item->setData(Qt::UserRole,QUuid(o["id"].toString()));item->setData(registry::GroupRole,o["group"].toString());item->setData(registry::EndpointRole,o["onvif"].toString());list->addItem(item);}
    refreshGroups();settings.setValue("deviceFile",path);settings.sync();
    int restored=0;
    if(snapshot.grid>0){int index=layout->findData(snapshot.grid);if(index>=0){layout->setCurrentIndex(index);arrange(snapshot.grid);
      for(int i=0;i<snapshot.cells.size()&&i<snapshot.grid;++i){auto identity=snapshot.cells[i];if(identity.isNull())continue;int row=devices::rowForId(list,identity);if(row<0)continue;connectCell(cells[i],row);++restored;}}}
    if(!restored)choose(cells[0]);
    window.statusBar()->showMessage(restored?QString("Configuration loaded: %1 device(s), %2 view(s) restored.").arg(list->count()).arg(restored):"Devices loaded. Double-click a device or drag it into the grid to connect.",8000);
    return 2;
   }catch(const std::exception &e){QMessageBox::warning(&window,"Load devices",e.what());return 1;}
  };
 QObject::connect(load,&QPushButton::clicked,[&]{auto path=QFileDialog::getOpenFileName(&window,"Load devices (replaces current list)",settings.value("deviceFile").toString(),"OpenWatch devices (*.owv)");if(!path.isEmpty())loadConfiguration(path);});

 button("Playback",[&]{QUrl initial;int playbackRow=list->currentRow();if(playbackRow<0&&tree->currentItem()&&tree->currentItem()->childCount())playbackRow=devices::rowForId(list,tree->deviceId(tree->currentItem()->child(0)));if(playbackRow<0)playbackRow=devices::rowForId(list,selected->deviceId);if(playbackRow>=0&&playbackRow<urls.size()&&urls[playbackRow].scheme()=="dvrip")initial=urls[playbackRow];if(list->currentRow()>=0&&list->currentRow()<urls.size()&&urls[list->currentRow()].scheme()=="dvrip")initial=urls[list->currentRow()];if(!playbackWindow){playbackWindow=new PlaybackWindow(initial,&window);playbackWindow->setAttribute(Qt::WA_DeleteOnClose);}playbackWindow->show();playbackWindow->raise();playbackWindow->activateWindow();});
 auto *xmButton=new QPushButton("XMEye recorder");xmButton->setObjectName("xmRecorder");deviceMenu->addAction("Add XMEye recorder",xmButton,&QPushButton::click);xmButton->setParent(side);xmButton->hide();
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
  QString message=QString("XMEye %1 • opening %2 streams").arg(dialog.isCloud()?"cloud relay (experimental)":"local").arg(count);
  if(channels.size()>64)message+=" • Grid limit: first 64 channels only";
  window.statusBar()->showMessage(message);
 });
 auto *discoverButton=new QPushButton("Discover devices");deviceMenu->addAction("Discover devices",discoverButton,&QPushButton::click);discoverButton->setParent(side);discoverButton->hide();
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
 auto *help=cameraMenu->addAction("Keyboard and mouse help");QObject::connect(help,&QAction::triggered,[&]{QMessageBox::information(&window,"Navigation","Double-click video: focus / grid\nMouse wheel: digital zoom\nLeft / Right: previous / next view\nF11: fullscreen · Esc: return to grid\nPTZ shortcuts work after loading Camera controls.");});
 auto *full=new QShortcut(QKeySequence(Qt::Key_F11),&window);QObject::connect(full,&QShortcut::activated,[&]{if(window.isFullScreen())window.showNormal();else window.showFullScreen();});auto *esc=new QShortcut(QKeySequence(Qt::Key_Escape),&window);QObject::connect(esc,&QShortcut::activated,[&]{navigation.restore();window.showNormal();});
 auto *left=new QShortcut(QKeySequence(Qt::Key_Left),&window);QObject::connect(left,&QShortcut::activated,[&]{if(!side->isAncestorOf(app.focusWidget()))navigation.step(-1);});
 auto *right=new QShortcut(QKeySequence(Qt::Key_Right),&window);QObject::connect(right,&QShortcut::activated,[&]{if(!side->isAncestorOf(app.focusWidget()))navigation.step(1);});
 QObject::connect(&app,&QApplication::focusChanged,[&](QWidget*,QWidget *now){bool browsing=now&&side->isAncestorOf(now);left->setEnabled(!browsing);right->setEnabled(!browsing);});
 auto *timer=new QTimer(&window);QObject::connect(timer,&QTimer::timeout,[&]{for(auto *cell:cells)if(cell->isVisible())cell->update();recordButton->setText(recordingUi.stopping(selected)?"Finishing…":recordingUi.pending(selected)?"Stop recording":"Record");recordButton->setEnabled(!recordingUi.stopping(selected)&&(recordingUi.pending(selected)||(!selected->video.frame().isNull()&&selected->video.status().startsWith("Live"))));recordingUi.poll();audioButton->setText(listen?"Audio: on":"Audio: off");
  auto audioState=selected->video.audioStatus();
  QString stateKey=audioState=="Live audio"?"live":audioState=="Muted"?"muted":"error";
  if(audioLabel->property("state").toString()!=stateKey){audioLabel->setProperty("state",stateKey);audioLabel->style()->unpolish(audioLabel);audioLabel->style()->polish(audioLabel);}
  audioLabel->setText(audioState);});timer->start(40);
 auto *menuBar=window.menuBar();
 auto *fileMenu=menuBar->addMenu("&File");
 fileMenu->addAction("Save devices...",save,&QPushButton::click);
 fileMenu->addAction("Load devices...",load,&QPushButton::click);
 fileMenu->addSeparator();
 fileMenu->addAction("Quit",&window,&QWidget::close);
 auto *configMenu=menuBar->addMenu("&Configuration");
 auto *autoLoadAction=configMenu->addAction("Load configuration on startup");
 autoLoadAction->setCheckable(true);autoLoadAction->setChecked(settings.value("autoLoad",false).toBool());
 autoLoadAction->setToolTip("Ask for the passphrase at startup and restore the saved camera layout");
 QObject::connect(autoLoadAction,&QAction::toggled,[&](bool on){settings.setValue("autoLoad",on);settings.sync();window.statusBar()->showMessage(on?"Startup configuration loading enabled.":"Startup configuration loading disabled.",5000);});
 auto *chooseFileAction=configMenu->addAction("Choose configuration file...");
 QObject::connect(chooseFileAction,&QAction::triggered,[&]{auto path=QFileDialog::getOpenFileName(&window,"Configuration file",settings.value("deviceFile").toString(),"OpenWatch devices (*.owv)");if(path.isEmpty())return;settings.setValue("deviceFile",path);settings.sync();autoLoadAction->setChecked(true);window.statusBar()->showMessage("Configuration file: "+path,5000);});
 auto *loadNowAction=configMenu->addAction("Load configuration now");
 QObject::connect(loadNowAction,&QAction::triggered,[&]{auto path=settings.value("deviceFile").toString();if(path.isEmpty()||!QFileInfo::exists(path)){window.statusBar()->showMessage("Choose a configuration file first (Configuration > Choose configuration file...).",6000);return;}loadConfiguration(path);});
 configMenu->addSeparator();
 auto *pathAction=configMenu->addAction("No configuration file chosen");pathAction->setEnabled(false);
 QObject::connect(configMenu,&QMenu::aboutToShow,[&]{auto path=settings.value("deviceFile").toString();pathAction->setText(path.isEmpty()?"No configuration file chosen":path);});
 auto *devicesMenu=menuBar->addMenu("&Devices");
 devicesMenu->addAction("Add device...",addDeviceButton,&QPushButton::click);
 devicesMenu->addAction("Discover devices",discoverButton,&QPushButton::click);
 devicesMenu->addAction("XMEye recorder",xmButton,&QPushButton::click);
 devicesMenu->addAction("Assign group",groupButton,&QPushButton::click);
 devicesMenu->addSeparator();
 devicesMenu->addAction("Edit device",editDevice,&QPushButton::click);
 devicesMenu->addAction("Remove device",removeDevice,&QPushButton::click);
 auto *viewMenu=menuBar->addMenu("&View");
 viewMenu->addAction("Show / hide sidebar",toggle,&QPushButton::click);
 auto *gridMenu=viewMenu->addMenu("Grid size");
 for(int row=0;row<layout->count();++row){auto *sizeAction=gridMenu->addAction(layout->itemText(row));sizeAction->setCheckable(true);sizeAction->setChecked(row==layout->currentIndex());
  QObject::connect(sizeAction,&QAction::triggered,[&,row]{layout->setCurrentIndex(row);});
  QObject::connect(layout,&QComboBox::currentIndexChanged,[&,row,sizeAction](int current){sizeAction->setChecked(current==row);});}
 auto *helpMenu=menuBar->addMenu("&Help");
 helpMenu->addAction(help); // shared with the Others menu
 helpMenu->addAction("About OpenWatch",[&]{QMessageBox::about(&window,"OpenWatch","OpenWatch video workspace\nDVRIP · RTSP · RTMP · HTTP(S) HLS playback, grid views, PTZ, recording and encrypted device lists.");});
 arrange(4);window.statusBar()->showMessage("Ready • Double-click a device or drag it into a grid cell to connect");window.show();
  // Startup configuration loading: prompt for the passphrase and restore the saved
  // camera layout. Never runs during scripted runs (--smoke-test, --screenshot, ...).
  if(settings.value("autoLoad",false).toBool()){
   bool scripted=false;for(const auto &argument:app.arguments())if(argument.startsWith("--"))scripted=true;
   const QString path=settings.value("deviceFile").toString();
   if(!scripted&&!path.isEmpty()&&QFileInfo::exists(path))QTimer::singleShot(0,&window,[&,path]{
    window.statusBar()->showMessage("Loading configuration...",3000);
    for(;;){int result=loadConfiguration(path);
     if(result==2)return; // loaded
     if(result==0)return; // user dismissed the passphrase prompt
     if(QMessageBox::question(&window,"Load configuration","The configuration could not be unlocked.\nTry again?")!=QMessageBox::Yes)return;}
   });
  }
 if(app.arguments().contains("--navigation-smoke")){window.resize(1280,720);
  for(int recorder=1;recorder<=2;++recorder)for(int channel=1;channel<=4;++channel){urls.append(QUrl(QString("dvrip://192.0.2.%1?channel=%2").arg(recorder).arg(channel-1)));list->addItem(QString("Recorder %1 · CH %2").arg(recorder).arg(channel));}
  urls.append(QUrl("rtsp://192.0.2.10/live"));list->addItem("Garden camera");urls.append(QUrl("rtsp://192.0.2.11/live"));list->addItem("Front door");
  QTimer::singleShot(100,[&]{if(tree->topLevelItemCount()!=3||!navPlayback->isVisible()||recordButton->isHidden())qFatal("Navigation smoke failed");toggle->click();if(side->isVisible())qFatal("Sidebar did not collapse");toggle->click();navPlayback->click();auto *tabs=playbackWindow?playbackWindow->findChild<QTabWidget*>("playbackTabs"):nullptr;if(!tabs||tabs->count()!=2)qFatal("Playback tabs missing");tabs->setCurrentIndex(1);tabs->setCurrentIndex(0);bool opened=false;for(auto *widget:app.topLevelWidgets())if(widget!=&window&&widget->isVisible()&&qobject_cast<QDialog*>(widget)){opened=true;widget->close();}if(!opened)qFatal("Playback entry failed");if(app.arguments().contains("--workspace-smoke")){tree->setCurrentItem(tree->topLevelItem(0));removeDevice->click();if(list->count()!=6||urls.size()!=6)qFatal("Recorder removal failed");tree->refresh(list,urls);tree->setCurrentItem(tree->topLevelItem(1));removeDevice->click();if(list->count()!=4||urls.size()!=4)qFatal("Stream section removal failed");}});
 }
 if(app.arguments().contains("--wfs-disk")){int i=app.arguments().indexOf("--wfs-disk");if(i+1<app.arguments().size()){auto *dialog=new WfsTestDialog(&window);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->loadDisk(app.arguments()[i+1]);dialog->show();}}
 if(app.arguments().contains("--wfs-library")){int i=app.arguments().indexOf("--wfs-library");if(i+1<app.arguments().size()){auto *dialog=new WfsTestDialog(&window);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->load(app.arguments()[i+1]);dialog->show();}}
 if(app.arguments().contains("--screenshot")){int i=app.arguments().indexOf("--screenshot");if(i+1<app.arguments().size())QTimer::singleShot((app.arguments().contains("--wfs-library")||app.arguments().contains("--wfs-disk"))?2500:700,[&,i]{(app.activeWindow()?app.activeWindow():&window)->grab().save(app.arguments()[i+1]);});}
 if(app.arguments().contains("--playback-smoke")){auto *dialog=new PlaybackWindow({},&window);dialog->show();dialog->raise();dialog->activateWindow();if(app.arguments().contains("--disk-tab-smoke"))dialog->findChild<QTabWidget*>("playbackTabs")->setCurrentIndex(1);}
 if(app.arguments().contains("--smoke-test"))QTimer::singleShot((app.arguments().contains("--wfs-library")||app.arguments().contains("--wfs-disk"))?3500:1500,&app,&QApplication::quit);
 int result=app.exec();recordingUi.closeAll();for(auto dialog:controls)if(dialog)delete dialog;controls.clear();for(auto *cell:cells)cell->video.requestStop();for(auto *cell:cells)cell->video.stop();return result;
}
