-- Exercise the installed EDS profiles through the actual injected routing blocks.
local sourcePath = assert(os.getenv('CPVR_BIKE_TEST_INIT'))
local modRoot = assert(os.getenv('CPVR_EDS_ROOT'))
assert(loadfile(sourcePath)) -- Compile the whole CET script without executing game APIs.
local file = assert(io.open(sourcePath, 'rb'))
local source = file:read('*a')
file:close()
local baseline = assert(source:match("(if %(vehicleType == 'vehicleBikeBaseObject'%) then %-%- CPVRPORT_PSVR2_BIKE_BASELINE_SWAP_V2.-end)"))
local shift = assert(source:match('(local cpvrportIsBike.-CPVRPORT_PSVR2_BIKE_SHIFT_POP.-end)'))
local route = assert(load('return function(vehData, vehicleType) ' .. baseline .. ' return vehData end'))()
local shiftRoute = assert(load('return function(data, vehicleType, GearChangeModeList, vehicleGearChangeModeList, chosenGearMode, config) ' .. shift .. ' return data end'))()
local config = { vehicleResistanceValue = 4, vehicleMachineValue = 40, gearChangeForce = 2 }
GetText = function(s) return s end
ManageSettings = { openFile = function() return config end }
GetVehicleSpeed = function() return 3000 end
GetResistanceTrigger = function() return 3 end
GetFrequency = function(value) return value end
local vehicle = { GetDisplayName = function() return 'Bike' end }
local mode = dofile(modRoot .. '/vehicles/modes/LTResistanceRTMachine.lua')
local gearMode = dofile(modRoot .. '/otherModes/GearChangeModes/Twitching.lua')
local function normal()
    return mode({}, vehicle, false, 3, false, true, false, false, true, false)
end
local original = normal()
local originalShift = gearMode(normal(), false, config)
local bike = route(normal(), 'vehicleBikeBaseObject')
assert(bike.rightTriggerType == original.leftTriggerType and bike.rightForceTrigger == original.leftForceTrigger, 'R2 must inherit the original L2 resistance')
local engineType, engineForce = bike.leftTriggerType, bike.leftForceTrigger
bike = shiftRoute(bike, 'vehicleBikeBaseObject', { gearMode }, { 1 }, 1, config)
assert(bike.rightTriggerType == originalShift.leftTriggerType and bike.rightForceTrigger == originalShift.leftForceTrigger, 'R2 must inherit the original L2 shift pulse')
assert(bike.leftTriggerType == engineType and bike.leftForceTrigger == engineForce, 'L2 engine baseline must survive the shift')
local after = route(normal(), 'vehicleBikeBaseObject')
assert(after.rightTriggerType == original.leftTriggerType and after.rightForceTrigger == original.leftForceTrigger, 'R2 must return to resistance after the shift')
local car = route(normal(), 'vehicleCarBaseObject')
assert(car.leftTriggerType == original.leftTriggerType and car.rightTriggerType == original.rightTriggerType, 'Car baseline changed')
car = shiftRoute(car, 'vehicleCarBaseObject', { gearMode }, { 1 }, 1, config)
assert(car.leftForceTrigger == originalShift.leftForceTrigger and car.rightForceTrigger == originalShift.rightForceTrigger, 'Car shift changed')
print('PASS: actual EDS bike profile gives R2 Resistance -> Machine shift -> Resistance; L2 engine and car routing preserved.')
