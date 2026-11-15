"""Render the actual production bitmap Draw function without a GPU/runtime."""
from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[2]
s=(r/'src/Framegen/StatsOverlay.cpp').read_text()
body=s[s.index('namespace cvr::framegen {'):s.index('const XrCompositionLayerBaseHeader*')]
prefix='''#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <cassert>
#include "Framegen/Framegen.hpp"
struct SYSTEMTIME {unsigned wHour=0,wMinute=0;}; void GetLocalTime(SYSTEMTIME*){}
namespace cvr::framegen { const char* BackendName(Backend){return "stub";} Backend ActiveBackend(){return Backend::Nvidia;} }
'''
main='''}
int main(int argc,char** argv){
 using namespace cvr::framegen;
 Settings s;s.overlayLayout=3;Statistics stats;stats.outputFps=67.4;stats.recentRepeatPercent=25;stats.deliveryAgeMs=36;
 const int w=420,h=144;std::vector<uint32_t> p(w*h+2,0xDEADBEEF);Draw({p.data()+1,w,h},s,stats);
 assert(p.front()==0xDEADBEEF&&p.back()==0xDEADBEEF);
 int groups=0;bool previous=false;
 for(int y=0;y<h;y++){bool green=false;for(int x=0;x<w;x++){auto v=p[1+y*w+x];assert(v==0xA0000000||v==0xFF40FF40);if(v==0xFF40FF40){green=true;assert(x>=12&&x<w-10);}}if(green&&!previous)++groups;previous=green;}
 assert(groups==3);
 FILE* f=fopen(argv[1],"wb");assert(f);fprintf(f,"P6\\n%d %d\\n255\\n",w,h);
 for(int i=0;i<w*h;i++){auto v=p[i+1];unsigned char c[]={static_cast<unsigned char>(v),static_cast<unsigned char>(v>>8),static_cast<unsigned char>(v>>16)};fwrite(c,1,3,f);}fclose(f);
 // Unknown age must render the explicit -- placeholder rather than invented zero.
 stats.deliveryAgeMs=-1;Draw({p.data()+1,w,h},s,stats);assert(p.front()==0xDEADBEEF&&p.back()==0xDEADBEEF);
 puts("PASS: three green lines, bounded raster, unknown age supported");
}
'''
(r/'tools/runtime_recorder/bin').mkdir(parents=True,exist_ok=True)
cpp=Path('/tmp/cpvr-overlay-test.cpp');cpp.write_text(prefix+body+main)
subprocess.run(['g++','-std=c++20','-O1','-fsanitize=address,undefined','-I'+str(r/'include'),str(cpp),'-o','/tmp/cpvr-overlay-test'],check=True)
subprocess.run(['/tmp/cpvr-overlay-test',str(r/'tools/runtime_recorder/bin/overlay-preview.ppm')],check=True)
