#pragma once
#include <QtWidgets>
#include "cell.h"
#include "wfsdisk.h"
#include "archivetimeline.h"
#include <future>

// Opens a prepared library of copied clips, never a disk device.
class WfsTestDialog : public QDialog {
 struct Clip {QString file;int channel;QDateTime start;int duration;int primary=-1;QString identity;};
 QVector<Clip> clips;
 QCalendarWidget *calendar;
 QListWidget *list;
 Cell *cell;
 QSlider *timeline;archive::Timeline *dayline;QTimeEdit *goTime;int scrubChannel=0;bool scrubbing=false;
 QLabel *clock, *message;
 QPushButton *play;
 QComboBox *speed,*channels;
 QLabel *summary;QSet<QDate> marked;
 std::future<wfs::Index> scan;std::shared_ptr<std::atomic_bool> cancelScan=std::make_shared<std::atomic_bool>(false);
 int current=-1;
 qint64 position=0,pendingWall=-1;
 bool running=false;
 void start(){
  if(current<0)return;
  auto url=QUrl::fromLocalFile(clips[current].file);QUrlQuery query;
  query.addQueryItem("seekMs",QString::number(position));query.addQueryItem("speed",speed->currentText().chopped(1));if(clips[current].primary>=0){query.addQueryItem("wfsPrimary",QString::number(clips[current].primary));query.addQueryItem("wfsIdentity",clips[current].identity);if(pendingWall>=0)query.addQueryItem("wfsTime",QString::number(pendingWall));}url.setQuery(query);
  cell->video.start(url);pendingWall=-1;running=true;play->setText("Pause");
 }
 void seek(qint64 value){if(current<0)return;position=qBound<qint64>(qint64(0),value,qint64(qMax(0,clips[current].duration-100)));start();}
 void skip(qint64 delta){
  if(current<0){daySeek(qBound(0,dayline->position()+int(delta/1000),86399));return;}
  if(!cell->video.frame().isNull())position=cell->video.playbackMilliseconds();
  qint64 target=position+delta;
  if(target>=0&&target<clips[current].duration){pendingWall=-1;seek(target);return;}
  auto frame=cell->video.archiveFrame();qint64 base=frame.second>=0?frame.second-cell->video.playbackMilliseconds():clips[current].start.toMSecsSinceEpoch();
  auto time=QDateTime::fromMSecsSinceEpoch(base+target,Qt::UTC);scrubChannel=clips[current].channel;
  if(time.date()==calendar->selectedDate())daySeek(time.time().msecsSinceStartOfDay()/1000);else message->setText("Reached the edge of the selected day.");
 }
 void toggle(){if(current<0)return;if(running){if(!cell->video.frame().isNull())position=cell->video.playbackMilliseconds();cell->video.stop();running=false;play->setText("Play");}else{if(position>=clips[current].duration-150)position=0;start();}}
 void daySeek(int seconds){
  int channel=scrubChannel?scrubChannel:(current>=0?clips[current].channel:channels->currentData().toInt());
  auto target=QDateTime(calendar->selectedDate(),QTime(0,0),Qt::UTC).addSecs(seconds);
  for(int i=0;i<clips.size();++i){auto &c=clips[i];if((!channel||c.channel==channel)&&target>=c.start&&target<c.start.addMSecs(c.duration)){
   {QSignalBlocker block(list);for(int row=0;row<list->count();++row)if(list->item(row)->data(Qt::UserRole).toInt()==i)list->setCurrentRow(row);}
   current=i;position=c.start.msecsTo(target);pendingWall=c.primary>=0?target.toMSecsSinceEpoch():-1;timeline->setRange(0,c.duration);cell->name=QString("CH %1 · Recording").arg(c.channel);start();return;
  }}
  cell->video.clear();running=false;current=-1;position=0;pendingWall=-1;{QSignalBlocker block(list);list->setCurrentRow(-1);}timeline->setRange(0,0);dayline->setPosition(seconds);clock->setText(QTime(0,0).addSecs(seconds).toString("HH:mm:ss")+" · No recording");play->setText("Play");message->setText("No recording at the selected time on this channel. Choose a teal section.");
 }
 void adjacent(int direction){
  if(current<0)return;int next=-1;for(int i=0;i<clips.size();++i){if(clips[i].channel!=clips[current].channel||clips[i].start.date()!=calendar->selectedDate())continue;
   if(direction>0&&clips[i].start>clips[current].start&&(next<0||clips[i].start<clips[next].start))next=i;
   if(direction<0&&clips[i].start<clips[current].start&&(next<0||clips[i].start>clips[next].start))next=i;
  }
  if(next>=0){scrubChannel=clips[next].channel;daySeek(clips[next].start.time().msecsSinceStartOfDay()/1000);}else message->setText("No more recordings on this channel for the selected day.");
 }
 void highlight(){
  for(auto day:marked)calendar->setDateTextFormat(day,QTextCharFormat());marked.clear();QTextCharFormat format;format.setBackground(QColor("#146b60"));format.setForeground(Qt::white);format.setFontWeight(QFont::Bold);
  int channel=channels->currentData().toInt();for(auto &clip:clips){if(channel&&clip.channel!=channel)continue;auto last=clip.start.addMSecs(qMax(0,clip.duration-1)).date();for(auto day=clip.start.date();day<=last;day=day.addDays(1))marked.insert(day);}
  for(auto day:marked)calendar->setDateTextFormat(day,format);
 }
 void setChannels(){QSignalBlocker block(channels);channels->clear();channels->addItem("All channels",0);QSet<int> ids;for(auto &clip:clips)ids.insert(clip.channel);auto values=ids.values();std::sort(values.begin(),values.end());for(int id:values)channels->addItem(QString("Channel %1").arg(id),id);highlight();}
 void refresh(){
  cell->video.stop();running=false;current=-1;position=0;pendingWall=-1;play->setText("Play");cell->video.clear();list->clear();timeline->setRange(0,0);clock->setText("Select a recording");
  for(int i=0;i<clips.size();++i)if((!channels->currentData().toInt()||clips[i].channel==channels->currentData().toInt()) && clips[i].start.date()<=calendar->selectedDate() && clips[i].start.addMSecs(qMax(0,clips[i].duration-1)).date()>=calendar->selectedDate()){
   auto *item=new QListWidgetItem(QString("CH %1 · %2 · %3 s").arg(clips[i].channel).arg(clips[i].start.time().toString("HH:mm:ss")).arg(clips[i].duration/1000.0,0,'f',1),list);item->setData(Qt::UserRole,i);
  }
  dayline->clips.clear();dayline->channels.clear();auto midnight=QDateTime(calendar->selectedDate(),QTime(0,0),Qt::UTC);
  for(int row=0;row<list->count();++row){auto &c=clips[list->item(row)->data(Qt::UserRole).toInt()];int ch=c.channel-1;if(!dayline->channels.contains(ch))dayline->channels.append(ch);int begin=qMax(0,int(midnight.msecsTo(c.start)/1000));int end=qMin(86400,int((midnight.msecsTo(c.start)+c.duration+999)/1000));dayline->clips.append({ch,begin,end,{}});}
  std::sort(dayline->channels.begin(),dayline->channels.end());dayline->setFixedHeight(qBound(100,40+int(dayline->channels.size())*28,240));dayline->setPosition(0);scrubChannel=0;
  summary->setText(QString("%1 recordings · %2 on selected day · Highlighted dates contain footage").arg(clips.size()).arg(list->count()));
  if(list->count())list->setCurrentRow(0);
 }
public:
 explicit WfsTestDialog(QWidget *parent=nullptr):QDialog(parent){
  setWindowTitle("OpenWatch · Recorder disk playback");resize(1150,760);
  setMinimumSize(960,680);
  auto *layout=new QVBoxLayout(this);layout->setContentsMargins(16,12,16,12);layout->setSpacing(10);
  auto *heading=new QHBoxLayout;auto *title=new QLabel("Recorder playback");title->setStyleSheet("font-size:22px;font-weight:600;color:#e5eef5;");heading->addWidget(title);heading->addStretch();auto *open=new QPushButton("Open library");auto *diskOpen=new QPushButton("Open disk / image");heading->addWidget(open);heading->addWidget(diskOpen);layout->addLayout(heading);
  auto *body=new QHBoxLayout;body->setSpacing(12);layout->addLayout(body,1);
  cell=new Cell;cell->name="Select a recording";cell->setMinimumSize(480,280);body->addWidget(cell,1);
  auto *sideWidget=new QWidget;sideWidget->setFixedWidth(280);auto *side=new QVBoxLayout(sideWidget);side->setContentsMargins(0,0,0,0);side->setSpacing(8);body->addWidget(sideWidget);
  auto *badge=new QLabel("●  READ-ONLY DISK PLAYBACK");badge->setStyleSheet("color:#54dfba;font-size:11px;font-weight:600;padding:8px;background:#142823;border-radius:5px;");side->addWidget(badge);
  calendar=new QCalendarWidget;calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);calendar->setHorizontalHeaderFormat(QCalendarWidget::SingleLetterDayNames);calendar->setStyleSheet("QAbstractItemView{background:#111b28;alternate-background-color:#1c2b3d;selection-background-color:#2785a3;color:#dbe5ef;} QToolButton{padding:4px;}");side->addWidget(calendar);
  channels=new QComboBox;channels->setObjectName("wfsChannels");channels->addItem("All channels",0);side->addWidget(channels);
  summary=new QLabel;summary->setWordWrap(true);summary->setStyleSheet("color:#93a9bb;font-size:11px;");side->addWidget(summary);
  list=new QListWidget;list->setMinimumHeight(80);side->addWidget(list,1);
  auto *buttons=new QHBoxLayout;layout->addLayout(buttons);
  auto *previous=new QPushButton("|◀");previous->setToolTip("Previous recording on this channel");auto *back=new QPushButton("−10 s");play=new QPushButton("Play");play->setMinimumWidth(100);auto *forward=new QPushButton("+10 s");auto *next=new QPushButton("▶|");next->setToolTip("Next recording on this channel");
  buttons->addWidget(previous);buttons->addWidget(back);buttons->addWidget(play);buttons->addWidget(forward);buttons->addWidget(next);
  speed=new QComboBox;speed->addItems({"0.25×","0.5×","1×","2×","4×","8×"});speed->setCurrentIndex(2);buttons->addWidget(speed);buttons->addStretch();clock=new QLabel("Select a recording");clock->setStyleSheet("color:#ffd27c;font-size:15px;font-weight:600;");buttons->addWidget(clock);
  timeline=new QSlider(Qt::Horizontal);timeline->setRange(0,0);timeline->setToolTip("Position within current recording");timeline->setMaximumHeight(12);layout->addWidget(timeline);
  dayline=new archive::Timeline;dayline->setObjectName("wfsDayTimeline");dayline->setMinimumHeight(100);dayline->setFixedHeight(120);layout->addWidget(dayline);
  auto *bottom=new QHBoxLayout;auto *legend=new QLabel("● Recorded     Dark areas: no indexed footage");legend->setStyleSheet("color:#54dfba;font-size:11px;");bottom->addWidget(legend);bottom->addStretch();goTime=new QTimeEdit;goTime->setDisplayFormat("HH:mm:ss");bottom->addWidget(goTime);auto *go=new QPushButton("Go");bottom->addWidget(go);
  auto *range=new QComboBox;range->addItem("24 hours",86400);range->addItem("2 hours",7200);range->addItem("1 hour",3600);range->addItem("30 minutes",1800);bottom->addWidget(range);layout->addLayout(bottom);
  message=new QLabel("Choose a highlighted date, then click or drag a recorded timeline section. Space: play/pause · ←/→: 10 seconds · Shift+←/→: 1 minute");message->setWordWrap(true);message->setStyleSheet("color:#93a9bb;font-size:11px;");layout->addWidget(message);
  connect(previous,&QPushButton::clicked,this,[this]{adjacent(-1);});connect(next,&QPushButton::clicked,this,[this]{adjacent(1);});
  connect(range,&QComboBox::currentIndexChanged,this,[this,range]{dayline->zoom(range->currentData().toInt());});
  connect(go,&QPushButton::clicked,this,[this]{scrubChannel=current>=0?clips[current].channel:0;daySeek(goTime->time().msecsSinceStartOfDay()/1000);});
  dayline->selectChannel=[this](int channel){scrubChannel=channel+1;};
  dayline->scrub=[this](int second,bool commit){scrubbing=!commit;goTime->setTime(QTime(0,0).addSecs(second));if(commit)daySeek(second);};
  connect(diskOpen,&QPushButton::clicked,this,[this]{bool ok=false;auto path=QInputDialog::getText(this,"Read recorder disk / image","Disk device or image path (read access required)",QLineEdit::Normal,"/dev/sdb",&ok);if(ok&&!path.isEmpty())loadDisk(path);});
  connect(channels,&QComboBox::currentIndexChanged,this,[this]{highlight();refresh();});
  connect(open,&QPushButton::clicked,this,[this]{auto path=QFileDialog::getOpenFileName(this,"Open prepared library",{},"WFS library (library.json)");if(!path.isEmpty())load(path);});
  connect(calendar,&QCalendarWidget::selectionChanged,this,[this]{refresh();});
  connect(list,&QListWidget::currentItemChanged,this,[this](QListWidgetItem *item){if(!item)return;current=item->data(Qt::UserRole).toInt();pendingWall=-1;position=qMax<qint64>(0,clips[current].start.msecsTo(QDateTime(calendar->selectedDate(),QTime(0,0),Qt::UTC)));timeline->setRange(0,clips[current].duration);cell->name=QString("CH %1 · Recording").arg(clips[current].channel);start();});
  connect(play,&QPushButton::clicked,this,[this]{toggle();});connect(back,&QPushButton::clicked,this,[this]{skip(-10000);});connect(forward,&QPushButton::clicked,this,[this]{skip(10000);});
  connect(timeline,&QSlider::sliderReleased,this,[this]{seek(timeline->value());});
  connect(speed,&QComboBox::currentIndexChanged,this,[this]{if(running)start();});
  for(auto key:{Qt::Key_Space,Qt::Key_Left,Qt::Key_Right}){auto *shortcut=new QShortcut(QKeySequence(key),this);shortcut->setContext(Qt::WidgetWithChildrenShortcut);connect(shortcut,&QShortcut::activated,this,[this,key]{if(key==Qt::Key_Space)toggle();else {skip(key==Qt::Key_Left?-10000:10000);}});}
  for(auto key:{Qt::Key_Left,Qt::Key_Right}){auto *shortcut=new QShortcut(QKeySequence(Qt::SHIFT|key),this);shortcut->setContext(Qt::WidgetWithChildrenShortcut);connect(shortcut,&QShortcut::activated,this,[this,key]{skip(key==Qt::Key_Left?-60000:60000);});}
  auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{if(scan.valid()&&scan.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{auto index=scan.get();QVector<Clip> next;for(auto &c:index.clips)next.append({index.path,c.channel,QDateTime::fromMSecsSinceEpoch(c.begin,Qt::UTC),int(qMax<qint64>(1,c.end-c.begin)),int(c.primary),index.identity});cell->video.stop();clips=next;setChannels();if(!clips.isEmpty()){QSignalBlocker block(calendar);calendar->setSelectedDate(clips.last().start.date());}refresh();message->setText(QString("Read-only disk · %1 indexed recordings · %2 invalid chains skipped. Unused/stale entries are not guaranteed filtered.").arg(clips.size()).arg(index.rejected));}catch(const std::exception &e){message->setText(QString::fromUtf8(e.what()));}}
   cell->update();if(current<0)return;if(running){if(!cell->video.frame().isNull())position=cell->video.playbackMilliseconds();auto state=cell->video.status();message->setText(state);if(state=="Playback complete"){running=false;play->setText("Play");}}
   if(!timeline->isSliderDown())timeline->setValue(int(position));clock->setText(QString("CH %1  ·  %2").arg(clips[current].channel).arg(((clips[current].primary>=0&&cell->video.archiveFrame().second>=0)?QDateTime::fromMSecsSinceEpoch(cell->video.archiveFrame().second,Qt::UTC):clips[current].start.addMSecs(position)).toString("HH:mm:ss")));if(!scrubbing){auto time=(clips[current].primary>=0&&cell->video.archiveFrame().second>=0)?QDateTime::fromMSecsSinceEpoch(cell->video.archiveFrame().second,Qt::UTC):clips[current].start.addMSecs(position);if(time.date()==calendar->selectedDate()){dayline->setPosition(time.time().msecsSinceStartOfDay()/1000);if(!goTime->hasFocus())goTime->setTime(time.time());}}
  });timer->start(50);
 }
 void suspend(){cell->video.stop();running=false;play->setText("Play");}
 void reject()override{suspend();if(isWindow())QDialog::reject();}
 ~WfsTestDialog(){*cancelScan=true;cell->video.stop();if(scan.valid())scan.wait();}
 void loadDisk(const QString &path){if(scan.valid()){message->setText("Disk scan already in progress");return;}cell->video.stop();running=false;play->setText("Play");*cancelScan=false;auto cancel=cancelScan;message->setText("Reading full recording index…");scan=std::async(std::launch::async,[path,cancel]{return wfs::Index::read(path,*cancel);});}
 bool load(const QString &path){
  if(scan.valid()){message->setText("Wait for disk scan to finish");return false;}
  QFile file(path);if(!file.open(QIODevice::ReadOnly)||file.size()>1024*1024){message->setText("Cannot read library (limit 1 MiB)");return false;}
  QJsonParseError error;auto doc=QJsonDocument::fromJson(file.readAll(),&error);auto rows=doc.object()["clips"].toArray();QVector<Clip> next;
  auto root=QFileInfo(path).canonicalPath();
  if(error.error!=QJsonParseError::NoError||doc.object()["version"].toInt()!=1||rows.isEmpty()||rows.size()>1000){message->setText("Invalid library");return false;}
  for(auto row:rows){auto o=row.toObject();auto name=o["file"].toString();auto info=QFileInfo(QDir(root).filePath(name));auto start=QDateTime::fromString(o["start"].toString(),"yyyy-MM-dd HH:mm:ss");start.setTimeSpec(Qt::UTC);int duration=o["duration_ms"].toInt();int channel=o["channel"].toInt();
   if(QFileInfo(name).fileName()!=name||info.canonicalPath()!=root||!info.isFile()||info.suffix()!="mkv"||!start.isValid()||duration<=0||duration>86400000||channel<1||channel>256){message->setText("Invalid clip path or metadata");return false;}
   next.append({info.canonicalFilePath(),channel,start,duration});
  }
  cell->video.stop();clips=next;setChannels();{QSignalBlocker block(calendar);calendar->setSelectedDate(clips[0].start.date());}refresh();return true;
 }
};
