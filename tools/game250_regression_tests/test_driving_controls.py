"""Compile the actual wheel-latch decision blocks and vehicle-offset selector."""
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
void maintain(Hand& w,bool grip,bool allowed=true,bool tracked=true,int h=0,bool g_hasWeaponEquipped=false){
'''+maintain+'''}
int main(){
 Hand w;update(w,true);assert(!w.engaged);update(w,false);
 w.atWheel=true;update(w,true);assert(w.engaged);update(w,false);assert(w.engaged);
 w.atWheel=false;update(w,true);assert(!w.engaged);update(w,false);
 w.atWheel=true;update(w,true);update(w,false);maintain(w,false);assert(w.engaged);
 maintain(w,true);assert(!w.engaged);maintain(w,false);
 update(w,true);update(w,false);maintain(w,false,true,false);assert(!w.engaged);
 update(w,true,true,true,true);assert(!w.engaged);update(w,true);assert(!w.engaged);
 update(w,false);update(w,true);assert(w.engaged);update(w,false,true,false);assert(!w.engaged);
 using namespace cvr::camera;
 const AnchorVector car{.1f,.2f,.3f},bike{.4f,.5f,.6f};
 auto c=SelectVehicleCameraOffset(car,bike,false),b=SelectVehicleCameraOffset(car,bike,true);
 assert(c.x==.1f&&c.y==.2f&&c.z==.3f&&b.x==.4f&&b.y==.5f&&b.z==.6f);
 auto suspended=ActiveVehicleCameraOffset(b,true);assert(suspended.x==0&&suspended.y==0&&suspended.z==0);
}
'''
with tempfile.TemporaryDirectory() as d:
 cpp=Path(d)/'test.cpp';exe=Path(d)/'test';cpp.write_text(code)
 subprocess.run(['g++','-std=c++17','-I'+str(root/'include'),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: production driving latch/release, camera-miss release, tracking/weapon/exit reset, separate bike/car offsets.')
