-- *****************************************************************************
-- file           hud_fragment_counter.lua
-- project        Lights Out
-- brief          HUD TextRenderer that shows "X / Y fragments collected".
--                Total fragment count is auto-detected by counting all entities
--                tagged "Fragment" — no hardcoding required. Follows the camera
--                (which tracks the player) via offsetX/offsetY.
--
-- All content (c) 2025 DigiPen Institute of Technology Singapore. All rights reserved.
-- *****************************************************************************

Properties = {
    { name = "offsetX", type = "float", default = -830.0, label = "Screen Offset X" },
    { name = "offsetY", type = "float", default =  -420.0, label = "Screen Offset Y" },
}

local HudFragmentCounter = {}

function HudFragmentCounter:onUpdate(entity, dt)
    local player = World.findByTag("Player")
    if not player or not player:isValid() then return end

    -- Keep HUD at a fixed screen position by tracking player (= camera) position
    entity.position = Vec2(player.position.x + self.offsetX,
                           player.position.y + self.offsetY)

    -- Count total fragments and how many have been collected
    local total     = 0
    local collected = 0
    World.eachByTag("Fragment", function(frag)
        total = total + 1
        if World.getScriptProp(frag.id, "collected") then
            collected = collected + 1
        end
    end)

    local label = collected .. " / " .. total .. " fragments collected"
    if entity:hasTextRenderer() and entity:getText() ~= label then
        entity:setText(label)
    end
end

return HudFragmentCounter
