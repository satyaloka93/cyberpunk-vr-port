#include "Hooks/FarInlineHook.hpp"
#include <Windows.h>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace cvr::hooks;
using Fn=int(*)(int);
void Check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int Detour(int n){return n+100;}
DWORD WINAPI Idle(void*){return 0;}
struct Code {
 unsigned char* p=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE));
 Code(){Check(p!=nullptr,"allocation");std::memset(p,0x90,4096);p[14]=0x8D;p[15]=0x41;p[16]=7;p[17]=0xC3;}
 ~Code(){VirtualFree(p,0,MEM_RELEASE);}
 void Flush(){Check(FlushInstructionCache(GetCurrentProcess(),p,4096)!=0,"flush");}
};
int main()try {
 {Code c;c.Flush();void* original=nullptr;Check(reinterpret_cast<Fn>(c.p)(5)==12,"fixture");
  Check(FarInlineHook(c.p,reinterpret_cast<void*>(&Detour),&original)==FarHookResult::Ok,"movable hook");
  Check(reinterpret_cast<Fn>(c.p)(5)==105,"detour execution");
  Check(reinterpret_cast<Fn>(original)(5)==12,"trampoline execution");}
 for(int kind=0;kind<4;++kind){Code c;
  if(kind==0){c.p[0]=0x48;c.p[1]=0x8B;c.p[2]=0x05;std::memset(c.p+3,0,4);} // RIP-relative
  if(kind==1){c.p[0]=0xE9;std::memset(c.p+1,0,4);} // relative jump
  if(kind==2)c.p[0]=0xC3;
  if(kind==3){c.p[0]=0xFF;c.p[1]=0xE0;} // indirect jump
  c.Flush();unsigned char before[32];std::memcpy(before,c.p,32);void* original=nullptr;
  Check(FarInlineHook(c.p,reinterpret_cast<void*>(&Detour),&original)==FarHookResult::Unsupported,"unsupported accepted");
  Check(!original&&!std::memcmp(before,c.p,32),"unsupported changed code");}
 {Code c;c.Flush();void* original=nullptr;unsigned char before[32];std::memcpy(before,c.p,32);
  HANDLE thread=CreateThread(nullptr,0,Idle,nullptr,CREATE_SUSPENDED,nullptr);Check(thread!=nullptr,"thread");
  CONTEXT saved{};saved.ContextFlags=CONTEXT_CONTROL;Check(GetThreadContext(thread,&saved)!=0,"get context");
  CONTEXT parked=saved;parked.Rip=reinterpret_cast<DWORD64>(c.p+1);Check(SetThreadContext(thread,&parked)!=0,"park context");
  const auto result=FarInlineHook(c.p,reinterpret_cast<void*>(&Detour),&original);
  const bool restored=SetThreadContext(thread,&saved)!=0;
  const auto suspendCount=ResumeThread(thread);const auto waited=WaitForSingleObject(thread,5000);CloseHandle(thread);
  Check(restored&&suspendCount==1&&waited==WAIT_OBJECT_0,"busy hook unbalanced thread suspension");
  Check(result==FarHookResult::Busy&&!original&&!std::memcmp(before,c.p,32),"busy hook changed code");
  Check(FarInlineHook(c.p,reinterpret_cast<void*>(&Detour),&original)==FarHookResult::Ok,"retry after busy");
  Check(reinterpret_cast<Fn>(c.p)(5)==105&&reinterpret_cast<Fn>(original)(5)==12,"retry execution");}
 Check(FarInlineHook(nullptr,reinterpret_cast<void*>(&Detour),nullptr)==FarHookResult::Failed,"null accepted");
 std::cout<<"PASS native detour/trampoline, unsupported prologues, parked-thread busy rejection and retry, suspension balance\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
