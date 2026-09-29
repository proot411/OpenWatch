#pragma once
#include <QtWidgets>
#include "video.h"
#include "xmcloud.h"
#include "archivetimeline.h"
#include "networkcoverage.h"
#include "dvrip.h"

// Recording coverage is queried independently of playback; seeking remains available on query failure.
class NetworkPlaybackDialog:public QDialog {
 Video video;
 QComboBox *mode;
 QLineEdit *address,*user,*password;
 QSpinBox *port,*channel,*minutes;
 QCalendarWidget *calendar;
 QTimeEdit *time,*goTime;
 archive::Timeline *dayline;bool scrubbing=false;
 QLabel *picture,*state,*clock,*summary,*videoState;
 QSlider *slider;
 QPushButton *play,*back,*forward,*stop;
 QWidget *connectionPanel;
 QDateTime beginning,ending,cursor;
 QUrl base;
 int activeChannel=0;
 bool running=false,ready=false;
 QTimer timer;
 QThread *coverageWorker=nullptr;
 std::atomic_bool cancelCoverage{false};
 quint64 coverageGeneration=0;
 QString coverageKey;
 bool pendingCoverage=false;
 QLabel *legend;
 QPushButton *findRecordings;
 void invalidateCoverage(){
  ++coverageGeneration;cancelCoverage=true;coverageKey.clear();dayline->clips.clear();dayline->update();
  legend->setText("Coverage unknown · Recorder time");summary->setText("Choose Find recordings to highlight available footage.");
 }
 QUrl recorderSource()const{
  QUrl source;source.setScheme("dvrip");source.setHost(mode->currentIndex()?"xmeye-cloud":address->text().trimmed());source.setPort(port->value());source.setUserName(user->text());source.setPassword(password->text());
  if(mode->currentIndex()){QUrlQuery query;query.addQueryItem("cloudId",address->text().trimmed());source.setQuery(query);}return source;
 }
 void searchCoverage(bool force=false){
  if(coverageWorker){pendingCoverage=true;return;}
  QHostAddress ip;
  if(user->text().isEmpty()||(mode->currentIndex()? !xm::cloud::validSerial(address->text().trimmed()): !ip.setAddress(address->text().trimmed()))){connectionPanel->show();summary->setText("Enter the recorder connection details first.");return;}
  const auto source=recorderSource();const auto day=calendar->selectedDate();const int ch=channel->value()-1;
  const QString key=source.toString()+day.toString(Qt::ISODate)+QString::number(ch);
  if(!force&&coverageKey==key)return;
  invalidateCoverage();const auto generation=coverageGeneration;cancelCoverage=false;
  findRecordings->setEnabled(false);findRecordings->setText("Searching…");legend->setText("Searching recording coverage…");summary->setText("Reading the recorder's file list for this camera and day…");
  coverageWorker=QThread::create([this,source,day,ch,key,generation]{
   QJsonArray all;QString note;bool partial=false;
   QString begin=day.toString("yyyy-MM-dd")+" 00:00:00",end=day.toString("yyyy-MM-dd")+" 23:59:59";
   QSet<QString> seen;
   try{
    for(int page=0;page<128&&!cancelCoverage;++page){
     xm::Client client(cancelCoverage);auto rows=client.recordings(source,ch,begin,end);QString latest=begin;
     for(const auto &v:rows){auto o=v.toObject();auto b=o["BeginTime"].toString();auto e=o["EndTime"].toString();
      if(QDateTime::fromString(b,"yyyy-MM-dd HH:mm:ss").isValid())latest=qMax(latest,b);
      auto id=o["FileName"].toString()+b+e;if(!seen.contains(id)){seen.insert(id);all.append(v);}}
     if(rows.size()<64)break;
     if(latest<=begin||page==127){partial=true;break;}begin=latest;
    }
   }catch(const std::exception &){partial=true;note="The recorder could not complete the file search. Manual seeking is still available.";}
   auto clips=archive::recordingRanges(all,day,ch);
   QMetaObject::invokeMethod(this,[this,clips,note,partial,key,generation]{
    if(generation!=coverageGeneration)return;
    if(!partial)coverageKey=key;dayline->clips=clips;dayline->update();
    legend->setText(clips.isEmpty()?(partial?"Coverage unavailable · Recorder time":"No ranges reported · Recorder time"):(partial?"Teal: reported recordings · Partial coverage":"Teal: available recordings · Recorder time"));
    summary->setText(QString("%1 recording ranges reported.%2\n%3").arg(clips.size()).arg(partial?" Search incomplete.":"").arg(note.isEmpty()?(clips.isEmpty()?"No files were returned. This does not rule out footage on incompatible firmware; you can still seek manually.":"Click a teal range to play that time."):note));
   },Qt::QueuedConnection);
  });
  connect(coverageWorker,&QThread::finished,this,[this]{coverageWorker->wait();delete coverageWorker;coverageWorker=nullptr;findRecordings->setEnabled(true);findRecordings->setText("Find recordings");if(pendingCoverage){pendingCoverage=false;searchCoverage();}});coverageWorker->start();
 }

 void halt(){video.requestStop();running=false;play->setText("Resume");}
 void startAt(QDateTime stamp){
  if(!ready)return;
  video.stop();stamp=qBound(beginning,stamp,ending.addSecs(-1));cursor=stamp;
  picture->setText("Opening recorder footage…");
  video.start(playbackUrl(base,activeChannel,stamp,ending));
  running=true;play->setText("Pause");
 }
 void load(){
  auto invalid=[this](QString message){connectionPanel->show();state->setText(message);};
  if(mode->currentIndex()==1&&!xm::cloud::validSerial(address->text().trimmed())){invalid("Enter a valid recorder serial number in Connection settings.");return;}
  QHostAddress ip;
  if(mode->currentIndex()==0&&(!ip.setAddress(address->text().trimmed())||ip.isNull()||ip.isMulticast()||ip==QHostAddress::Broadcast)){invalid("Enter the recorder IP address in Connection settings.");return;}
  if(user->text().isEmpty()){invalid("Enter the recorder username in Connection settings.");return;}
  video.stop();base=QUrl();base.setScheme("dvrip");base.setHost(mode->currentIndex()?"xmeye-cloud":address->text().trimmed());base.setPort(port->value());base.setUserName(user->text());base.setPassword(password->text());
  if(mode->currentIndex()){QUrlQuery query;query.addQueryItem("cloudId",address->text().trimmed());base.setQuery(query);}
  // Preserve recorder wall-clock fields without applying the PC timezone.
  activeChannel=channel->value()-1;beginning=QDateTime(calendar->selectedDate(),time->time(),Qt::UTC);ending=beginning.addSecs(minutes->value()*60);cursor=beginning;ready=true;
  slider->setRange(0,minutes->value()*60-1);slider->setValue(0);slider->setEnabled(true);
  searchCoverage();
  dayline->channels={activeChannel};dayline->setPosition(beginning.time().msecsSinceStartOfDay()/1000);goTime->setTime(beginning.time());
  for(auto *button:{back,forward,stop})button->setEnabled(true);
  connectionPanel->hide();startAt(cursor);
 }
 void seekDay(int second){time->setTime(QTime(0,0).addSecs(second));load();}
 void step(int delta){if(!ready)return;auto target=cursor.addSecs(delta);if(target>=beginning&&target<ending)startAt(target);else{calendar->setSelectedDate(target.date());channel->setValue(activeChannel+1);time->setTime(target.time());load();}}
public:
 static QUrl playbackUrl(QUrl source,int channel,const QDateTime &begin,const QDateTime &end){
  QUrlQuery query(source);
  for(const auto &key:{"archiveFile","playMode","channel","begin","end","speed"})query.removeAllQueryItems(key);
  query.addQueryItem("archiveFile","time");query.addQueryItem("playMode","ByTime");query.addQueryItem("channel",QString::number(channel));
  query.addQueryItem("begin",begin.toString("yyyy-MM-dd HH:mm:ss"));query.addQueryItem("end",end.toString("yyyy-MM-dd HH:mm:ss"));query.addQueryItem("speed","1");source.setQuery(query);return source;
 }
 NetworkPlaybackDialog(QUrl initial={},QWidget *parent=nullptr):QDialog(parent){
  setWindowTitle("Network playback");resize(1100,730);
  setMinimumSize(960,680);auto *root=new QVBoxLayout(this);root->setContentsMargins(16,12,16,12);root->setSpacing(10);
  auto *heading=new QHBoxLayout;auto *title=new QLabel("Recorder playback");title->setStyleSheet("font-size:22px;font-weight:600;");heading->addWidget(title);heading->addStretch();
  auto *settings=new QPushButton("Connection settings");heading->addWidget(settings);root->addLayout(heading);
  connectionPanel=new QWidget;connectionPanel->setObjectName("playbackConnection");auto *form=new QGridLayout(connectionPanel);form->setContentsMargins(0,0,0,0);root->addWidget(connectionPanel);
  mode=new QComboBox;mode->addItems({"Local / VPN","XMEye cloud"});mode->setCurrentIndex(QUrlQuery(initial).hasQueryItem("cloudId")?1:0);
  address=new QLineEdit(mode->currentIndex()?QUrlQuery(initial).queryItemValue("cloudId"):initial.host());address->setPlaceholderText("Recorder IP or serial number");
  port=new QSpinBox;port->setRange(1,65535);port->setValue(initial.port(34567));
  user=new QLineEdit(initial.userName().isEmpty()?"admin":initial.userName());password=new QLineEdit(initial.password());password->setEchoMode(QLineEdit::Password);
  form->addWidget(mode,0,0);form->addWidget(address,0,1);form->addWidget(new QLabel("Port"),0,2);form->addWidget(port,0,3);
  form->addWidget(new QLabel("Username"),1,0);form->addWidget(user,1,1);form->addWidget(new QLabel("Password"),1,2);form->addWidget(password,1,3);
  port->setEnabled(!mode->currentIndex());connect(mode,&QComboBox::currentIndexChanged,this,[this]{port->setEnabled(!mode->currentIndex());});
  connectionPanel->hide();connect(settings,&QPushButton::clicked,this,[this]{connectionPanel->setVisible(!connectionPanel->isVisible());});
  auto *body=new QHBoxLayout;body->setSpacing(12);root->addLayout(body,1);
  auto *player=new QWidget;auto *playerLayout=new QVBoxLayout(player);playerLayout->setContentsMargins(0,0,0,0);playerLayout->setSpacing(0);
  picture=new QLabel("Choose a camera and date, then select a time below");picture->setAlignment(Qt::AlignCenter);picture->setMinimumSize(480,240);picture->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Ignored);picture->setStyleSheet("background:#090f17;border:1px solid #304259;color:#91a6bc;");playerLayout->addWidget(picture,1);videoState=new QLabel("Select a time · Ready");videoState->setStyleSheet("background:#172231;color:#adbed0;padding:8px;");playerLayout->addWidget(videoState);body->addWidget(player,1);
  auto *scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);scroll->setFixedWidth(280);auto *side=new QWidget;scroll->setWidget(side);body->addWidget(scroll);auto *sideLayout=new QVBoxLayout(side);sideLayout->setContentsMargins(0,0,0,0);sideLayout->setSpacing(8);sideLayout->setSizeConstraint(QLayout::SetNoConstraint);
  auto *badge=new QLabel("●  NETWORK PLAYBACK");badge->setStyleSheet("color:#54dfba;font-size:11px;font-weight:600;padding:8px;background:#142823;border-radius:5px;");sideLayout->addWidget(badge);
  calendar=new QCalendarWidget;calendar->setFixedHeight(180);calendar->setMinimumWidth(0);calendar->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);calendar->setStyleSheet("QAbstractItemView{background:#111b28;alternate-background-color:#1c2b3d;selection-background-color:#2785a3;color:#dbe5ef;} QToolButton{padding:4px;}");calendar->setHorizontalHeaderFormat(QCalendarWidget::SingleLetterDayNames);calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
  for(int day=1;day<=7;++day){QTextCharFormat format;format.setForeground(QColor(day>=6?"#ff7889":"#b7c9d9"));format.setBackground(QColor("#172231"));calendar->setWeekdayTextFormat(Qt::DayOfWeek(day),format);}sideLayout->addWidget(calendar);
  channel=new QSpinBox;channel->setRange(1,256);channel->setValue(QUrlQuery(initial).queryItemValue("channel").toInt()+1);
  time=new QTimeEdit(QTime::currentTime().addSecs(-300));time->setDisplayFormat("HH:mm:ss");time->setParent(this);time->hide();minutes=new QSpinBox;minutes->setRange(1,60);minutes->setValue(5);minutes->setSuffix(" minutes");
  auto *choices=new QFormLayout;choices->addRow("Camera",channel);sideLayout->addLayout(choices);
  auto *loadButton=new QPushButton("Play selected time");sideLayout->addWidget(loadButton);connect(loadButton,&QPushButton::clicked,this,[this]{seekDay(goTime->time().msecsSinceStartOfDay()/1000);});
  findRecordings=new QPushButton("Find recordings");sideLayout->addWidget(findRecordings);connect(findRecordings,&QPushButton::clicked,this,[this]{searchCoverage(true);});
  summary=new QLabel("Choose Find recordings to highlight footage for this camera and day.");summary->setWordWrap(true);summary->setStyleSheet("color:#93a9bb;font-size:11px;");sideLayout->addWidget(summary);sideLayout->addStretch();
  auto *bar=new QHBoxLayout;root->addLayout(bar);auto *previous=new QPushButton("|◀");previous->setToolTip("Previous time range");back=new QPushButton("−10 s");play=new QPushButton("Play");play->setMinimumWidth(100);forward=new QPushButton("+10 s");auto *next=new QPushButton("▶|");next->setToolTip("Next time range");stop=new QPushButton("Stop");
  for(auto *button:{previous,back,play,forward,next,stop})bar->addWidget(button);
  connect(previous,&QPushButton::clicked,this,[this]{step(-minutes->value()*60);});connect(next,&QPushButton::clicked,this,[this]{step(minutes->value()*60);});bar->addStretch();
  clock=new QLabel("No footage loaded");clock->setStyleSheet("color:#ffd27c;font-size:15px;font-weight:600;");bar->addWidget(clock);
  for(auto *button:{back,forward,stop})button->setEnabled(false);
  connect(back,&QPushButton::clicked,this,[this]{step(-10);});connect(forward,&QPushButton::clicked,this,[this]{step(10);});connect(stop,&QPushButton::clicked,this,[this]{halt();});
  connect(play,&QPushButton::clicked,this,[this]{if(running)halt();else if(ready)startAt(cursor);else seekDay(goTime->time().msecsSinceStartOfDay()/1000);});
  slider=new QSlider(Qt::Horizontal);slider->setRange(0,299);slider->setEnabled(false);slider->setToolTip("Drag to seek within the selected time range. Teal day-timeline ranges show reported recordings; seeking may land on a nearby keyframe.");slider->setMaximumHeight(12);root->addWidget(slider);
  connect(slider,&QSlider::sliderReleased,this,[this]{if(ready)startAt(beginning.addSecs(slider->value()));});
  dayline=new archive::Timeline;dayline->setObjectName("networkDayTimeline");dayline->setFixedHeight(120);dayline->channels={channel->value()-1};root->addWidget(dayline);
  auto *bottom=new QHBoxLayout;legend=new QLabel("Coverage unknown · Recorder time");legend->setStyleSheet("color:#93a9bb;font-size:11px;");bottom->addWidget(legend);bottom->addStretch();
  goTime=new QTimeEdit(time->time());goTime->setDisplayFormat("HH:mm:ss");bottom->addWidget(goTime);auto *go=new QPushButton("Go");bottom->addWidget(go);minutes->setSuffix(" min");minutes->setToolTip("Length of the playback request");bottom->addWidget(minutes);
  auto *range=new QComboBox;for(auto pair:QList<QPair<QString,int>>{{"24 hours",86400},{"2 hours",7200},{"1 hour",3600},{"30 minutes",1800}})range->addItem(pair.first,pair.second);bottom->addWidget(range);root->addLayout(bottom);
  connect(range,&QComboBox::currentIndexChanged,this,[this,range]{dayline->zoom(range->currentData().toInt());});
  connect(go,&QPushButton::clicked,this,[this]{seekDay(goTime->time().msecsSinceStartOfDay()/1000);});
  dayline->scrub=[this](int second,bool commit){scrubbing=!commit;goTime->setTime(QTime(0,0).addSecs(second));if(commit)seekDay(second);};
  connect(channel,&QSpinBox::valueChanged,this,[this](int value){invalidateCoverage();dayline->channels={value-1};dayline->update();});
  connect(calendar,&QCalendarWidget::selectionChanged,this,[this]{invalidateCoverage();});
  for(auto *field:{address,user,password})connect(field,&QLineEdit::textChanged,this,[this]{invalidateCoverage();});
  connect(port,&QSpinBox::valueChanged,this,[this]{invalidateCoverage();});connect(mode,&QComboBox::currentIndexChanged,this,[this]{invalidateCoverage();});
  state=new QLabel("Click or drag the timeline to seek. Space: play/pause · ←/→: 10 seconds · Shift+←/→: 1 minute");state->setStyleSheet("color:#93a9bb;font-size:11px;");state->setWordWrap(true);state->setTextFormat(Qt::PlainText);root->addWidget(state);
  for(auto pair:QList<QPair<int,int>>{{Qt::Key_Left,-10},{Qt::Key_Right,10},{Qt::Key_Space,0},{Qt::SHIFT|Qt::Key_Left,-60},{Qt::SHIFT|Qt::Key_Right,60}}){
   auto *key=new QShortcut(QKeySequence(pair.first),this);key->setContext(Qt::WidgetWithChildrenShortcut);
   connect(key,&QShortcut::activated,this,[this,pair]{auto *focus=QApplication::focusWidget();if(qobject_cast<QLineEdit*>(focus)||qobject_cast<QAbstractSpinBox*>(focus)||qobject_cast<QComboBox*>(focus)||(focus&&calendar->isAncestorOf(focus)))return;
    if(pair.second)step(pair.second);else play->click();});
  }
  connect(&timer,&QTimer::timeout,this,[this]{
   if(!ready)return;
   auto sample=video.archiveFrame();if(!sample.first.isNull()){
    picture->setPixmap(QPixmap::fromImage(sample.first).scaled(picture->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
    if(sample.second>=0){cursor=QDateTime::fromMSecsSinceEpoch(sample.second,Qt::UTC);clock->setText(QString("CH %1 · %2").arg(activeChannel+1).arg(cursor.toString("HH:mm:ss")));if(!slider->isSliderDown())slider->setValue(qBound(0,int(beginning.secsTo(cursor)),slider->maximum()));if(!scrubbing&&cursor.date()==calendar->selectedDate()&&channel->value()==activeChannel+1){dayline->setPosition(cursor.time().msecsSinceStartOfDay()/1000);if(!goTime->hasFocus())goTime->setTime(cursor.time());}}
   }
   auto status=video.status();videoState->setText(QString("CH %1 · %2").arg(activeChannel+1).arg(status));
   if(running&&(status=="Playback complete"||(!status.startsWith("Playback")&&!status.startsWith("Connecting")&&!status.contains(QChar(0x2026))&&!status.isEmpty()))){running=false;play->setText("Resume");}
  });timer.start(100);
 }
 ~NetworkPlaybackDialog()override{cancelCoverage=true;if(coverageWorker){coverageWorker->wait();delete coverageWorker;}video.stop();}
 void suspend(){halt();video.stop();}
 void reject()override{suspend();if(isWindow())QDialog::reject();}
};
