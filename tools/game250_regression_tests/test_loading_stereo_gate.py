"""Compile the actual SDK gate and exercise loading/front-end/gameplay transitions."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[2]
s=(r/'src/Stereo/EngineStereoSdk.cpp').read_text()
a=s.index('bool Available(Object*)');z=s.index('{',a)+1;depth=1
while depth:
 depth+=(s[z]=='{')-(s[z]=='}');z+=1
code='''#include <atomic>
#include <cassert>
struct Object{};std::atomic<bool> g_armed{false};bool loading=false;
int count=0;void Count(int,int){++count;}bool LoadingShown(){return loading;}
void Log(const char*,...){}
'''+s[a:z]+'''
int main(){
 assert(!Available(nullptr));loading=true;assert(!Available(nullptr));
 g_armed=true;for(int i=0;i<100;++i)assert(!Available(nullptr));
 assert(g_armed);loading=false;assert(Available(nullptr));
 // Reloading an existing save must also show a full-window panel.
 loading=true;assert(!Available(nullptr));loading=false;assert(Available(nullptr));
 g_armed=false;assert(!Available(nullptr));assert(count==106);
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: actual SDK gate keeps loading mono and resumes SBS, including repeated loads')
