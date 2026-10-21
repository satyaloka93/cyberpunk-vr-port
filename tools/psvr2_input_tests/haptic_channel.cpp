#include "Runtimes/Psvr2HapticChannel.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
static int checks=0;
static void check(bool ok) { ++checks; if(!ok) std::exit(1); }
int main() {
    // Never reset an active game's shared channel from a test.
    HANDLE existing=OpenFileMappingA(FILE_MAP_READ,FALSE,cvr::psvr2::HapticMapping);
    if(existing) { CloseHandle(existing); std::cerr<<"Active mapping; close game before test\n";return 2; }
    auto& channel=cvr::psvr2::Haptics();
    HANDLE reader=OpenFileMappingA(FILE_MAP_ALL_ACCESS,FALSE,cvr::psvr2::HapticMapping);
    check(reader!=nullptr);
    auto* data=static_cast<float*>(MapViewOfFile(reader,FILE_MAP_ALL_ACCESS,0,0,1024));
    check(data!=nullptr);
    check(data[168]==18512 && data[169]==1);
    channel.Heartbeat();check(data[170]==1);
    channel.Pulse(1,.85f,45);
    check(data[157]==1 && data[158]==1 && data[159]==.85f && data[160]==45);
    channel.Pulse(0,1,90);
    check(data[157]==2 && data[158]==0 && data[159]==1 && data[160]==90);
    channel.Pulse(2,1,90);channel.Pulse(1,0,90);channel.Pulse(1,1.1f,90);
    channel.Pulse(1,std::numeric_limits<float>::quiet_NaN(),90);
    channel.Pulse(1,1,0);channel.Pulse(1,1,1001);
    check(data[157]==2);
    data[157]=1000000;data[170]=1000000;
    channel.Pulse(1,1,90);channel.Heartbeat();
    check(data[157]==1 && data[170]==1);
    UnmapViewOfFile(data);CloseHandle(reader);
    std::cout<<checks<<" haptic IPC checks passed\n";
}
