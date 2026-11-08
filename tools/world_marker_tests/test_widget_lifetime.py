"""Regression: a freed root address reused by a child must retain its parent's depth."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
s = (root/'src/Hooks/WorldMarkers.cpp').read_text()
a = s.index('        auto it=roots.find(widget);', s.index('void Paint('))
b = s.index('        // Diagnostic: a marker', a)
paint = s[a:b]
a = s.index('            for(auto it=roots.begin();it!=roots.end();)')
b = s.index('\n        }', a)
prune = s[a:b]
code = r'''
#include <cassert>
#include <cstdint>
#include <map>
struct Handle { void* p{}; Handle Lock(){return *this;} void* GetPtr(){return p;} };
struct SourceProjection{};struct MarkerAnchor{};
struct Entry{Handle root;bool hidden{},inheritDepth{};uint64_t stamp{};float inverseDepth{};SourceProjection source;MarkerAnchor anchor;};
std::map<void*,Entry> roots;
float paintDepth=.25f;uintptr_t paintOwner{};SourceProjection paintSource;MarkerAnchor paintAnchor;
uint64_t GetTickCount64(){return 5000;}
bool paint(void* widget){bool hidden=false;
''' + paint + r'''
 return hidden;
}
void prune(uint64_t now){
''' + prune + r'''
}
int main(){
 int address,other;
 // Old weak handle expired, but map key was reused for a child: it must not flatten that subtree.
 roots[&address]={{nullptr},true,false,1000,.1f};
 assert(!paint(&address));assert(paintDepth==.25f);
 // Also reject a handle which resolves to a different widget.
 roots[&address].root.p=&other;assert(!paint(&address));assert(paintDepth==.25f);
 prune(5000);assert(roots.empty());
 // A live hidden root keeps ownership, even when old; stale entries alone are removed.
 roots[&address]={{&address},true,false,1000,.1f};prune(5000);assert(roots.size()==1);
 roots[&address].stamp=4900;assert(paint(&address));assert(paintDepth==.1f);
 roots[&address].inheritDepth=true;paintDepth=.3f;paint(&address);assert(paintDepth==.3f);
}
'''
with tempfile.TemporaryDirectory() as td:
    p=Path(td);(p/'test.cpp').write_text(code)
    subprocess.run(['g++','-std=c++17',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: actual marker lookup/pruning rejects reused addresses and preserves live hidden/inherited depth')
