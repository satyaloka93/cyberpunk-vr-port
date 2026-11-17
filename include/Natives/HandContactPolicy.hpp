#pragma once
#include <algorithm>
#include <cmath>

namespace cvr::contact {
struct Vec { float x{}, y{}, z{}; };
inline Vec operator+(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec operator-(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec operator*(Vec a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float Dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec Cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool Finite(Vec a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
inline float Length(Vec a){return std::sqrt(Dot(a,a));}
struct Contact { Vec normal; float depth{}; };
// Resolve the actual hand volume, not its wrist point. Never publish a partial
// correction when the geometry is invalid or a bounded solve cannot get clear.
template<class Probe> bool SurfaceSeat(Vec& position, Probe probe, float maxMove=.08f) {
    const Vec original=position;
    Vec p=position;
    if(!Finite(p)||!std::isfinite(maxMove)||maxMove<=0)return false;
    for(int pass=0;pass<17;++pass) {
        const Contact c=probe(p);
        if(!std::isfinite(c.depth))return false;
        if(c.depth<=.001f){position=p;return true;}
        if(pass==16 || !Finite(c.normal))return false;
        const float length=Length(c.normal);
        if(length<.5f || length>1.5f)return false;
        p=p+c.normal*((c.depth+.0005f)/length);
        if(!Finite(p)||Length(p-original)>maxMove)return false;
    }
    return false;
}
// Closest separation of finite line segments, including parallel/zero-length
// capsules. Used against cached NPC/torso capsules; no scene queries per elbow.
inline float SegmentDistance(Vec a,Vec b,Vec c,Vec d) {
    const Vec u=b-a,v=d-c,w=a-c;
    const float uu=Dot(u,u),vv=Dot(v,v),uv=Dot(u,v),uw=Dot(u,w),vw=Dot(v,w);
    float s=0,t=0;
    if(uu<=1e-12f && vv<=1e-12f)return Length(w);
    if(uu<=1e-12f)t=std::clamp(vw/vv,0.f,1.f);
    else {
        if(vv<=1e-12f)s=std::clamp(-uw/uu,0.f,1.f);
        else {
            const float den=uu*vv-uv*uv;
            if(den>1e-12f)s=std::clamp((uv*vw-uw*vv)/den,0.f,1.f);
            t=(uv*s+vw)/vv;
            if(t<0){t=0;s=std::clamp(-uw/uu,0.f,1.f);}
            else if(t>1){t=1;s=std::clamp((uv-uw)/uu,0.f,1.f);}
        }
    }
    return Length((a+u*s)-(c+v*t));
}
// Move only the elbow around the shoulder-to-wrist line: both segment lengths
// and tracked wrist position stay unchanged. Prefer the smallest clearing turn.
template<class Cost> bool ClearForearm(Vec shoulder,Vec wrist,Vec& elbow,Cost cost) {
    if(!Finite(shoulder)||!Finite(wrist)||!Finite(elbow))return false;
    Vec axis=wrist-shoulder;
    const float len=Length(axis);
    if(len<.02f)return false;
    axis=axis*(1.f/len);
    const Vec center=shoulder+axis*Dot(elbow-shoulder,axis),radial=elbow-center;
    if(Length(radial)<.005f)return false;
    const float initial=cost(elbow);
    if(!std::isfinite(initial)||initial<=1e-6f)return false;
    float best=initial;
    Vec selected=elbow;
    const Vec tangent=Cross(axis,radial);
    for(int step=1;step<=6;++step) {
        const float angle=float(step)*.261799388f; // at most 90 degrees, 12 trials
        for(float sign:{-1.f,1.f}) {
            const Vec candidate=center+radial*std::cos(angle)+tangent*(sign*std::sin(angle));
            const float score=cost(candidate);
            if(std::isfinite(score)&&score<best) {best=score;selected=candidate;}
        }
        if(best<=1e-6f)break;
    }
    if(best>=initial-.000001f)return false;
    elbow=selected;
    return true;
}
}
