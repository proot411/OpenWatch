#pragma once
#include <QtWidgets>
#include "video.h"
#include "dvrip.h"
#include "archivetimeline.h"
#include <array>
class ArchiveDialog:public QDialog {
 QLineEdit *host,*user,*password,*channelText;QSpinBox *port;QCalendarWidget *calendar;
 QPushButton *search,*toggle;QLabel *status,*clockLabel;archive::Timeline *timeline;QTimeEdit *time;
 QWidget *connection;QGridLayout *grid;QComboBox *layoutMode,*speed;
 struct View {Video video;QLabel *picture=nullptr,*label=nullptr;QWidget *panel=nullptr;int clip=-1,start=0;};std::array<View,4> views;
 QThread *worker=nullptr;std::atomic_bool cancelled{false};QVector<archive::Clip> clips,result;QVector<int> selectedChannels;QUrl source;QDate day;QString error;
 bool playing=false,dragging=false;int cursor=0,focused=0;QElapsedTimer gapClock;int gapStart=0;QTimer *seekTimer;
 void searchDay();void seek(int second);void refresh();void arrange();void stop();void showTime(int second);void openView(int index,int second);void step(int seconds);
public:
 ArchiveDialog(QUrl source,QWidget *parent=nullptr);~ArchiveDialog()override;void reject()override;
 bool eventFilter(QObject*,QEvent*)override;
};
