"""Execute the production shared dot renderer against known muzzle rays and eye positions."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2]
s=(r/'src/Overlay/OverlayDebugDraw.cpp').read_text()
a=s.index('static void DrawBarrelCrosshairForHand');b=s.index('\n}  // namespace overlay',a)
body=s[a:b]
code=r'''
#include <cassert>
#include <cstdio>
#include <cmath>
#include <cstdint>
#include <vector>
#include "Utils/SharedSlots.hpp"
struct ImVec2{float x{},y{};ImVec2()=default;ImVec2(float a,float b):x(a),y(b){}};
using ImU32=uint32_t;
#define IM_COL32(r,g,b,a) (ImU32(r)|(ImU32(g)<<8)|(ImU32(b)<<16)|(ImU32(a)<<24))
struct Circle{ImVec2 p;ImU32 color;};std::vector<Circle> circles;
struct ImDrawList{
 void PushClipRect(ImVec2,ImVec2,bool){} void PopClipRect(){}
 void AddCircleFilled(ImVec2 p,float,ImU32 c){circles.push_back({p,c});}
};
namespace ImGui{struct IO{ImVec2 DisplaySize{1600,800};};IO& GetIO(){static IO i;return i;}
ImDrawList* GetBackgroundDrawList(){static ImDrawList l;return &l;}}
float slots[256]{};
struct AsymEyeWindows{struct Window{float l,r,d,u;}eye[2];};
class OpenXRManager{public:static OpenXRManager& Get(){static OpenXRManager x;return x;}
 float GetSharedSlot(int i){return slots[i];} float GetRuntimeIpd(){return .064f;}
 bool GetAsymmetricEyeWindows(AsymEyeWindows*){return false;}};
namespace cvr::projection{bool Active(){return false;}
float MapNdcX(float x,AsymEyeWindows::Window,AsymEyeWindows::Window){return x;}
float MapNdcY(float x,AsymEyeWindows::Window,AsymEyeWindows::Window){return x;}}
namespace cvr::engine_stereo{bool mono=false;bool Enabled(){return true;}bool OnePicture(){return mono;}}
namespace cvr::camera{struct BarrelFrame{float camQuat[4];float muzzleFwd[3];};
bool BarrelFrameRead(BarrelFrame* b){*b={{0,0,0,1},{0,1,0}};return true;}}
namespace cvr::cyberware{bool launcher=false;bool LauncherLine(float* p,float* f){p[0]=10;p[1]=21;p[2]=30;f[0]=.1f;f[1]=1;f[2]=.2f;return launcher;}}
namespace cvr::sight{void SightLineOffset(const float*,float*){}}
bool g_drawBarrelCross=true;int CyberpunkVR_SightDot=0,CyberpunkVR_WeaponClass=1;
uint64_t CyberpunkVR_BarrelDotTick=0;
float CyberpunkVR_BarrelDotNdcX=0,CyberpunkVR_BarrelDotNdcY=0,CyberpunkVR_BarrelDotNdcX2=0,CyberpunkVR_BarrelDotRadiusPx=0;
float CyberpunkVR_BarrelDotDistM=20,CyberpunkVR_SightZeroMeters=20,CyberpunkVR_BarrelDotOffX2=0;
int CyberpunkVR_BarrelDotWorld=1,CyberpunkVR_MainIsRightEye=0,CyberpunkVR_BarrelDotEyeSign=-1;
int32_t g_lastLocatePosFP[3]={10*131072,20*131072,30*131072};
float CyberpunkVRPort_HalfIpd(){return .032f;}uint64_t GetTickCount64(){return 42;}
void Log(const char*,...){}
void RotateVectorByQuaternion(float x,float y,float z,float,float,float,float,float* a,float* b,float* c){*a=x;*b=y;*c=z;}
bool GetOverlayProjTans(ImVec2,float* x,float* y){*x=*y=1;return true;}
bool ProjectHeadSpacePointToScreen(float x,float y,float z,ImVec2 d,ImVec2* p){
 if(z>=0)return false;*p={(x/-z*.5f+.5f)*d.x,(.5f-y/-z*.5f)*d.y};return true;}
'''+body+r'''
void near(float a,float b){if(std::abs(a-b)>=.003f)std::fprintf(stderr,"actual=%f expected=%f\n",a,b);assert(std::abs(a-b)<.003f);}
int main(){
 slots[144]=1;slots[200]=10.3f;slots[201]=21;slots[202]=29.75f;
 slots[vrshared::kDualWieldActive]=1;slots[vrshared::kLeftMuzzleValid]=1;
 slots[vrshared::kLeftMuzzlePos]=9.7f;slots[vrshared::kLeftMuzzlePos+1]=21;slots[vrshared::kLeftMuzzlePos+2]=29.75f;
 slots[vrshared::kLeftMuzzleFwd]=.2f;slots[vrshared::kLeftMuzzleFwd+1]=1;
 DrawBarrelCrosshair();assert(circles.size()==4);
 // Each hand's ray is seen from both eyes. Left muzzle deliberately aims well away from right.
 near(circles[0].p.x,400+400*(.3f+.032f)/21);
 near(circles[1].p.x,1200+400*(.3f-.032f)/21);
 near(circles[2].p.x,400+400*(-.3f+4+.032f)/21);
 near(circles[3].p.x,1200+400*(-.3f+4-.032f)/21);
 for(auto c:circles)near(c.p.y,400+400*.25f/21);
 near(CyberpunkVR_BarrelDotNdcX,(.3f+.032f)/21); // offhand never overwrites main publication
 assert(circles[0].color!=circles[2].color && CyberpunkVR_BarrelDotTick==42);
 circles.clear();slots[vrshared::kLeftMuzzleValid]=0;DrawBarrelCrosshair();assert(circles.size()==2);
 circles.clear();CyberpunkVR_WeaponClass=5;DrawBarrelCrosshair();assert(circles.empty() && CyberpunkVR_BarrelDotTick==0);
 cvr::cyberware::launcher=true;g_drawBarrelCross=false;DrawBarrelCrosshair();assert(circles.size()==2);
 near(circles[0].p.x,400+400*(2+.032f)/21);near(circles[0].p.y,400-400.0f*4/21);
 circles.clear();cvr::engine_stereo::mono=true;DrawBarrelCrosshair();assert(circles.empty());
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++17','-I'+str(r/'include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: production dual-dot renderer, independent muzzle rays, both eyes, primary publication, launcher and mono gates')
