#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstring>
extern void Log(const char*,...);
namespace cvr::hookaudit {
inline std::atomic<void*> targets[3]{}, originals[3]{};
inline const char* names[]{"slSetTag","slSetConstants","slEvaluateFeature"};
inline void Register(unsigned i,void* target,void* original){originals[i].store(original);targets[i].store(target);}
inline void Owner(void* p,wchar_t* text){HMODULE m{};text[0]=0;
    if(p && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(p),&m))GetModuleFileNameW(m,text,MAX_PATH);}
inline void* Jump(void* p){unsigned char b[16]{};SIZE_T n{};
    if(!p || !ReadProcessMemory(GetCurrentProcess(),p,b,sizeof(b),&n) || n!=sizeof(b))return nullptr;
    int32_t d{};
    if(b[0]==0xE9){std::memcpy(&d,b+1,4);return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p)+5+d);}
    if(b[0]==0xFF && b[1]==0x25){std::memcpy(&d,b+2,4);void* result{};
        if(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(p)+6+d),&result,sizeof(result),&n))return result;}
    return nullptr;
}
inline void Report(){
    static std::atomic<uint64_t> next{};const auto now=GetTickCount64();auto due=next.load();
    if(now<due || !next.compare_exchange_strong(due,now+5000))return;
    for(unsigned i=0;i<3;++i){auto target=targets[i].load();if(!target)continue;
        auto jump=Jump(target);auto relay=Jump(jump);auto destination=relay?relay:jump;
        wchar_t owner[MAX_PATH]{},destinationOwner[MAX_PATH]{};Owner(target,owner);Owner(destination,destinationOwner);
        Log("[hook-audit] %s target=%p owner=%ls original=%p jump=%p destination=%p owner=%ls\n",
            names[i],target,owner,originals[i].load(),jump,destination,destinationOwner);}
}
}
