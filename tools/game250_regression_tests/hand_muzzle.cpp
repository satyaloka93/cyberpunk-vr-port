#include "Anim/HandMuzzle.hpp"
#include <cassert>
#include <limits>
int main(){
 using namespace cvr::aim;
 HandMuzzle left{},right{},out{};
 left.hand=0;left.weapon=10;left.stamp=1000;left.q[0]=.70710678f;left.q[3]=.70710678f;
 right.hand=1;right.weapon=20;right.stamp=1000;right.q[3]=1;
 PublishHand(left);PublishHand(right);
 assert(ForWeapon(10,1001,out)&&out.hand==0&&out.f[2]>.99f);
 assert(ForWeapon(20,1001,out)&&out.hand==1&&out.f[1]>.99f&&out.f[2]==0);
 assert(DualMuzzles(1001));assert(!ForWeapon(30,1001,out));
 assert(!ForWeapon(10,1151,out));assert(!ForWeapon(10,999,out));
 left.q[0]=std::numeric_limits<float>::quiet_NaN();left.weapon=30;PublishHand(left);
 assert(!ForWeapon(30,1001,out));
 right.weapon=10;PublishHand(right);assert(!DualMuzzles(1001));
}
