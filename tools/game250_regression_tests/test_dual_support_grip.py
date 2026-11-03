"""Compile the production early-exit guard; an occupied hand must not weld to the other gun."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/Anim/TwoHandGrip.cpp').read_text()
m=re.search(r'    if \(g_hasWeaponEquipped && CyberpunkVR_DualWieldRecords\[0\] != 0 &&.*?\n    }',s,re.S)
assert m
assert s.index(m[0]) < s.index('    SelectHold();',s.index(m[0]))
cpp='''#include <cstdint>
#include <cassert>
bool g_hasWeaponEquipped;
uint32_t CyberpunkVR_DualWieldRecords[2];
uint64_t CyberpunkVR_DualWieldUntil, now;
int CyberpunkVR_TwoHandActive;
float CyberpunkVR_TwoHandBlend;
bool reset,continued;
uint64_t GetTickCount64(){return now;}
void SteerReset(){reset=true;}
void run(){
'''+m[0]+'''
continued=true;
}
int main(){
for(int held=0;held<2;++held)for(int left=0;left<2;++left)for(int right=0;right<2;++right)for(int lease=0;lease<3;++lease){
 g_hasWeaponEquipped=held;CyberpunkVR_DualWieldRecords[0]=left;CyberpunkVR_DualWieldRecords[1]=right;
 now=100;CyberpunkVR_DualWieldUntil=99+lease;
 CyberpunkVR_TwoHandActive=1;CyberpunkVR_TwoHandBlend=.7f;reset=continued=false;
 run();bool blocked=held&&left&&right&&lease==2;
 assert(continued==!blocked);assert(reset==blocked);
 assert(CyberpunkVR_TwoHandActive==(blocked?0:1));
 assert(CyberpunkVR_TwoHandBlend==(blocked?0.f:.7f));
}}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(cpp)
 subprocess.run(['g++','-std=c++20',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: dual guns release support grip; single/empty/expired leases retain normal behavior (24 cases)')
