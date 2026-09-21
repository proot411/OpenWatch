#pragma once
#include "cell.h"
#include <cmath>
// Focus is a presentation change only: hidden cells retain their workers and recordings.
class Navigation {
 QGridLayout *grid;
 const QVector<Cell*> &cells;
 std::function<void(Cell*)> choose;
 int gridSize=4;
 Cell *focused=nullptr;
 void paintLayout(){
  for(auto *cell:cells){grid->removeWidget(cell);cell->hide();}
  if(focused){grid->addWidget(focused,0,0);focused->show();return;}
  int columns=int(std::sqrt(gridSize));
  for(int i=0;i<gridSize;++i){grid->addWidget(cells[i],i/columns,i%columns);cells[i]->show();}
 }
public:
 Navigation(QGridLayout *layout,const QVector<Cell*> &items,std::function<void(Cell*)> select):grid(layout),cells(items),choose(std::move(select)){
  for(auto *cell:cells)cell->activate=[this](Cell *target){toggle(target);};
 }
 void setGridSize(int count){gridSize=count;focused=nullptr;paintLayout();bool visibleSelection=false;for(int i=0;i<count;++i)visibleSelection|=cells[i]->selected;if(!visibleSelection)choose(cells[0]);}
 void toggle(Cell *cell){if(focused){restore();return;}choose(cell);focused=cell;paintLayout();}
 void restore(){if(!focused)return;focused=nullptr;paintLayout();}
 void step(int direction){
  QVector<Cell*> assigned;Cell *current=nullptr;
  for(int i=0;i<gridSize;++i){if(!cells[i]->name.isEmpty())assigned.append(cells[i]);if(cells[i]->selected)current=cells[i];}
  if(assigned.isEmpty())return;
  int index=assigned.indexOf(current);
  index=index<0?(direction>0?0:assigned.size()-1):(index+direction+assigned.size())%assigned.size();
  choose(assigned[index]);if(focused){focused=assigned[index];paintLayout();}
 }
 bool isFocused()const{return focused!=nullptr;}
};
