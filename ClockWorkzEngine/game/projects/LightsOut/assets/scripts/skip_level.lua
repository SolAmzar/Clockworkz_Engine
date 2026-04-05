-- *****************************************************************************
-- file           skip_level.lua
-- project        Lights Out
-- brief          immediately load the next level.
-- author         [Syafiq]
--
-- NOTE: Level order is hardcoded below. No per-scene setup needed.
-- *****************************************************************************

Properties = {}

local CheatManager      = nil
local CheatNotification = nil

-- Maps current level → { next/prev level name, spawn position matching the in-game portal }
local NEXT_LEVEL = {
    ["assets/scenes/Level_Save.json"]        = { level = "Level_Save_Part_2.json", x = 2250, y = 1100 },
    ["assets/scenes/Level_Save_Part_2.json"] = { level = "Level_Save_03.json",     x = -100, y = -100  },
    ["assets/scenes/Level_Save_03.json"]     = { level = "Level_Save_04.json",     x = 250,  y = -750 },
    ["assets/scenes/Level_Save_04.json"]     = { level = "Level_Save_05.json",     x = -1150,y = -350 },
}

local PREV_LEVEL = {
    ["assets/scenes/Level_Save_Part_2.json"] = { level = "Level_Save.json",        x = -1050, y = 250  },
    ["assets/scenes/Level_Save_03.json"]     = { level = "Level_Save_Part_2.json", x = 2250, y = -250 },
    ["assets/scenes/Level_Save_04.json"]     = { level = "Level_Save_03.json",     x = 850,  y = -1550},
    ["assets/scenes/Level_Save_05.json"]     = { level = "Level_Save_04.json",     x = 350,  y = -650 },
}

local SkipLevel = {}

-- ----------------------------------------------------------------
local function doSkip()
    local current = World.getCurrentLevel()
    local entry   = NEXT_LEVEL[current]

    if entry then
        CheatNotification.notify("SKIPPING TO: " .. entry.level)
        _G.nextSpawnPos = { x = entry.x, y = entry.y }
        World.loadLevel(entry.level)
    else
        CheatNotification.notify("SKIP: no next level for " .. (current or "?"))
    end
end

local function doPrev()
    local current = World.getCurrentLevel()
    local entry   = PREV_LEVEL[current]

    if entry then
        CheatNotification.notify("GOING BACK TO: " .. entry.level)
        _G.nextSpawnPos = { x = entry.x, y = entry.y }
        World.loadLevel(entry.level)
    else
        CheatNotification.notify("SKIP: no previous level for " .. (current or "?"))
    end
end

-- ----------------------------------------------------------------
function SkipLevel:onStart(entity)
    CheatManager      = require("cheat_manager")
    CheatNotification = require("cheat_notification")

    CheatManager.register(
        { "Num5" },
        function() doPrev() end,
        "Previous Level"
    )
    CheatManager.register(
        { "Num6" },
        function() doSkip() end,
        "Skip Level"
    )
end

function SkipLevel:onUpdate(entity, dt)
    if Gamepad:isActionPressed("rb") then doSkip() end
    if Gamepad:isActionPressed("lb") then doPrev() end
end

return SkipLevel
