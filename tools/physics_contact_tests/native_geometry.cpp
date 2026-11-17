#include "Natives/HandContactPolicy.hpp"
#include <windows.h>
#include <cassert>
#include <iostream>
#include <cstdlib>
#undef assert
#define assert(test) do { if (!(test)) { std::cerr << "FAIL line " << __LINE__ << ": " << #test << "\n"; std::exit(1); } } while (false)
struct Pose {float q[4]{0,0,0,1};float p[3]{};};
struct Box {int type=3;float half[3];};
struct Sphere {int type=0;float radius;};
struct Capsule {int type=2;float radius,half;};
using Penetration=bool(*)(float*,float*,const void*,const Pose*,const void*,const Pose*);
using Distance=float(*)(const float*,const void*,const Pose*,float*);
int wmain(int argc,wchar_t** argv){
    if(argc!=2){std::cerr<<"Supply absolute PhysX3Common_x64.dll path\n";return 2;}
    HMODULE dll=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!dll){std::cerr<<"LoadLibrary failed "<<GetLastError()<<"\n";return 3;}
    auto penetration=reinterpret_cast<Penetration>(GetProcAddress(dll,"?computePenetration@PxGeometryQuery@physx@@SA_NAEAVPxVec3@2@AEAMAEBVPxGeometry@2@AEBVPxTransform@2@23@Z"));
    auto distance=reinterpret_cast<Distance>(GetProcAddress(dll,"?pointDistance@PxGeometryQuery@physx@@SAMAEBVPxVec3@2@AEBVPxGeometry@2@AEBVPxTransform@2@PEAV32@@Z"));
    assert(penetration&&distance);
    // Exterior surface from an interior point: box, sphere, head capsule.
    Box box{3,{.08f,.10f,.12f}};Sphere sphere{0,.10f};Capsule head{2,.078f,.025f};Pose body;
    int cases=0;
    for(const void* shape:{static_cast<const void*>(&box),static_cast<const void*>(&sphere),static_cast<const void*>(&head)}) {
        const float pt[3]={.01f,.015f,.02f};float closest[3]={pt[0],pt[1],pt[2]};
        assert(distance(pt,shape,&body,closest)==0.f);
        Sphere probe{0,.0001f};Pose at;for(int k=0;k<3;++k)at.p[k]=pt[k];
        float normal[3]{},depth=0;assert(penetration(normal,&depth,&probe,&at,shape,&body));
        float surface[3];for(int k=0;k<3;++k)surface[k]=pt[k]+normal[k]*(depth-probe.radius);
        // A small outward margin must be outside; just inward must be inside.
        float out[3],in[3];for(int k=0;k<3;++k){out[k]=surface[k]+normal[k]*.001f;in[k]=surface[k]-normal[k]*.001f;}
        assert(distance(out,shape,&body,closest)>0.f);assert(distance(in,shape,&body,closest)==0.f);++cases;
    }
    // Bounded surface seating with the actual DLL's box-vs-capsule query.
    Box palm{3,{.04f,.03f,.02f}};
    cvr::contact::Vec p{0,.09f,0};
    assert(cvr::contact::SurfaceSeat(p,[&](cvr::contact::Vec pos){Pose at;at.p[0]=pos.x;at.p[1]=pos.y;at.p[2]=pos.z;float n[3]{},d=0;if(!penetration(n,&d,&palm,&at,&head,&body))return cvr::contact::Contact{};return cvr::contact::Contact{{n[0],n[1],n[2]},d};}));
    Pose at;at.p[0]=p.x;at.p[1]=p.y;at.p[2]=p.z;float normal[3]{},depth=0;
    const bool overlap=penetration(normal,&depth,&palm,&at,&head,&body);
    assert(!overlap || depth<=.001f);++cases;
    std::cout<<"PASS "<<cases<<" native PhysX geometry cases (CPU only, no game attach)\n";
    FreeLibrary(dll);
}
