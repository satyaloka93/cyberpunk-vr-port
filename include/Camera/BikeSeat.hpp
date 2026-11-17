#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
namespace cvr::camera {
// Remove only native lateral camera sway in the CURRENT render-owner frame.
// Called before physical head translation/IPD. Never imports a physics-tick pose.
class BikeSeat {
public:
    void Reset(){*this={};}
    float CorrectionMeters()const{return m_correction;}
    bool Apply(int32_t* camera,const int32_t* owner,const float* q,uintptr_t vehicle,
               uintptr_t root,uintptr_t origin,bool eligible,bool capture) {
        m_correction=0;
        if(!eligible || !vehicle || !root || !origin){Reset();return false;}
        if(vehicle!=m_vehicle||root!=m_root||origin!=m_origin){Reset();m_vehicle=vehicle;m_root=root;m_origin=origin;}
        const double n=double(q[0])*q[0]+double(q[1])*q[1]+double(q[2])*q[2]+double(q[3])*q[3];
        if(!std::isfinite(n)||n<.5||n>1.5)return false;
        // Game +X right, +Y forward, +Z up.
        const double right[3]={1-2*(double(q[1])*q[1]+double(q[2])*q[2])/n,
            2*(double(q[0])*q[1]+double(q[2])*q[3])/n,
            2*(double(q[0])*q[2]-double(q[1])*q[3])/n};
        double lateral=0,distance2=0;
        for(int i=0;i<3;++i){const double d=(int64_t(camera[i])-owner[i])/131072.;lateral+=d*right[i];distance2+=d*d;}
        if(!std::isfinite(distance2)||distance2>16)return false;
        if(!m_valid){if(!capture)return false;m_neutral=lateral;m_valid=true;return false;}
        const double correction=m_neutral-lateral;
        if(std::abs(correction)>.6)return false; // changed camera/mount, never teleport
        int32_t next[3]{};
        for(int i=0;i<3;++i){const double v=double(camera[i])+correction*right[i]*131072.;
            if(v<std::numeric_limits<int32_t>::min()||v>std::numeric_limits<int32_t>::max())return false;
            next[i]=int32_t(std::llround(v));}
        for(int i=0;i<3;++i)camera[i]=next[i];
        m_correction=float(correction);return true;
    }
private:
    uintptr_t m_vehicle{},m_root{},m_origin{};double m_neutral{};float m_correction{};bool m_valid{};
};
}
