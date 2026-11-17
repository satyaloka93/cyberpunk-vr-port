#pragma once
#include <vector>
namespace cvr::psvr2 {
struct HapticChannel {
 struct Event { int hand; float amplitude; int duration; };
 std::vector<Event> events;
 bool bikeRoadMuted=false;
 void SetBikeRoadMute(bool value){bikeRoadMuted=value;}
 void Pulse(int h,float a,int d) { events.push_back({h,a,d}); }
};
inline HapticChannel& Haptics(){static HapticChannel channel;return channel;}
}
