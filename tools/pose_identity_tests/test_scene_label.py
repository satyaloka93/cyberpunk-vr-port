"""Execute the production scene-label selection/publication with stale-pose cases."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/Hooks/LocateCamera.cpp').read_text()
a = source.index('            if (bdNow && !takeoverNow) {')
b = source.index('\n        }\n    }\n', a)
select = source[a:b]
a = source.index('    if (sceneLabelKnown && scenePoseId) {')
b = source.index('\n\n    if (CyberpunkVR_BdPushTransform', a)
publish = source[a:b]
code = r'''
#include <cassert>
#include <cstdint>
struct OpenXRHeadPose { bool valid; uint64_t originSerial; int sample; };
uint64_t CyberpunkVR_DebugSceneLabelPublished{};
struct Result { uint64_t id{}; int sample{}; } result;
namespace cvr::camera {
uintptr_t CurrentSerializedCameraComponent(){return 91;}
void PublishCameraSetupPose(uintptr_t buffer,uintptr_t owner,int kind,uint64_t id,OpenXRHeadPose pose){
 assert(buffer==73 && owner==91 && kind==1);result={id,pose.sample};
}
}
#define CVR_DIAGNOSTIC(x) x
void run(bool bdNow,bool takeoverNow,bool bdUseWriteSite,bool one,
         uint64_t seq,OpenXRHeadPose xrPose,uint64_t writeId,OpenXRHeadPose writePose){
 const bool CyberpunkVR_OneSamplePerFrame=one;
 const uint64_t cameraPoseSequence=seq,takeoverPoseId=writeId;
 const OpenXRHeadPose takeoverPose=writePose;
 OpenXRHeadPose scenePose{};uint64_t scenePoseId{};bool sceneLabelKnown=false;
 void* rbxPtr=reinterpret_cast<void*>(73);
 result={};
''' + select + '\n' + publish + r'''
}
int main(){
 OpenXRHeadPose frame{true,4,101},write{true,4,102};
 run(true,false,false,true,5,frame,5,write);assert(result.id==5 && result.sample==101);
 run(true,false,true,true,5,frame,5,write);assert(result.id==5 && result.sample==102);
 run(true,false,true,true,5,frame,4,write);assert(!result.id); // previous frame
 write.originSerial=3;run(true,false,true,true,5,frame,5,write);assert(!result.id); // recentered
 write.originSerial=4;write.valid=false;run(true,false,true,true,5,frame,5,write);assert(!result.id);
 run(true,false,false,false,5,frame,5,write);assert(!result.id); // unlabelled legacy sampling
 run(true,false,false,true,0,frame,5,write);assert(!result.id);
 frame.valid=false;run(true,false,false,true,5,frame,5,write);assert(!result.id);
 frame.valid=true;run(false,false,false,true,5,frame,5,write);assert(!result.id);
 run(true,true,false,true,5,frame,5,write);assert(!result.id); // takeover retains its own publisher
 assert(CyberpunkVR_DebugSceneLabelPublished==2);
}
'''
with tempfile.TemporaryDirectory() as td:
    p = Path(td)
    (p/'test.cpp').write_text(code)
    subprocess.run(['g++','-std=c++17',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: actual scene publisher selects matching poses and rejects stale, recentered, invalid and takeover labels')
