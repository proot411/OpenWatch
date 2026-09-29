#pragma once
#include "cell.h"

// Pending clips survive cancelled save dialogs and application restarts.
class RecordingUi {
 struct Session {QString directory;bool stopping=false;};
 QMap<Cell*,Session> sessions;
 QWidget *parent;bool polling=false;
public:
 explicit RecordingUi(QWidget *window):parent(window){}
 static QString pendingRoot(){return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/pending-recordings";}
 bool pending(Cell *cell)const{return sessions.contains(cell);}
 bool stopping(Cell *cell)const{return sessions.contains(cell)&&sessions.value(cell).stopping;}
 void toggle(Cell *cell){
  if(sessions.contains(cell)){sessions[cell].stopping=true;cell->video.record({});return;}
  if(cell->video.frame().isNull())return;
  QString directory=pendingRoot()+"/"+QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")+"-"+QUuid::createUuid().toString(QUuid::Id128).left(8);
  if(!QDir().mkpath(directory)){QMessageBox::warning(parent,"Record","Cannot create the pending recording folder.");return;}
  sessions.insert(cell,{directory,false});cell->video.record(directory+"/clip.mkv");
 }
 static bool copyClip(const QString &source,const QString &target){
  QFile in(source);QSaveFile out(target);if(!in.open(QIODevice::ReadOnly)||!out.open(QIODevice::WriteOnly))return false;
  while(!in.atEnd()){auto bytes=in.read(1024*1024);if(bytes.isEmpty()&&in.error()!=QFile::NoError)return false;if(out.write(bytes)!=bytes.size())return false;}
  return out.commit();
 }
 void save(const Session &session){
  auto files=QDir(session.directory).entryInfoList({"*.mkv"},QDir::Files,QDir::Name);
  if(files.isEmpty()){QMessageBox::information(parent,"No clip captured","No video was recorded before stopping. Wait for the red border before stopping a recording.");return;}
  QString destination;
  if(files.size()==1){destination=QFileDialog::getSaveFileName(parent,"Save recording",QDir::homePath()+"/OpenWatch-"+QFileInfo(session.directory).fileName()+".mkv","Matroska video (*.mkv)");if(!destination.isEmpty()&&!destination.endsWith(".mkv",Qt::CaseInsensitive)){destination+=".mkv";if(QFileInfo::exists(destination)&&QMessageBox::question(parent,"Replace recording?","Replace the existing file?")!=QMessageBox::Yes)destination.clear();}}
  else destination=QFileDialog::getExistingDirectory(parent,"Save recording segments (connection was interrupted)");
  if(destination.isEmpty()){QMessageBox::information(parent,"Recording kept","Your unsaved footage is kept in:\n"+session.directory+"\n\nOpen Others → Unsaved recordings to retrieve it later.");return;}
  for(const auto &file:files){QString target=files.size()==1?destination:QDir(destination).filePath(QFileInfo(session.directory).fileName()+"-"+file.fileName());
   if(QFileInfo(target).absoluteFilePath()==file.absoluteFilePath())continue;
   if(files.size()>1&&QFileInfo::exists(target)){QMessageBox::warning(parent,"Recording kept","A destination file already exists. Original footage remains in:\n"+session.directory);return;}
   if(!copyClip(file.absoluteFilePath(),target)){QMessageBox::warning(parent,"Recording kept","Could not save the clip. Original footage remains in:\n"+session.directory);return;}
  }
  // Remove only the private staging files that were successfully copied.
  for(const auto &file:files){QString target=files.size()==1?destination:QDir(destination).filePath(QFileInfo(session.directory).fileName()+"-"+file.fileName());if(QFileInfo(target).absoluteFilePath()!=file.absoluteFilePath())QFile::remove(file.absoluteFilePath());}
  QDir().rmdir(session.directory);
 }
 void poll(){
  if(polling)return;QScopedValueRollback<bool> guard(polling,true);
  for(auto *cell:sessions.keys()){
   auto &session=sessions[cell];auto state=cell->video.status();
   if(!cell->video.recordingRequested()||state=="Stopped"||state=="Ready"||state=="Playback complete"){session.stopping=true;cell->video.record({});}
   if(session.stopping&&cell->video.recordingFinished()){auto finished=session;sessions.remove(cell);save(finished);}
  }
 }
 void closeAll(){QScopedValueRollback<bool> guard(polling,true);for(auto *cell:sessions.keys()){cell->video.record({});cell->video.stop();auto finished=sessions.take(cell);save(finished);}}
};
