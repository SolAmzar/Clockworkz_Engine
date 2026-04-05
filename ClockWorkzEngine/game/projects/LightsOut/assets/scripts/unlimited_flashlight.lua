-- *****************************************************************************
-- file           unlimited_flashlight.lua
-- project        Lights Out
-- brief          unlimited torchlight
-- author         [Syafiq]
--
-- HOW TO ATTACH (in the Level Editor):
--   1. Create an empty entity (or reuse a persistent game-manager entity).
--   2. Add a LuaScript component to it.
--   3. Add this file as a script slot
--   4. That entity must persist across the scen
-- *****************************************************************************

Properties = {}

local CheatManager      = nil  -- loaded in onStart
local CheatNotification = nil

local UnlimitedFlashlight = {}

UnlimitedFlashlight._active    = false
UnlimitedFlashlight._playerId  = nil
UnlimitedFlashlight._prevCombo = false

-- ----------------------------------------------------------------
local function findPlayer()
    local p = World.findByTag("Player")
    return (p and p:isValid()) and p or nil
end

local function toggle(self)
    self._active = not self._active

    if not self._playerId then
        local player = findPlayer()
        if player then self._playerId = player.id end
    end

    if self._playerId then
        World.setScriptProp(self._playerId, "_unlimited", self._active)
        CheatNotification.notify("UNLIMITED FLASHLIGHT " .. (self._active and "ON" or "OFF"))
    else
        CheatNotification.notify("UNLIMITED FLASHLIGHT: player not found")
    end
end

-- ----------------------------------------------------------------
function UnlimitedFlashlight:onStart(entity)
    CheatManager      = require("cheat_manager")
    CheatNotification = require("cheat_notification")
    -- Keyboard control
    CheatManager.register(
        { "Num2" },
        function() toggle(self) end,
        "Unlimited Flashlight"
    )
end

function UnlimitedFlashlight:onUpdate(entity, dt)
    -- Gamepad control
    local combo = Gamepad:isActionDown("lt") and Gamepad:isActionDown("btn_x")
    if combo and not self._prevCombo then
        toggle(self)
    end
    self._prevCombo = combo

    if self._active then
        local player = findPlayer()
        if player then
            self._playerId = player.id
            World.setScriptProp(self._playerId, "_unlimited", true)
        end
    end
end

return UnlimitedFlashlight
