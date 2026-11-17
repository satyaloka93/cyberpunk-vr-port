#include "Lobby/CompositionGrading.hpp"
#include "fixture.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
int checks=0;
void check(bool b,const char* why){++checks;if(!b){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
    std::array<float,108> recorded{};
    recorded[4]=1;recorded[8]=1;recorded[66]=6288;recorded[67]=3000;
    std::memcpy(recorded.data()+16,first,80);std::memcpy(recorded.data()+88,second,80);
    check(std::memcmp(recorded.data()+16,recorded.data()+88,72)!=0,"fixture no longer reproduces upstream rejection");
    check(cvr::lobby::CompositionBlock(recorded.data())==0,"recorded blank-lobby block rejected");
    auto b=recorded;std::memcpy(b.data()+88,b.data()+16,80);
    check(cvr::lobby::CompositionBlock(b.data())==0,"normal upstream full repeat rejected");
    b[106]=std::numeric_limits<float>::quiet_NaN();b[107]=-123;
    check(cvr::lobby::CompositionBlock(b.data())==0,"upstream last-eight-byte padding correction lost");
    b=recorded;const uint32_t stale[]={0x8B19DBE0,0x178,0x20D2,0,0xFCBE3500,0x034CFFCE,0xFE55A4C5,0xBC6A4B76,0x3D226FC6,0xBF32C3F6,0x3F36ED6A,0x7FF6};
    std::memcpy(b.data()+96,stale,48);check(cvr::lobby::CompositionBlock(b.data())==0,"stale repeated tail reintroduced flashing");
    const auto valid=b;
    for(int index:{4,5,6,7,8}) {
        for(float value:{-.1f,1.1f,std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::quiet_NaN()}) {
            b=valid;b[index]=value;check(cvr::lobby::CompositionBlock(b.data())==2,"invalid weight accepted");
        }
    }
    for(int index:{66,67})for(float value:{0.f,63.f,16385.f,3000.5f,std::numeric_limits<float>::infinity()}) {
        b=valid;b[index]=value;check(cvr::lobby::CompositionBlock(b.data())==3,"invalid target accepted");
    }
    b=valid;b[89]=.5f;check(cvr::lobby::CompositionBlock(b.data())==4,"mismatched stable grading accepted");
    for(float value:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::infinity()}) {
        b=valid;b[16]=b[88]=value;check(cvr::lobby::CompositionBlock(b.data())==4,"invalid stable grading accepted");
    }
    b=valid;for(int i=0;i<8;++i)b[16+i]=b[88+i]=0;
    check(cvr::lobby::CompositionBlock(b.data())==4,"all-zero fallback accepted");
    b=valid;b[26]=0;check(cvr::lobby::CompositionBlock(b.data())==5,"saturation guard removed");
    b=valid;b[27]=1;check(cvr::lobby::CompositionBlock(b.data())==5,"aberration guard removed");
    std::cout<<checks<<" lobby composition checks passed (upstream rejects recorded fixture; fallback accepts)\n";
}
