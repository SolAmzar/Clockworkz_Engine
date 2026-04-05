-- *****************************************************************************
-- file           cheat_notification.lua
-- project        Lights Out
-- brief          notifify the player via textRenderer in game
-- author         [Syafiq] 
-- Call CheatNotification.notify(message) from any cheat script.
-- *****************************************************************************


local CheatNotification = {}

local DISPLAY_DURATION = 2.5   -- number of seconds the message stays on screen
local FADE_DURATION    = 1.0   -- How long it fades
local MARGIN_Y         = 80.0
local MARGIN_X         = 80.0

CheatNotification._timer = 0.0

local function getEntity()
    local e = World.findByTag("CheatNotification")
    return (e and e:isValid() and e:hasTextRenderer()) and e or nil
end

local function getPlayer()
    local p = World.findByTag("Player")
    return (p and p:isValid()) and p or nil
end

function CheatNotification.notify(message)
    CheatNotification._timer = DISPLAY_DURATION
    local e = getEntity()
    if e then
        e:setText(message)
        e:setTextColor(0.2, 1.0, 0.2)
    end
end

function CheatNotification.update(dt)
    local e = getEntity()
    if not e then return end

    -- Follow camera
    local player = getPlayer()
    if player then
        local zoom  = Engine.cameraZoom()
        local halfW = Engine.screenWidth()  * 0.5 / zoom
        local halfH = Engine.screenHeight() * 0.5 / zoom
        e.position = Vec2(player.position.x - halfW + MARGIN_X,
                          player.position.y + halfH - MARGIN_Y)
    end

    -- Count down and clear text when expired
    if CheatNotification._timer <= 0.0 then return end
    CheatNotification._timer = CheatNotification._timer - dt

    if CheatNotification._timer <= 0.0 then
        CheatNotification._timer = 0.0
        e:setText("")
        e:setTextColor(0.2, 1.0, 0.2)
    elseif CheatNotification._timer < FADE_DURATION then
        -- Fade
        local t = CheatNotification._timer / FADE_DURATION
        e:setTextColor(0.2 * t, 1.0 * t, 0.2 * t)
    end
end

return CheatNotification
