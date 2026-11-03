"""Compile and exercise the actual hitscan hand-selection branch."""
from pathlib import Path
import subprocess, tempfile
r=Path(__file__).resolve().parents[2]
s=(r/'src/Hooks/WeaponAim.cpp').read_text()
a=s.index('    bool left = false;',s.index('static bool WaLeftGunFired'))
b=s.index('    if (cvr::RuntimeDiagnosticsEnabled())',a)
code='''#include <atomic>
#include <cassert>
#include <cstdint>
void Log(const char*,...){}
std::atomic<uint32_t> s_waDualAlternate{0};
int choose(uint32_t record,uint32_t leftRecord,uint32_t rightRecord,bool leftTrigger,bool rightTrigger) {
'''+s[a:b]+'''return (left ? 1 : 0) | (path<<1);}
int main(){
 assert(choose(11,11,22,true,false)==5);
 assert(choose(22,11,22,true,false)==5); // shared primary record must not steal left trigger
 assert(choose(11,11,22,false,true)==4);
 assert(choose(11,11,22,true,true)==3);
 assert(choose(22,11,22,true,true)==2);
 assert(choose(22,11,22,false,true)==4);
 // A valid-looking but unmatched record must not masquerade as a right shot.
 assert(choose(99,11,22,true,false)==5);
 assert(choose(99,11,22,false,true)==4);
 assert(choose(0,11,22,true,false)==5);
 assert(choose(11,11,11,true,false)==5);
 assert(choose(11,11,11,false,true)==4);
 assert(choose(11,11,11,true,true)==6);
 assert(choose(11,11,11,true,true)==7);
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: actual shot routing, known/missing/unmatched records, identical weapons, single/both triggers')
