#include "Natives/HandContactPolicy.hpp"
#include "Natives/CollisionSnapshot.hpp"
#include <array>
#include <atomic>
#include <cassert>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>
using namespace cvr::contact;
int main(){
    int cases=0;
    auto close=[](float a,float b){return std::abs(a-b)<1e-5f;};
    // Realistic face approach: the palm volume is already two centimetres inside
    // a planar skin boundary. Resolve outward, preserving tangent coordinates.
    Vec hand{.01f,.2f,.3f};
    auto wall=[](Vec p){return Contact{{1,0,0},.03f-p.x};};
    assert(SurfaceSeat(hand,wall));assert(hand.x>=.029f&&close(hand.y,.2f)&&close(hand.z,.3f));++cases;
    // Two surfaces meeting at a corner; one correction must not ignore the other.
    hand={0,0,0};assert(SurfaceSeat(hand,[](Vec p){return p.x<p.y?Contact{{1,0,0},.025f-p.x}:Contact{{0,1,0},.025f-p.y};}));
    assert(hand.x>=.024f&&hand.y>=.024f);++cases;
    // Fully embedded / opposing constraints never save a partial invalid seat.
    hand={0,0,0};assert(!SurfaceSeat(hand,[](Vec){return Contact{{1,0,0},.2f};}));assert(close(hand.x,0));++cases;
    assert(!SurfaceSeat(hand,[](Vec){return Contact{{0,0,0},.02f};}));++cases;
    assert(!SurfaceSeat(hand,[](Vec){return Contact{{1,0,0},std::numeric_limits<float>::quiet_NaN()};}));++cases;
    assert(!SurfaceSeat(hand,[](Vec p){return Contact{{p.x>0?-1.f:1.f,0,0},.01f};}));assert(close(hand.x,0));++cases;
    // Segment distance: parallel, skew/intersecting, end-to-end, point capsules.
    assert(close(SegmentDistance({0,0,0},{1,0,0},{0,1,0},{1,1,0}),1));
    assert(close(SegmentDistance({-1,0,0},{1,0,0},{0,-1,0},{0,1,0}),0));
    assert(close(SegmentDistance({0,0,0},{1,0,0},{2,0,0},{3,0,0}),1));
    assert(close(SegmentDistance({0,0,0},{0,0,0},{0,1,0},{0,2,0}),1));
    assert(close(SegmentDistance({0,0,0},{0,0,0},{0,0,0},{0,0,0}),0));++cases;
    // Elbow initially intersects an NPC capsule; avoid it without changing either
    // limb length or the wrist. The obstacle is close to the elbow, not the hand.
    const Vec shoulder{0,0,0},wrist{0,0,.4f},obstacle{.19f,0,.22f};
    Vec elbow{.2f,0,.2f};
    auto cost=[&](Vec e){float d=std::max(0.f,.075f-SegmentDistance(e,wrist,obstacle,obstacle));return d*d;};
    float before=cost(elbow),upper=Length(elbow-shoulder),fore=Length(elbow-wrist);
    assert(ClearForearm(shoulder,wrist,elbow,cost));assert(cost(elbow)<before);
    assert(close(Length(elbow-shoulder),upper)&&close(Length(elbow-wrist),fore));++cases;
    Vec old=elbow;assert(!ClearForearm(shoulder,wrist,elbow,[](Vec){return 0.f;}));assert(close(Length(elbow-old),0));++cases;
    elbow={0,0,.2f};assert(!ClearForearm(shoulder,wrist,elbow,cost));++cases;
    // Concurrent payload copies must never tear, including concurrent writers.
    struct Payload {std::array<unsigned long long,128> words;};
    cvr::physhands::CollisionSnapshot<Payload> snapshot;
    Payload p{};assert(!snapshot.Read(p));
    std::atomic<bool> start{false},done{false};std::atomic<int> reads{0};
    std::vector<std::thread> readers;
    for(int i=0;i<3;++i)readers.emplace_back([&]{while(!start.load())std::this_thread::yield();while(!done.load()){Payload q{};if(snapshot.Read(q)){for(auto n:q.words)assert(n==q.words[0]);++reads;}}});
    std::thread second([&]{while(!start.load())std::this_thread::yield();for(int i=0;i<30000;++i){Payload q;q.words.fill(0xabcdef000000ull+i);snapshot.Write(q);}});
    start=true;
    for(int i=0;i<30000;++i){p.words.fill(0x123450000000ull+i);snapshot.Write(p);}
    second.join();done=true;for(auto& t:readers)t.join();assert(reads>0);++cases;
    std::cout<<cases<<" contact/snapshot cases passed; coherent concurrent reads="<<reads<<"\n";
}
