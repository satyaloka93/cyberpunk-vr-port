-- Run from mods/cet/CyberpunkVRPort_Menu with Lua 5.1+ (no game required).
local A=require('modules/availability')
assert(A.enabled(), 'older plugin compatibility')
VRInterfaceEnabled=function() return false end
assert(not A.enabled())
VRInterfaceEnabled=function() error('native unavailable') end
assert(not A.enabled())
VRInterfaceEnabled=function() return true end
assert(A.enabled())
VRInterfaceEnabled=function() return false end

local overrides={}
Override=function(class,method,fn) overrides[class..'.'..method]=fn end
ObserveBefore=function() end
ObserveAfter=function() end
NameToString=function(name) return name end
FromVariant=function(value) return value end
local V=require('modules/vendor_visit')
local S=require('modules/stash_visit')
local shared={}
V.bind(shared)
S.bind(shared)
-- No Game APIs exist here: off must return before touching their objects.
assert(V.begin(shared,{data={}},nil)==false)
assert(S.begin(shared,{},false)==false)
local calls=0
local function wrapped(...) calls=calls+1;return 'original' end
assert(overrides['gameuiInGameMenuGameController.OnOpenStorage']({},123,wrapped)=='original')
assert(overrides['MenuScenario_Idle.OnLeaveScenario']({},'MenuScenario_Vendor',wrapped)=='original')
assert(shared.vendorPresentation==nil and shared.vendorPresentationEarly==nil)
assert(overrides['UISystem.RequestVendorMenu']({}, {}, 'MenuScenario_Vendor',wrapped)=='original')
assert(calls==3, 'disabled interface must preserve original menus')
local polled=nil
Game={GetPlayer=function() return {} end, GetUISystem=function() return {} end}
VRMenuVendorPoll=function(on) polled=on;return nil end
VRMenuVendorResolve=function() error('nothing to resolve') end
V.pollNative(shared)
assert(polled==false, 'native vendor interceptor must not arm while disabled')
VRInterfaceEnabled=function() return true end
V.pollNative(shared)
assert(polled==true, 'native vendor interceptor can rearm')
for _,name in ipairs({'menu','vendor_visit','stash_visit','availability'}) do
  assert(loadfile('modules/'..name..'.lua'))
end
print('PASS interface availability, disabled vendor/stash fallback, native poll off/on, and modified Lua syntax')
