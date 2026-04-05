-- *****************************************************************************
-- file           speed_hack.lua
-- project        Lights Out
-- brief          Spped hack
-- author         [Syafiq]

-- HOW TO ATTACH (in the Level Editor):
--   1. Create an empty entity (or reuse a persistent game-manager entity).
--   2. Add a LuaScript component to it.
--   3. Add this file as a script slot
--   4. That entity must persist across the scen
-- *****************************************************************************

Properties = {}

local CheatManager      = nil  -- loaded in onStart
local CheatNotification = nil

local SpeedHack = {}

SpeedHack._active   = false
SpeedHack._playerId = nil

local SPEED_MULTIPLIER = 3.0

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
        local base = World.getScriptProp(self._playerId, "baseWalkSpeed")
        if base then
            local newSpeed = self._active and (base * SPEED_MULTIPLIER) or base
            World.setScriptProp(self._playerId, "walkSpeed", newSpeed)
        end
        CheatNotification.notify("SPEED HACK " .. (self._active and ("ON (x" .. SPEED_MULTIPLIER .. ")") or "OFF"))
    else
        CheatNotification.notify("SPEED HACK: player not found")
    end
end

-- ----------------------------------------------------------------
function SpeedHack:onStart(entity)
    CheatManager      = require("cheat_manager")
    CheatNotification = require("cheat_notification")
    -- Keyboard: number 3
    CheatManager.register(
        { "Num3" },
        function() toggle(self) end,
        "Speed Hack"
    )
end

function SpeedHack:onUpdate(entity, dt)
    -- Gamepad: hold RT and click L3
    if Gamepad:isActionDown("rt") and Gamepad:isActionPressed("l3") then
        toggle(self)
    end

    -- Re-apply every frame in case the player entity was recreated.
    if self._active then
        local player = findPlayer()
        if player then
            self._playerId = player.id
            local base = World.getScriptProp(self._playerId, "baseWalkSpeed")
            if base then
                World.setScriptProp(self._playerId, "walkSpeed", base * SPEED_MULTIPLIER)
            end
        end
    end
end

return SpeedHack
