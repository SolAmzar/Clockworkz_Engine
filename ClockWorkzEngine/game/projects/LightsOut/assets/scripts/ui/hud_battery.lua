-- *****************************************************************************
-- file           hud_battery.lua
-- project        Lights Out
-- brief          HUD sprite that swaps between 5 battery textures based on the
--                player's current charge level. Follows the camera (which tracks
--                the player) by repositioning itself via offsetX/offsetY each frame.
--
-- All content (c) 2025 DigiPen Institute of Technology Singapore. All rights reserved.
-- *****************************************************************************

Properties = {
    -- Margin (in world units) inward from the bottom-right corner of the screen
    { name = "marginX",    type = "float", default =  80.0,  label = "Margin from Right"   },
    { name = "marginY",    type = "float", default =  60.0,  label = "Margin from Bottom"  },
    -- Charge % above which each texture is shown (battery.lua charge is 0–100)
    { name = "threshold4", type = "float", default =  75.0,  label = "Full Threshold %"    },
    { name = "threshold3", type = "float", default =  50.0,  label = "3/4 Threshold %"     },
    { name = "threshold2", type = "float", default =  25.0,  label = "Half Threshold %"    },
    { name = "threshold1", type = "float", default =   0.0,  label = "Low Threshold %"     },
    -- Seconds between texture checks (0 = every frame)
    { name = "updateRate", type = "float", default =   0.05, label = "Update Interval (s)" },
}

local HudBattery = {}
HudBattery._timer = 0.0

local function findPlayer()
    local player = World.findByTag("Player")
    if player and player:isValid() then
        return player
    end

    local found = nil
    World.eachWithScript("player_controller.lua", function(entityRef)
        if found == nil and entityRef:isValid() then
            found = entityRef
        end
    end)
    return found
end

-- Texture IDs (registered in Textures.json) → source assets:
--   "battery_full"  → energy 4.png
--   "battery_3/4"   → energy 3.png
--   "battery_2/4"   → energy 2.png
--   "battery_1/4"   → energy 1.png
--   "battery_empty" → energy flat.png

function HudBattery:onUpdate(entity, dt)
    local player = findPlayer()
    if not player or not player:isValid() then return end

    -- Keep HUD at a fixed screen position by tracking player (= camera) position
    -- Bottom-right corner: offset = +(halfW - marginX, -(halfH - marginY))
    local zoom  = Engine.cameraZoom()
    local halfW = Engine.screenWidth()  * 0.5 / zoom
    local halfH = Engine.screenHeight() * 0.5 / zoom
    entity.position = Vec2(player.position.x + halfW - self.marginX,
                           player.position.y - halfH + self.marginY)

    -- Throttle the texture-swap check
    self._timer = self._timer + dt
    if self._timer < self.updateRate then return end
    self._timer = 0.0

    -- Select texture from inspector-configurable thresholds
    local thresholds = {
        { threshold = self.threshold4, id = "battery_full"  },
        { threshold = self.threshold3, id = "battery_3/4"   },
        { threshold = self.threshold2, id = "battery_2/4"   },
        { threshold = self.threshold1, id = "battery_1/4"   },
        { threshold = -1.0,            id = "battery_empty" },
    }

    local charge = player:hasBattery() and player:getBatteryCharge() or 100.0
    local texId = "battery_empty"
    for _, entry in ipairs(thresholds) do
        if charge > entry.threshold then
            texId = entry.id
            break
        end
    end

    if entity:hasSpriteRenderer() and entity:getSpriteTextureId() ~= texId then
        entity:setSpriteTextureId(texId)
    end
end

return HudBattery
