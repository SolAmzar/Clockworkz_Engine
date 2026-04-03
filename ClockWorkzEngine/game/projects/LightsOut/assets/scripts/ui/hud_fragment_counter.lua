local function getFragmentProgress()
    if _G.FragmentProgress then
        return _G.FragmentProgress
    end

    local FragmentProgress = {
        currentGroupId = "",
        groups = {}
    }

    function FragmentProgress.ensureGroup(groupId, maxFragments)
        if not groupId or groupId == "" then
            return nil
        end

        local group = FragmentProgress.groups[groupId]
        if not group then
            group = {
                collectedIds = {},
                maxFragments = maxFragments or 0
            }
            FragmentProgress.groups[groupId] = group
        elseif maxFragments and maxFragments > group.maxFragments then
            group.maxFragments = maxFragments
        end

        return group
    end

    function FragmentProgress.setCurrentGroup(groupId, maxFragments)
        if groupId and groupId ~= "" then
            FragmentProgress.currentGroupId = groupId
            FragmentProgress.ensureGroup(groupId, maxFragments)
        else
            FragmentProgress.currentGroupId = ""
        end
    end

    function FragmentProgress.getCurrentGroupId()
        return FragmentProgress.currentGroupId
    end

    function FragmentProgress.markCollected(groupId, fragmentId)
        if not groupId or groupId == "" or not fragmentId or fragmentId == 0 then
            return false
        end

        local group = FragmentProgress.ensureGroup(groupId)
        if not group then
            return false
        end

        if group.collectedIds[fragmentId] then
            return false
        end

        group.collectedIds[fragmentId] = true
        return true
    end

    function FragmentProgress.isCollected(groupId, fragmentId)
        if not groupId or groupId == "" or not fragmentId or fragmentId == 0 then
            return false
        end

        local group = FragmentProgress.groups[groupId]
        return group ~= nil and group.collectedIds[fragmentId] == true
    end

    function FragmentProgress.getCollectedCount(groupId)
        local group = FragmentProgress.groups[groupId]
        if not group then
            return 0
        end

        local count = 0
        for _, collected in pairs(group.collectedIds) do
            if collected then
                count = count + 1
            end
        end
        return count
    end

    function FragmentProgress.getMaxCount(groupId)
        local group = FragmentProgress.groups[groupId]
        if not group then
            return 0
        end
        return group.maxFragments or 0
    end

    function FragmentProgress.resetGroup(groupId)
        if groupId and groupId ~= "" then
            FragmentProgress.groups[groupId] = nil
            if FragmentProgress.currentGroupId == groupId then
                FragmentProgress.currentGroupId = ""
            end
        end
    end

    function FragmentProgress.resetAll()
        FragmentProgress.currentGroupId = ""
        FragmentProgress.groups = {}
    end

    _G.FragmentProgress = FragmentProgress
    return FragmentProgress
end

local FragmentProgress = getFragmentProgress()

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
    { name = "levelGroupId", type = "string", default = "", label = "Level Group ID" },
    { name = "groupMaxFragments", type = "int", default = 0, label = "Group Maximum" },
}

local HudFragmentCounter = {}

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

-- ── Fragment visibility (merged from fragment_system.lua) ─────────────────────
local function updateFragmentVisibility(dt, self)
    World.eachWithScript("components/fragment.lua", function(frag)
        if not frag:isValid() then return end

        local collected = World.getScriptProp(frag.id, "collected") or false
        
        if self.levelGroupId ~= "" then
            local fragId = World.getScriptProp(frag.id, "fragmentId")
            if FragmentProgress.isCollected(self.levelGroupId, fragId) then
                collected = true
                World.setScriptProp(frag.id, "collected", true)
            end
        end

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
    local player = findPlayer()
    if not player or not player:isValid() then return end

    -- Keep HUD at a fixed screen position by tracking player (= camera) position
    -- Bottom-left corner: offset = -(halfW - marginX, -(halfH - marginY))
    local zoom  = Engine.cameraZoom()
    local halfW = Engine.screenWidth()  * 0.5 / zoom
    local halfH = Engine.screenHeight() * 0.5 / zoom
    entity.position = Vec2(player.position.x - halfW + self.marginX,
                           player.position.y - halfH + self.marginY)

    FragmentProgress.setCurrentGroup(self.levelGroupId, self.groupMaxFragments)

    -- Count total fragments and how many have been collected
    local total     = 0
    local collected = 0
    
    if self.levelGroupId ~= "" then
        total = FragmentProgress.getMaxCount(self.levelGroupId)
        collected = FragmentProgress.getCollectedCount(self.levelGroupId)
    else
        World.eachByTag("Fragment", function(frag)
            total = total + 1
            if World.getScriptProp(frag.id, "collected") then
                collected = collected + 1
            end
        end)
    end

    local label = collected .. " / " .. total .. " fragments collected"
    if entity:hasTextRenderer() and entity:getText() ~= label then
        entity:setText(label)
    end

    -- Drive fragment light-cone visibility (was FragmentManager / fragment_system.lua)
    updateFragmentVisibility(dt, self)
end

return HudFragmentCounter
