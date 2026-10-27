#include "Stereo/AutoGrassReadiness.hpp"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
using namespace cvr::detail;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main()try {
    // Recorded 2.31 failure from PID20652, transcribed to the verified 2.50
    // layout: handle+830 is zero while count+81C is16 and adjacent buffers
    // are live. The pointer hierarchy itself is unchanged.
    constexpr uintptr_t wc=0x49e9cff220,owner=0x223188e1550,state=0x22713f49b90;
    std::map<uintptr_t,std::vector<unsigned char>> memory;
    auto write=[&](uintptr_t address,auto value){auto& bytes=memory[address];bytes.resize(sizeof(value));std::memcpy(bytes.data(),&value,sizeof(value));};
    auto read=[&](uintptr_t address,auto* value){auto i=memory.find(address);if(i==memory.end()||i->second.size()!=sizeof(*value))return false;std::memcpy(value,i->second.data(),sizeof(*value));return true;};
    auto inspect=[&]{return InspectAutoGrassInputs(wc,read,read);};
    write(wc+0x20,owner);write(owner+0x98,state);write(state+0x81C,uint32_t{16});
    write(state+0x824,uint32_t{32670});write(state+0x828,uint32_t{32669});write(state+0x82C,uint32_t{32668});
    write(state+0x830,uint32_t{0});write(state+0x834,uint32_t{32667});
    Check(inspect().status==AutoGrassStatus::MissingBuffer,"crash-state zero buffer reached the resource table");
    Check(uint64_t(uint32_t(0)-1)*176==0xafffffff50ULL,"recorded resource-table underflow no longer reproduced");
    for(uint32_t handle:{32671u,0u,32672u,0u,32673u}) {
        write(state+0x830,handle);const auto current=inspect();
        Check(bool(current)==(handle!=0),"ready/teardown/recreate transition retained stale eligibility");
        if(handle)Check(current.state==state && current.buffer==handle,"current world's resource identity was lost");
    }
    constexpr uintptr_t replacement=0x252afff4060;
    write(owner+0x98,replacement);write(replacement+0x81C,uint32_t{68});write(replacement+0x830,uint32_t{0});
    Check(!inspect(),"replacement world reused the previous world's ready buffer");
    write(replacement+0x830,uint32_t{40000});Check(bool(inspect()),"grass did not resume after resource creation");
    write(replacement+0x830,uint32_t{0xffffffff});Check(!inspect(),"invalid handle sentinel accepted");
    memory.erase(replacement+0x830);Check(!inspect(),"unreadable handle accepted");
    write(owner+0x98,uintptr_t{0});Check(inspect().status==AutoGrassStatus::MissingState,"missing terrain state accepted");
    memory.erase(wc+0x20);Check(inspect().status==AutoGrassStatus::MissingContext,"missing work context accepted");
    Check(!InspectAutoGrassInputs(0,read,read),"null work context accepted");
    std::cout<<"PASS recorded zero-handle crash, readiness recovery, teardown, world replacement and invalid reads\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
