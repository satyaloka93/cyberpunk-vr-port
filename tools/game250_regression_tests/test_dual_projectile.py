"""Exercise production projectile selection and per-hand spread rotation with fake attack memory."""
from pathlib import Path
import subprocess,tempfile
from lupa.lua54 import LuaRuntime
r=Path(__file__).resolve().parents[2]
s=(r/'src/Natives/OrientationProvider.cpp').read_text()
a=s.index('static bool ProviderUsesLeftWeapon(');b=s.index('\nstatic uintptr_t __fastcall PosProvStub33',a)
helper=s[a:b].replace('__try','try').replace('__except(EXCEPTION_EXECUTE_HANDLER)','catch(...)')
a=s.index('                        float mq[4]',s.index('if (mode == 6)'));b=s.index('                    } else if (mode == 5)',a)
rotation=s[a:b]
code='''#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cassert>
#include <cmath>
#include "Utils/SharedSlots.hpp"
float slots[256]{};float* g_pSharedHands=slots;
std::atomic<uintptr_t> g_dualWeaponEntity[2]{};
constexpr int kProvAttackHandleOffsets[]={0xA8,0xB8,0xC8,0xD8};
bool ProvPlausiblePtr(uintptr_t p){return p>0x10000 && !(p&7);}
'''+helper+'''
volatile float g_provMuzzleQ[4]={0,0,0,1},g_leftLaunchQuat[4]={0,0,.70710678f,.70710678f};
volatile float g_provDeltaQ[4]{};volatile uint32_t g_provDeltaSeq=~0u;
uint32_t g_provMuzzleSeq=1;int g_provOverrides=0;
void rotate(bool leftLaunch,float* q){
'''+rotation+'''}
int main(){
 alignas(8) uintptr_t provider[32]{},source[8]{},attack[48]{};
 provider[0x78/8]=provider[0x88/8]=uintptr_t(source);source[0x10/8]=uintptr_t(attack);
 slots[vrshared::kDualWieldActive]=slots[vrshared::kLeftMuzzleValid]=1;
 slots[vrshared::kLeftTriggerAnalog]=1;
 g_dualWeaponEntity[0]=0x123400;g_dualWeaponEntity[1]=0x567800;
 attack[0xB8/8]=0x567800; // primary record/participant must not override exclusive left press
 assert(ProviderUsesLeftWeapon(uintptr_t(provider),0x78));
 assert(ProviderUsesLeftWeapon(uintptr_t(provider),0x88));
 slots[vrshared::kRightTriggerAnalog]=1;
 assert(!ProviderUsesLeftWeapon(uintptr_t(provider),0x78));
 attack[0xB8/8]=0x123400;
 assert(ProviderUsesLeftWeapon(uintptr_t(provider),0x78));
 assert(ProviderUsesLeftWeapon(uintptr_t(provider),0x88));
 slots[vrshared::kLeftMuzzleValid]=0;assert(!ProviderUsesLeftWeapon(uintptr_t(provider),0x78));
 float l[4]={0,0,0,1},rr[4]={0,0,0,1};rotate(true,l);rotate(false,rr);
 assert(std::abs(l[2]-.70710678f)<.0001f && std::abs(rr[2])<.0001f && rr[3]==1);
 float pellet[4]={.1f,0,0,.994987f};rotate(true,pellet);
 assert(std::abs(pellet[0])>.01f && std::abs(pellet[1])>.01f); // spread preserved around left, not replaced
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.cpp').write_text(code)
 subprocess.run(['g++','-std=c++20','-I'+str(r/'include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
LuaRuntime().execute('assert(load(...))',(r/'mods/cet/CyberpunkVRPort_Reload/dual_wield.lua').read_text())
print('PASS: actual projectile hand selection, position/orientation agreement, independent cone rotation, Lua syntax')
