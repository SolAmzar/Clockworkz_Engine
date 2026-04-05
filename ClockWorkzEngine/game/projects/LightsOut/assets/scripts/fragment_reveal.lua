-- *****************************************************************************
-- file           fragment_reveal.lua
-- project        Lights Out
-- brief          Cheat: reveal all fragments (visible without flashlight).
--                Keyboard : 4
--                Gamepad  : North button (Y / Triangle)
--                Press again to toggle off.
--
-- Attach on the CheatManager entity.
-- NOTE: CheatManager.update(dt) is NOT called here — driven by konami_invincibility.lua.
-- *****************************************************************************

Properties = {}

local CheatManager      = nil  -- loaded in onStart
local CheatNotification = nil

local FragmentReveal = {}

FragmentReveal._active = false

-- ----------------------------------------------------------------
local function applyToAllFragments(requiresLight)
    World.eachWithScript("components/fragment.lua", function(frag)
        local collected = World.getScriptProp(frag.id, "collected")
        if not collected then
            World.setScriptProp(frag.id, "requiresLight", requiresLight)
        end
    end)
end

local function toggle(self)
    self._active = not self._active
    applyToAllFragments(not self._active)   -- false = always visible, true = needs light
    CheatNotification.notify("FRAGMENT REVEAL " .. (self._active and "ON" or "OFF"))
end

-- ----------------------------------------------------------------
function FragmentReveal:onStart(entity)
    CheatManager      = require("cheat_manager")
    CheatNotification = require("cheat_notification")
    local cb = function() toggle(self) end

    -- Keyboard: number 4
    CheatManager.register({ "Num4" }, cb, "Fragment Reveal (KB)")

    -- Gamepad: North button (Y / Triangle)
    CheatManager.register({ "Y" }, cb, "Fragment Reveal (GP)")
end

function FragmentReveal:onUpdate(entity, dt)
    -- Re-apply every frame so fragments spawned mid-level are also revealed.
    if self._active then
        applyToAllFragments(false)
    end
end

return FragmentReveal
