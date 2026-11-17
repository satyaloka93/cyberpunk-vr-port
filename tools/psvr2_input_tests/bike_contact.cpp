#include "Runtimes/BikeContactHaptics.hpp"
#include <cstdlib>
#include <iostream>
using namespace cvr::haptics;
int checks=0;
void check(bool b,const char* why){++checks;if(!b){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
    BikeContactHaptics p;cvr::anim::BikeAttitude a{};
    a.valid=a.wheelsValid=true;a.vehicle=42;a.frontContact=a.rearContact=true;a.stampUs=1000000;
    auto step=[&](uint64_t delta,bool front,bool rear,bool enabled=true){a.stampUs+=delta;a.frontContact=front;a.rearContact=rear;return p.Update(a,a.stampUs,42,enabled);};
    check(!step(0,true,true).event,"initial grounded state fabricated impact");
    auto out=step(10000,false,true);check(out.muteRoad&&!out.event,"front loss must mute immediately without premature pulse");
    out=p.Update(a,a.stampUs+40000,42,true);check(!out.event,"render polls confirmed an unchanged physics sample");
    out=step(10000,true,true);check(!out.event,"brief bump emitted event");
    step(10000,false,true);out=step(30000,false,true);
    check(out.event==1&&out.muteRoad&&out.durationMs==75,"confirmed liftoff missing");
    const float lift=out.amplitude;
    check(!step(10000,false,true).event,"sustained wheelie repeated pulse");
    out=step(10000,true,true);check(out.muteRoad&&!out.event,"front-contact flicker resumed road");
    out=step(30000,true,true);check(out.event==2&&!out.muteRoad&&out.amplitude>lift,"short wheelie lost distinct stronger landing");
    step(10000,false,true);out=step(30000,false,true);check(!out.event,"contact bounce defeated lift cooldown");
    step(10000,true,true);out=step(30000,true,true);check(!out.event&&!out.muteRoad,"landing cooldown blocks road restoration");
    out=step(200000,true,false);check(!out.muteRoad,"stoppie muted road with front grounded");
    out=step(30000,true,false);check(out.event==1&&!out.muteRoad,"rear liftoff not distinguished");
    step(10000,true,true);out=step(30000,true,true);check(out.event==2,"rear landing missed");
    out=p.Update(a,a.stampUs+100001,42,true);check(!out.event,"stale data fabricated pulse");
    out=step(200000,false,false);check(out.muteRoad&&!out.event,"resume airborne fabricated liftoff");
    step(200000,true,true);out=step(30000,true,true);check(out.event==2&&out.amplitude==.9f,"two-wheel landing not coalesced");
    check(!p.Update(a,a.stampUs,43,true).event,"foreign vehicle contact used");
    check(!step(10000,true,true).event,"identity recovery fabricated event");
    step(10000,false,true);check(!step(30000,false,true,false).event,"disabled emitted pulse");
    check(!step(10000,false,true).event,"reenable mid-wheelie fabricated pulse");
    a.valid=false;out=step(40000,false,true);
    check(out.muteRoad,"invalid pitch restored road despite valid front contact loss");
    check(!cvr::anim::BikeMotionFresh(a,a.stampUs,42),"invalid pose accepted by angle limiter");
    a.wheelsValid=false;check(!step(10000,true,true).event,"unknown layout fabricated landing");
    std::cout<<checks<<" bike-contact policy checks passed\n";
}
