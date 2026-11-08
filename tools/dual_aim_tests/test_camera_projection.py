"""Regression: game 2.5 camera layout, ADS recovery and off-centre dot projection."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2]
s=(r/'src/Stereo/Dlss.cpp').read_text()
a=s.index('                float* c = reinterpret_cast<float*>(ctx + cvr::view_layout::kCamera);')
b=s.index('                } else if (key == g_vrcam_ctx_key',a)
read=s[a:b]+'\n}'
p=(r/'src/Overlay/OverlayProjection.cpp').read_text();a=p.index('bool GetOverlayProjWindow(');b=p.index('\nbool GetOverlayProjTans(',a);project=p[a:b]
code=r'''
#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include "Render/ViewCameraLayout.hpp"
float g_main_cam_fov{},g_main_cam_zoom{},g_main_cam_near{},g_main_cam_far{},g_main_proj_yy{},g_ads_factor{};
float CyberpunkVR_MainAdsZoomFactor{},CyberpunkVR_DebugMainCamFov{},CyberpunkVR_DebugMainProjYY{};
bool is_main_view(void*){return true;}
void sample(uintptr_t ctx){
'''+read+r'''
}
struct ImVec2{float x,y;};
struct AsymEyeWindows{struct W{float l=-1,r=1,d=-1,u=1;}eye[2];};
namespace cvr::projection{bool Active(){return true;}}
struct OpenXRManager{static OpenXRManager& Get(){static OpenXRManager x;return x;}
 bool GetAsymmetricEyeWindows(AsymEyeWindows* w){*w={};return true;}
 float GetRuntimeHorizontalFovDeg(){return 90;}};
float GetGameRenderFovDeg(){return 90;}
int CyberpunkVR_MainIsRightEye=0;
'''+project+r'''
int main(){
 alignas(16) float view[1024]{};
 float* cam=view+cvr::view_layout::kCamera/4;
 cam[0x20/4]=90;cam[0x2C/4]=1;cam[0x40/4]=.1f;cam[0x44/4]=4000;
 // Poison the former 2.31 locations with plausible header/matrix values.
 // The old code accepted them, so merely checking that FOV is positive missed this bug.
 for(float pitch:{0.f,15.f,30.f,60.f})for(float zoom:{1.f,1.5f,4.25f}){
  view[0x90/4]=80;view[0x214/4]=std::cos(pitch*.01745329252f);
  cam[0x1A4/4]=zoom;
  sample(reinterpret_cast<uintptr_t>(view));
  assert(std::abs(CyberpunkVR_MainAdsZoomFactor-zoom)<.00001f);
  assert(g_main_cam_near==.1f && g_main_cam_far==4000 && g_main_cam_fov==90);
  float tx,ty,cx,cy;assert(GetOverlayProjWindow({1000,1000},&tx,&ty,&cx,&cy));
  // Compare an off-centre barrel ray to the real world projection coefficient.
  const float rise=std::tan(pitch*.01745329252f);
  assert(std::abs(rise/ty-rise*cam[0x1A4/4])<.0001f);
  assert(std::abs(tx-1/zoom)<.00001f && cy==0);
 }
 cam[0x20/4]=std::numeric_limits<float>::quiet_NaN();sample(reinterpret_cast<uintptr_t>(view));
 assert(CyberpunkVR_MainAdsZoomFactor==1);
}
'''
code=code.replace('#include <cassert>','#include <cassert>\n#include <initializer_list>')
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++17','-I'+str(r/'include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: actual 2.5 camera sampling and overlay projection across pitch/zoom, poisoned legacy offsets, invalid-state fallback')
