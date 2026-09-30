#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
// Offset backward source-clock jumps without changing PTS-DTS composition offsets.
class RecordingClock {
 int64_t offset=0,last=-1;
 static int64_t add(int64_t a,int64_t b){
  if((b>0&&a>INT64_MAX-b)||(b<0&&a<INT64_MIN-b))throw std::runtime_error("Recording timestamp overflow");
  return a+b;
 }
public:
 void reset(){offset=0;last=-1;}
 void apply(int64_t &dts,int64_t &pts,int64_t duration){
  auto next=add(dts,offset);
  if(next<=last){
   auto desired=add(last,std::max<int64_t>(1,duration));
   if(next<0&&desired>INT64_MAX+next)throw std::runtime_error("Recording timestamp overflow");
   offset=add(offset,desired-next);next=desired;
  }
  dts=next;if(pts!=INT64_MIN)pts=add(pts,offset);last=next;
 }
};
