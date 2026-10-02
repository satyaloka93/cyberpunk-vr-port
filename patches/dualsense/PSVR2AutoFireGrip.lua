-- Add only grip recoil when a rapid auto weapon's stock R2 profile does not
-- already provide Machine/AutomaticGun rhythm. Never change adaptive triggers.
local eligible = {
    Wea_Handgun = true, Wea_Rifle = true, Wea_SubmachineGun = true,
    Wea_LightMachineGun = true, Wea_HeavyMachineGun = true,
    Wea_Shotgun = true, Wea_ShotgunDual = true,
}
local activeWeapon, remaining = nil, 0

local function AutoFireGrip(effect, weaponType, state, triggerType, shotsPerSecond,
                            weaponName, delta, isMelee, isCyberware, enabled, publishPulse)
    local active = enabled and type(effect) == 'table' and eligible[weaponType] and
        not isMelee and not isCyberware and state == 'Shoot' and
        (triggerType == 'FullAuto' or triggerType == 'Charge') and
        type(shotsPerSecond) == 'number' and shotsPerSecond == shotsPerSecond and
        shotsPerSecond > 5 and shotsPerSecond < math.huge and
        effect.rightTriggerType ~= 'Machine' and effect.rightTriggerType ~= 'AutomaticGun' and
        type(publishPulse) == 'function'
    if not active then
        activeWeapon, remaining = nil, 0
        return
    end
    if activeWeapon ~= weaponName then
        activeWeapon, remaining = weaponName, 0
    end
    local interval = 1 / math.min(shotsPerSecond, 20)
    local elapsed = type(delta) == 'number' and delta > 0 and math.min(delta, 0.1) or 0
    remaining = remaining - elapsed
    if remaining <= 0 then
        -- Right hand is 1 in the isolated 0.1.7 haptic IPC. The bridge mixes
        -- this with audio/semantic haptics; it does not touch the R2 motor.
        pcall(publishPulse, 1, 0.68, 36)
        remaining = interval + math.max(remaining, -interval / 2)
    end
end

return AutoFireGrip
