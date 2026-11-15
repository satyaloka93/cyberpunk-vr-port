#pragma once
#include <array>
#include <cmath>
#include <cstddef>
namespace cvr::framegen {
// One common two-second observation window for repeats and age, refreshed independently of framegen.
// Capture timestamps are image-content timestamps (midpoint for synthesized frames), NOT headset scanout.
class DeliveryWindow {
    struct Sample {double time{},age=-1;bool repeat{};};
    std::array<Sample,512> samples{};
    size_t next{},count{};double lastCapture{},lastTime{};
public:
    struct Summary {double ageMs=-1,repeatPercent=-1;size_t count{};};
    void Add(double now,double capture,bool repeat) {
        if(!std::isfinite(now) || now<=0)return;
        if(now<lastTime)*this={};
        lastTime=now;
        if(std::isfinite(capture)&&capture>0&&capture<=now)lastCapture=capture;
        else if(!repeat)lastCapture=0;
        samples[next]={now,lastCapture>0 ? now-lastCapture : -1,repeat};
        next=(next+1)%samples.size();if(count<samples.size())++count;
    }
    Summary Read(double now) const {
        Summary out;size_t ages=0,repeats=0;double sum=0;
        for(size_t i=0;i<count;++i)if(now>=samples[i].time&&now-samples[i].time<=2000) {
            ++out.count;if(samples[i].repeat)++repeats;
            if(samples[i].age>=0){++ages;sum+=samples[i].age;}
        }
        if(out.count)out.repeatPercent=100.0*repeats/out.count;
        if(ages)out.ageMs=sum/ages;
        return out;
    }
};
}
