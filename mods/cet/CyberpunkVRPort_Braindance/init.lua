-- BRAINDANCE SCENE CAMERA, published to the plugin.
--
-- A braindance renders through a camera that belongs to the SCENE, not to the player and not to a
-- device the player took over. The plugin's camera writer classifies it as unknown and discards it,
-- so nothing of ours ever reached it: the second eye kept whatever pose it last had and the left eye
-- showed lighting with no world. Measured in `The Information`, entering the BD chair:
--
--     main=2701  vrcam=2700   sep=(0.059, 0.000, 0.000)     healthy, 1:1, clean IPD
--     main=4501  vrcam=3072   sep=(-426.9, -581.6, -32.9)   VRCAM frozen, 43 patches per 900
--
-- The plugin cannot be told WHICH object that camera is -- script cannot hand over the object. What
-- script CAN hand over is a DESCRIPTION: the pose the scene system reports for its camera, every
-- frame. BraindanceCameraMatch then finds the object by matching a patched component against it.
--
-- Imported from upstream 0.1.6 (b4a7446) as a standalone mod rather than folded into ForceFPP, so it
-- can be removed by deleting one folder and cannot disturb the first-person path.

local S = { on = false, owns = false, pose = "none", fov = 0.0 }
local memo = { bdSystem = nil, sceneIface = nil }

local function clearScene()
  if type(VRSceneCamera) == "function" then
    pcall(function() VRSceneCamera(0, 0, 0, 0, 0, 0, 0, 1) end)
  end
end

registerForEvent("onUpdate", function()
  if type(VRBraindance) ~= "function" or type(VRSceneCamera) ~= "function" then return end

  local on = false
  pcall(function()
    if memo.bdSystem == nil then memo.bdSystem = Game.GetBraindanceSystem() end
    if memo.bdSystem ~= nil then on = memo.bdSystem:GetIsInBraindance() end
  end)
  S.on = on

  if not on then
    pcall(function() VRBraindance(0, 0.0) end)
    clearScene()
    S.owns, S.pose = false, "none"
    return
  end

  -- The fov the game reports for the ACTIVE camera. This is the only identity script can offer for a
  -- scene camera, and the matcher needs it: pose alone latched a `Senses` component upstream, which
  -- rides the head and therefore sits exactly where the camera does.
  local fov = 0.0
  pcall(function()
    local cam = Game.GetPlayer():GetFPPCameraComponent()
    if cam ~= nil then fov = cam:GetFOV() end
  end)
  S.fov = fov
  pcall(function() VRBraindance(1, fov) end)

  local si = nil
  pcall(function()
    if memo.sceneIface == nil then memo.sceneIface = Game.GetSceneSystem():GetScriptInterface() end
    si = memo.sceneIface
  end)
  if si == nil then clearScene(); S.pose = "no scene interface"; return end

  -- ONLY WHILE THE SCENE ACTUALLY OWNS THE CAMERA. In the braindance EDITOR the player flies the
  -- camera and this getter keeps returning the last pose the replay left behind -- which would nail
  -- the second eye to that spot while MAIN flies away.
  local owns = false
  pcall(function() owns = si:GetSceneSystemCameraControlEnabled() end)
  S.owns = owns
  if not owns then clearScene(); S.pose = "editor: the player flies the camera"; return end

  local sp, sq = nil, nil
  pcall(function() sp = si:GetSceneSystemCameraLastCameraPosition() end)
  pcall(function() sq = si:GetSceneSystemCameraLastCameraOrientation() end)
  local sv = nil
  if sp ~= nil then pcall(function() sv = WorldPosition.ToVector4(sp) end) end

  -- AND ONLY A REAL ONE. Outside a scene this getter returns the origin, which is over a kilometre
  -- from the player; handing that over would place the second eye's lens in the void.
  local real = (sv ~= nil) and ((math.abs(sv.x) + math.abs(sv.y) + math.abs(sv.z)) > 1.0)
  if real and sq ~= nil then
    pcall(function() VRSceneCamera(1, sv.x, sv.y, sv.z, sq.i, sq.j, sq.k, sq.r) end)
    S.pose = string.format("%.1f %.1f %.1f", sv.x, sv.y, sv.z)
  else
    clearScene()
    S.pose = "the scene reports no camera pose"
  end
end)

registerForEvent("onShutdown", function()
  if type(VRBraindance) == "function" then pcall(function() VRBraindance(0, 0.0) end) end
  clearScene()
end)

registerForEvent("onDraw", function()
  if not S.on then return end
  ImGui.Begin("VR Braindance")
  ImGui.Text(string.format("in braindance : %s", tostring(S.on)))
  ImGui.Text(string.format("scene owns cam: %s", tostring(S.owns)))
  ImGui.Text(string.format("scene pose    : %s", S.pose))
  ImGui.Text(string.format("active fov    : %.3f", S.fov))
  ImGui.End()
end)
