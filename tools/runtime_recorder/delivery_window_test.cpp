#include "Framegen/DeliveryWindow.hpp"
#include <cassert>
#include <limits>
int main(){
 using cvr::framegen::DeliveryWindow;
 DeliveryWindow w;assert(w.Read(1000).ageMs==-1);
 w.Add(1000,980,false);w.Add(1010,0,true);
 auto s=w.Read(1010);assert(s.count==2&&s.ageMs==25&&s.repeatPercent==50);
 assert(w.Read(4000).ageMs==-1&&w.Read(4000).repeatPercent==-1);
 w.Add(4010,5000,false);assert(w.Read(4010).ageMs==-1); // invalid future source must not reuse old image age
 w.Add(4020,0,true);assert(w.Read(4020).ageMs==-1);
 w.Add(100,80,false);assert(w.Read(100).count==1&&w.Read(100).ageMs==20); // clock reset
 w={};for(int i=0;i<1000;i++)w.Add(1000+i,980+i,i%2);
 s=w.Read(1999);assert(s.count==512&&s.ageMs==20&&s.repeatPercent==50);
 w.Add(std::numeric_limits<double>::quiet_NaN(),0,true);assert(w.Read(1999).count==512);
}
