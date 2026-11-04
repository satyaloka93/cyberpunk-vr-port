"""Exercise production camera preparation across live asymmetric/symmetric switches."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2]
s=(r/'src/Stereo/AsymProjection.cpp').read_text()
def function(signature):
 a=s.index(signature);z=s.index('{',a)+1;depth=1
 while depth:
  depth+=(s[z]=='{')-(s[z]=='}');z+=1
 return s[a:z]
code=r"""
#include <cmath>
#include <cstdint>
#include <cassert>
namespace cvr::projection {
struct EyeWindow {float l,r,d,u;};
struct EngineProjection {float vfovDeg,aspect,shiftX,shiftY;};
EngineProjection ToEngine(EyeWindow w){return {90,1.05f,w.l,0.1f};}
}
namespace cvr::engine_stereo {
struct EyeProjection {float aspect,shiftX,shiftY;};
bool enabled=true,valid=false;EyeProjection eyes[2]{};
bool Enabled(){return enabled;}
void PublishEyes(const EyeProjection* p,bool v){valid=v;if(v){eyes[0]=p[0];eyes[1]=p[1];}}
}
struct AsymEyeWindows {cvr::projection::EyeWindow eye[2];};
struct OpenXRManager {
 static OpenXRManager& Get(){static OpenXRManager m;return m;}
 bool ok=true;
 bool GetAsymmetricEyeWindows(AsymEyeWindows* w,bool=true){w->eye[0]={-0.2f,1,-1,1};w->eye[1]={0.2f,1,-1,1};return ok;}
};
cvr::projection::EyeWindow EngineStereoCentre(const AsymEyeWindows& w){
 cvr::engine_stereo::EyeProjection e[2]={{1.05f,-.2f,.1f},{1.05f,.2f,.1f}};
 cvr::engine_stereo::PublishEyes(e,true);return w.eye[0];
}
void NoteAspect(const uint8_t*,cvr::projection::EyeWindow,cvr::projection::EyeWindow){}
struct Side {bool active=false;float wroteFov=0,wroteX=0,wroteY=0,wroteAspect=0,engineFov=0,engineAspect=0,halfX=0,halfY=0;} g_side[2];
struct {int side;bool want;float x,y,lastX,lastY;} t_preparing;
constexpr int kInputFov=0,kPlanes=0,kPlaneOverrides=256,kFovScale=128,kFovAdd=132;
int CyberpunkVR_AsymProjection=1,CyberpunkVR_MainIsRightEye=0,g_launcherWidth=3144,g_launcherHeight=3000;
float CyberpunkVR_DebugAsymView[16]{},CyberpunkVR_DebugAsymPlanes[64]{},CyberpunkVR_DebugAsymFovTerms[4]{},CyberpunkVR_DebugAsymWindow[8]{},CyberpunkVR_AsymTest[8]{};
float GetGameRenderVerticalFovDeg(){return 110.f;}
"""+function('void Restore(')+function('void Apply(')+r"""
int main(){
 alignas(16) uint8_t view[512]{};auto f=reinterpret_cast<float*>(view);
 f[0]=90;f[2]=2.096f;f[kFovScale/4]=1;
 Apply(view,0);assert(cvr::engine_stereo::valid);assert(cvr::engine_stereo::eyes[0].shiftX==-0.2f);
 for(int i=0;i<20;++i){
  CyberpunkVR_AsymProjection=0;Apply(view,0);
  assert(cvr::engine_stereo::valid);assert(f[0]==110);assert(f[4]==0&&f[5]==0);
  for(auto e:cvr::engine_stereo::eyes){assert(e.shiftX==0&&e.shiftY==0);assert(std::fabs(e.aspect-1.048f)<1e-5);}
  assert(std::fabs(f[2]-1.048f)<1e-5);
  CyberpunkVR_AsymProjection=1;Apply(view,0);assert(cvr::engine_stereo::eyes[1].shiftX==0.2f);
 }
 OpenXRManager::Get().ok=false;Apply(view,0);assert(!cvr::engine_stereo::valid);
 // VRCAM retains its original restore behavior, not the SBS per-eye aspect.
 cvr::engine_stereo::enabled=false;CyberpunkVR_AsymProjection=0;Apply(view,0);assert(!cvr::engine_stereo::valid);
}
"""
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: production Apply switches both eyes, preserves per-eye aspect, and invalidates failed queries')

# Compile the exact DLSS history-reset block with two independent eye histories.
s=(r/'src/Stereo/EngineStereoDlss.cpp').read_text()
a=s.index('        static EyeProjection previousProjection[2]');b=s.index('        values.mvecScale.x',a)
code=r"""
#include <cmath>
#include <cassert>
namespace sl {enum class Boolean {eFalse,eTrue};}
struct EyeProjection {float aspect=0,shiftX=0,shiftY=0;};
EyeProjection eyes[2]={{1.048f,-.2f,.1f},{1.048f,.2f,.1f}};bool valid=true;
bool EyeWindow(int eye,EyeProjection* p){*p=eyes[eye];return valid;}
bool reset(int eye,bool engineReset=false){
 struct {sl::Boolean reset;} values{engineReset?sl::Boolean::eTrue:sl::Boolean::eFalse};
"""+s[a:b]+r"""
 return values.reset==sl::Boolean::eTrue;
}
int main(){
 for(int e=0;e<2;e++){assert(reset(e));assert(!reset(e));assert(reset(e,true));}
 for(int e=0;e<2;e++)eyes[e]={1.048f,0,0};
 for(int e=0;e<2;e++){assert(reset(e));assert(!reset(e));}
 valid=false;for(int e=0;e<2;e++){assert(reset(e));assert(!reset(e));}
 valid=true;for(int e=0;e<2;e++){assert(reset(e));assert(!reset(e));}
}
"""
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: each DLSS eye resets once per geometry transition; engine reset preserved')

# Compile actual submission + panel-coverage math against asymmetric runtime lenses.
s=(r/'include/Runtimes/OpenXRInternal.hpp').read_text()
submit=function('inline XrFovf ApplyForcedProjectionFov(')
correction=(r/'include/Runtimes/RuntimeFovCorrection.hpp').read_text().replace('#pragma once','').replace('#include <openxr/openxr.h>','')
code=r"""
#include <cmath>
#include <cassert>
namespace std {using ::tanf;}
struct XrFovf{float angleLeft,angleRight,angleUp,angleDown;};
float forced=0,horizontal=0,vertical=0;int g_verboseLog=0;
float GetForcedFov(){return forced;}
float GetGameRenderFovDeg(){return horizontal;}
float GetGameRenderVerticalFovDeg(){return vertical;}
#define LOG_THROTTLED(...) ((void)0)
"""+correction+submit+r"""
int main(){
 constexpr float rad=3.1415926535f/180.f;
 XrFovf lenses[2]={{-54*rad,40*rad,44*rad,-55*rad},{-40*rad,54*rad,44*rad,-55*rad}};
 for(float aspect:{1.f,1.048f,0.9f}){
  horizontal=GetPanelCoveringHorizontalFovDeg(lenses[0],lenses[1],aspect);
  vertical=2*atanf(tanf(horizontal*rad/2)/aspect)/rad;
  for(int e=0;e<2;e++){
   auto f=ApplyForcedProjectionFov(lenses[e],lenses,e,3000*aspect,3000);
   assert(f.angleLeft==-f.angleRight);assert(f.angleDown==-f.angleUp);
   assert(f.angleLeft<=lenses[e].angleLeft+1e-6);assert(f.angleRight>=lenses[e].angleRight-1e-6);
   assert(f.angleUp>=lenses[e].angleUp-1e-6);assert(f.angleDown<=lenses[e].angleDown+1e-6);
   assert(std::fabs(tanf(f.angleRight)/tanf(f.angleUp)-aspect)<1e-5);
  }
 }
 // Small offsets below correction threshold must not survive into a centered submit.
 lenses[0]={-.91f,.90f,.89f,-.90f};lenses[1]=lenses[0];
 auto f=ApplyForcedProjectionFov(lenses[0],lenses,0,3144,3000);
 assert(f.angleLeft==-f.angleRight&&f.angleDown==-f.angleUp);
 forced=104.5f;f=ApplyForcedProjectionFov(lenses[0],lenses,0,3144,3000);
 assert(std::fabs(f.angleRight-forced*rad/2)<1e-6);
}
"""
code='#include <cstdint>\n#include <initializer_list>\n'+code
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: symmetric submission covers both runtime lenses and matches per-eye render aspect')

# Exercise the actual Present caller dimensions, not just the helper in isolation.
s=(r/'src/Runtimes/OpenXRPresent.cpp').read_text()
a=s.index('            float fovWidth =');b=s.index('            bool hasRenderHeadPose',a)
dimensions=s[a:b]
code=code[:code.index('int main(){')]+r'''
namespace cvr::engine_stereo {bool enabled=true;bool Enabled(){return enabled;}}
int main(){
 XrFovf lenses[2]={{-.91f,.90f,.89f,-.90f},{-.90f,.91f,.89f,-.90f}};
 forced=112.f;
 for(bool fallback:{false,true})for(bool stereo:{false,true}){
  cvr::engine_stereo::enabled=stereo;
  struct {struct {unsigned Width,Height;} BufferDesc;} desc{{fallback?0u:6288u,fallback?0u:3000u}};
  struct {unsigned Width,Height;} resourceDesc{6288,3000};
'''+dimensions+r'''
  auto f=ApplyForcedProjectionFov(lenses[0],lenses,0,fovWidth,fovHeight);
  const float expectedAspect=stereo?1.048f:2.096f;
  assert(std::fabs(tanf(f.angleRight)/tanf(f.angleUp)-expectedAspect)<1e-5);
  if(stereo)assert(std::fabs(2*f.angleUp*180.f/3.1415926535f-109.488f)<.002f);
 }
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: actual Present caller submits one-eye aspect for SBS, including resource fallback and 112-degree override')
