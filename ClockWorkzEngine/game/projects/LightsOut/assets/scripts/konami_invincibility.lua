-- *****************************************************************************
-- file           konami_invincibility.lua
-- project        Lights Out
-- brief          toggle player invincibility.
-- author         [Syafiq]
-- HOW TO ATTACH (in the Level Editor):
--   1. Create an empty entity (or reuse a persistent game-manager entity).
--   2. Add a LuaScript component to it.
--   3. Add this file as a script slot
--   4. That entity must persist across the scene
-- *****************************************************************************

Properties = {}

local CheatManager       = nil  -- loaded in onStart
local CheatNotification  = nil

local KonamiInvincibility = {}

-- Internal state
KonamiInvincibility._active   = false
KonamiInvincibility._playerId = nil

-- ----------------------------------------------------------------
-- Helpers
-- ----------------------------------------------------------------
local function findPlayer()
    local p = World.findByTag("Player")
    return (p and p:isValid()) and p or nil
end

local function setPlayerInvincible(playerId, on)
    World.setScriptProp(playerId, "_invincible", on)
end

-- ----------------------------------------------------------------
-- Lifecycle
-- ----------------------------------------------------------------
function KonamiInvincibility:onStart(entity)
    CheatManager      = require("cheat_manager")
    CheatNotification = require("cheat_notification")
    CheatManager.register(
        { "Num1" },
        function()
            self._active = not self._active

            if not self._playerId then
                local player = findPlayer()
                if player then self._playerId = player.id end
            end

            if self._playerId then
                setPlayerInvincible(self._playerId, self._active)
                CheatNotification.notify("INVINCIBILITY " .. (self._active and "ON" or "OFF"))
            else
                CheatNotification.notify("INVINCIBILITY: player not found")
            end
        end,
        "Konami Invincibility"
    )
end

function KonamiInvincibility:onUpdate(entity, dt)
    if not CheatManager then return end
    CheatManager.update(dt)
    CheatNotification.update(dt)

    if self._active then
        local player = findPlayer()
        if player then
            self._playerId = player.id
            setPlayerInvincible(player.id, true)
        end
    end
end

return KonamiInvincibility
