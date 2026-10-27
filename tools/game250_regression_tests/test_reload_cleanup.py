from pathlib import Path
from lupa import LuaRuntime
r=Path(__file__).resolve().parents[2].joinpath('mods/cet/CyberpunkVRPort_Reload').resolve()
l=LuaRuntime(unpack_returned_tuples=True)
l.execute('package.path = ... .. "/?.lua;" .. package.path',str(r))
l.execute('''VRRigWrite=function() end;VRRigWriteRot=function() end;VRRigWriteClear=function() end
GetVRSharedSlot=function() return 0 end;VRHandRawModel=function() return {x=0,y=0,z=0,w=1} end
Vector4={new=function(...) return {...} end};cleared={}
VRReloadFingerApply=function(side,on) cleared[side]=on end
VRHandStopRot=function() end;VRHandStopModel=function() end
''')
l.globals().reload=l.execute((r/'reload/reload.lua').read_text())
l.execute('''
local function up(f,name)
 for i=1,200 do local n,v=debug.getupvalue(f,i);if not n then break end;if n==name then return v end end
 error('missing upvalue '..name)
end
-- Reproduce support-hand ownership from a preview/slide, then switch to a
-- rifle without a physical-reload profile. It must clear the reload fingers.
local fade=up(reload.frame,'fadeS');fade.hand=0;fade.blend=1
local slide=up(reload.frame,'slideS');slide.grabbed=true;slide.hand=0
local w={GetEntityID=function() return {hash=900} end,GetComponents=function() return {} end}
reload.frame(w,{},1,.016)
assert(cleared[0]==0 and fade.hand==nil and slide.grabbed==false and reload.ownedHand==nil)
''')
print('PASS: actual module releases reload ownership after switching to an unsupported gun, allowing native two-handed grip again.')
s=(r/'reload/reload.lua').read_text()
for p in [r/'init.lua',r/'reload/reload.lua']:l.execute('assert(load(...))',p.read_text())
a=s.index('local function slotModel(');b=s.index('\nend',a)+4
l.execute('CName={new=function() error("invalid slot must not reach CName constructor") end}; slotNames={};'+s[a:b]+'\nassert(slotModel({},nil)==nil); assert(slotModel({},"")==nil)')
a=s.index('local function gameReloading()');b=s.index('\nend',a)+4
l.execute('''selected=13;fields={RightWeapon=13,Weapon=7};value=2
Game={GetPlayer=function() return {GetPlayerStateMachineBlackboard=function() return {GetInt=function(_,field) assert(field==selected);return value end} end} end}
GetAllBlackboardDefs=function() return {PlayerStateMachine=fields} end
EnumValueToString=function(_,v) return v==2 and 'Reload' or 'Ready' end
'''+s[a:b]+'''\nassert(gameReloading());value=5;assert(not gameReloading());fields.RightWeapon=nil;selected=7;value=2;assert(gameReloading());value=nil;assert(not gameReloading())''')
print('PASS: syntax, missing slide-slot guard, 2.50 RightWeapon state, legacy fallback, reload enum and missing blackboard value.')
l.execute('''
local function up(f,name)
 for i=1,200 do local n,v=debug.getupvalue(f,i);if not n then break end;if n==name then return v end end
 error('missing upvalue '..name)
end
local home=up(reload.frame,'magHome');local hatch=up(home,'hatchS')
hatch.open=true;hatch.p=1;hatch.openT=20;hatch.hp={5,5,5};hatch.btnPrev=true;hatch.lat=99
home()
assert(hatch.open==false and hatch.p==0 and hatch.openT==0 and hatch.hp==nil and hatch.lat==nil and not hatch.btnPrev)
''')
print('PASS: weapon cleanup clears cylinder latch and stale flick velocity history.')
l.execute('''
local function up(f,name)
 for i=1,200 do local n,v=debug.getupvalue(f,i);if not n then break end;if n==name then return v end end
 error('missing upvalue '..name)
end
local home=up(reload.frame,'magHome')
local hatch=up(home,'hatchS');local ham=up(home,'hamS');local spin=up(home,'spinS')
local mag=up(home,'magS');local own=up(home,'ownS')
hatch.open=true;hatch.p=1;ham.p=1;ham.want=1;ham.wrote=true;spin.wrote=true;own.tagged=true;mag.state='in'
local writesCleared=0;VRRigWriteClear=function() writesCleared=writesCleared+1 end
local function wrapper()
 return {GetEntityID=function() return {hash=901} end,GetComponents=function() return {} end}
end
reload.frame(wrapper(),{},1,.016)
assert(writesCleared>0 and hatch.p==0 and not hatch.open and ham.p==0 and not spin.wrote and not own.tagged)
-- CET can return a new userdata wrapper each frame for the same entity. Logging
-- must use the stable ID; repeated wrappers must not exhaust the audit budget.
local audit=up(reload.frame,'reloadAudit');local count=audit.count
reload.frame(wrapper(),{},1,.016)
assert(audit.count==count)
''')
print('PASS: revolver-to-unsupported switch clears native part writes; repeated entity wrappers do not exhaust audit budget.')
