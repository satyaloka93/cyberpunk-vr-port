"""Compile the actual wheel-latch decision blocks."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/Anim/WheelGrab.cpp').read_text()
a=s.index('        if (!enabled || !driving || handBlocked)');b=s.index('        w.gripPrev = grip;',a)+len('        w.gripPrev = grip;');update=s[a:b]
a=s.index('        if(!allowed || !tracked');b=s.index('        w.gripPrev=grip;',a)+len('        w.gripPrev=grip;');maintain=s[a:b]
code='''#include <cassert>
#include "Camera/AnchorTranslation.hpp"

struct Hand {bool engaged=false,gripPrev=false,atWheel=false;};
void update(Hand& w,bool grip,bool enabled=true,bool driving=true,bool handBlocked=false){
'''+update+'''}
void maintain(Hand& w,bool grip,bool allowed=true,bool tracked=true,int h=0,bool g_hasWeaponEquipped=false,int gunHand=0){
'''+maintain+'''}
int main(){
 using namespace cvr::camera;
 AnchorVector car{.1f,.2f,.3f},bike{-.2f,.4f,-.1f};
 auto c=SelectVehicleCameraOffset(car,bike,false),b=SelectVehicleCameraOffset(car,bike,true);
 assert(c.x==car.x && c.z==car.z && b.y==bike.y && b.z==bike.z);
 auto p=ActiveVehicleCameraOffset(b,true);assert(p.x==0 && p.y==0 && p.z==0);
 Hand w;update(w,true);assert(!w.engaged);update(w,false);
 w.atWheel=true;update(w,true);assert(w.engaged);update(w,false);assert(w.engaged);
 w.atWheel=false;update(w,true);assert(!w.engaged);update(w,false);
 w.atWheel=true;update(w,true);update(w,false);maintain(w,false);assert(w.engaged);
 maintain(w,true);assert(!w.engaged);maintain(w,false);
 update(w,true);update(w,false);maintain(w,false,true,false);assert(!w.engaged);
 update(w,true,true,true,true);assert(!w.engaged);update(w,true);assert(!w.engaged);
 update(w,false);update(w,true);assert(w.engaged);update(w,false,true,false);assert(!w.engaged);
 // Physical left gun cannot remain latched; opposite hand remains usable.
 w.engaged=true;maintain(w,false,true,true,1,true,1);assert(!w.engaged);
 w.engaged=true;maintain(w,false,true,true,0,true,1);assert(w.engaged);
 Hand other;other.atWheel=true;update(other,true);update(other,false);
 update(w,true);assert(!w.engaged && other.engaged);
 maintain(other,false,false);assert(!other.engaged);

}
'''
with tempfile.TemporaryDirectory() as d:
 cpp=Path(d)/'test.cpp';exe=Path(d)/'test';cpp.write_text(code)
 subprocess.run(['g++','-std=c++17','-I'+str(root/'include'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: production driving latch/release, camera-miss release, tracking/weapon/exit reset.')
