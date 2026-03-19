-- *****************************************************************************
-- file           hud_fragment_counter.lua
-- project        Lights Out
-- brief          HUD TextRenderer that shows "X / Y fragments collected".
--                Also drives the light-cone visibility fade for all fragment
--                entities (previously handled by a separate FragmentManager
--                entity running fragment_system.lua).
--
-- All content (c) 2025 DigiPen Institute of Technology Singapore. All rights reserved.
-- *****************************************************************************

Properties = {
    -- Margin (in world units) inward from the bottom-left corner of the screen
    { name = "marginX", type = "float", default = 80.0, label = "Margin from Left"   },
    { name = "marginY", type = "float", default = 40.0, label = "Margin from Bottom" },
}

local HudFragmentCounter = {}

-- ── Fragment visibility (merged from fragment_system.lua) ─────────────────────
local function updateFragmentVisibility(dt)
    World.eachWithScript("components/fragment.lua", function(frag)
        if not frag:isValid() then return end

        local collected = World.getScriptProp(frag.id, "collected") or false
        if collected then
            frag:setSpriteAlpha(0.0)
            return
        end

        local requiresLight = World.getScriptProp(frag.id, "requiresLight")
        if requiresLight == nil then requiresLight = true end

        local targetAlpha = 0.0
        if not requiresLight then
            targetAlpha = 1.0
        else
            World.eachLight(function(torchEnt)
                if targetAlpha >= 1.0 then return end
                if not torchEnt:isValid() then return end
                if not torchEnt:isLightActive() then return end
                local charge = World.getScriptProp(torchEnt.id, "charge")
                if charge ~= nil and charge <= 0.0 then return end

                local rot  = torchEnt.rotation
                local c, s = math.cos(rot), math.sin(rot)
                local ox   = torchEnt:getLightOffsetX()
                local oy   = torchEnt:getLightOffsetY()
                local tx   = torchEnt.position.x + ox * c - oy * s
                local ty   = torchEnt.position.y + ox * s + oy * c

                local fragPos = frag.position
                local dx   = fragPos.x - tx
                local dy   = fragPos.y - ty
                local distSq = dx * dx + dy * dy
                local radius = torchEnt:getLightRadius()

                if distSq <= radius * radius then
                    local dist = math.sqrt(distSq)
                    if dist > 0.0001 then
                        local fdx = dx / dist
                        local fdy = dy / dist
                        local dotProd = math.cos(rot) * fdx + math.sin(rot) * fdy
                        local halfFov = (torchEnt:getLightAngle() * math.pi / 180.0) * 0.5
                        if dotProd >= math.cos(halfFov) then
                            targetAlpha = 1.0
                        end
                    else
                        targetAlpha = 1.0
                    end
                end
            end)
        end

        local fadeSpeed = World.getScriptProp(frag.id, "fadeSpeed") or 5.0
        local curAlpha  = World.getScriptProp(frag.id, "_alpha") or 1.0
        if curAlpha < targetAlpha then
            curAlpha = math.min(curAlpha + fadeSpeed * dt, targetAlpha)
        elseif curAlpha > targetAlpha then
            curAlpha = math.max(curAlpha - fadeSpeed * dt, targetAlpha)
        end
        World.setScriptProp(frag.id, "_alpha", curAlpha)
        if frag:hasSpriteRenderer() then
            frag:setSpriteAlpha(curAlpha)
            frag:setSpriteUnlit(true)
        end
    end)
end

-- ── Main update ───────────────────────────────────────────────────────────────
function HudFragmentCounter:onUpdate(entity, dt)
    local player = World.findByTag("Player")
    if not player or not player:isValid() then return end

    -- Keep HUD at a fixed screen position by tracking player (= camera) position
    -- Bottom-left corner: offset = -(halfW - marginX, -(halfH - marginY))
    local zoom  = Engine.cameraZoom()
    local halfW = Engine.screenWidth()  * 0.5 / zoom
    local halfH = Engine.screenHeight() * 0.5 / zoom
    entity.position = Vec2(player.position.x - halfW + self.marginX,
                           player.position.y - halfH + self.marginY)

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

    -- Drive fragment light-cone visibility (was FragmentManager / fragment_system.lua)
    updateFragmentVisibility(dt)
end

return HudFragmentCounter
