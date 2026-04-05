--*****************************************************************************
-- file           fragment_goal_trigger.lua
-- project        Lights Out
-- brief          Triggers an action once all required fragments are collected.
--                Designed for level scripting from the editor.
--*****************************************************************************

Properties = {
    { name = "active", type = "bool", default = true, label = "Active" },
    { name = "levelGroupId", type = "string", default = "", label = "Level Group ID" },
    { name = "useAllGroupsWhenNoLevelId", type = "bool", default = true, label = "Use All Groups If No Level ID" },
    { name = "requiredFragments", type = "int", default = 0, label = "Required Fragments (0=Auto)" },
    { name = "targetTag", type = "string", default = "", label = "Target Tag (empty=self)" },
    { name = "actionType", type = "string", default = "HideTarget", label = "Action Type" },
    { name = "moveOffsetX", type = "float", default = 0.0, label = "Move Offset X" },
    { name = "moveOffsetY", type = "float", default = 0.0, label = "Move Offset Y" },
    { name = "oneShot", type = "bool", default = true, label = "One Shot" },
    { name = "triggered", type = "bool", default = false, label = "Triggered" },
}

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

local function getProgressFromGroup(groupId)
    if groupId == nil or groupId == "" then
        return nil, nil
    end
    if _G.FragmentProgress == nil then
        return nil, nil
    end
    return _G.FragmentProgress.getCollectedCount(groupId),
           _G.FragmentProgress.getMaxCount(groupId)
end

local function getProgressFromAllGroups()
    if _G.FragmentProgress == nil or _G.FragmentProgress.groups == nil then
        return nil, nil
    end

    local totalCollected = 0
    local totalMax = 0

    for _, group in pairs(_G.FragmentProgress.groups) do
        if group then
            local count = 0
            if group.collectedIds then
                for _, collected in pairs(group.collectedIds) do
                    if collected then
                        count = count + 1
                    end
                end
            end
            totalCollected = totalCollected + count
            totalMax = totalMax + (group.maxFragments or 0)
        end
    end

    return totalCollected, totalMax
end

local function getProgressFromPlayer()
    local player = findPlayer()
    if player == nil or not player:isValid() then
        return 0, 0
    end

    local collected = World.getScriptProp(player.id, "collectedCount") or 0
    local required = World.getScriptProp(player.id, "requiredFragments") or 0
    return collected, required
end

local function resolveProgress(self)
    local collected, required = getProgressFromGroup(self.levelGroupId)
    if collected == nil and (self.useAllGroupsWhenNoLevelId == true) then
        collected, required = getProgressFromAllGroups()
    end
    if collected == nil then
        collected, required = getProgressFromPlayer()
    end

    if (self.requiredFragments or 0) > 0 then
        required = self.requiredFragments
    end

    return collected or 0, required or 0
end

local function findTarget(self, entity)
    if self.targetTag == nil or self.targetTag == "" then
        return entity
    end
    local target = World.findByTag(self.targetTag)
    if target and target:isValid() then
        return target
    end
    return nil
end

local function hideTarget(target)
    if target:hasSpriteRenderer() then
        target:setSpriteAlpha(0.0)
    end
    if target:hasTextRenderer() then
        target:setTextVisible(false)
    end
    if target:hasCollider() then
        target:setColliderSolid(false)
    end
    if target:hasParticleEmitter() then
        target:setParticleEmitterActive(false)
    end
end

local function moveTarget(target, dx, dy)
    local pos = target.position
    target.position = Vec2(pos.x + dx, pos.y + dy)
end

local function applyAction(self, entity)
    local target = findTarget(self, entity)
    if target == nil then
        return
    end

    if self.actionType == "MoveTarget" then
        moveTarget(target, self.moveOffsetX or 0.0, self.moveOffsetY or 0.0)
    else
        -- Default: HideTarget
        hideTarget(target)
    end
end

function onStart(self, entity)
    -- Runtime reset so editor play sessions start cleanly.
    self.triggered = false
end

function onUpdate(self, entity, dt)
    if not self.active then
        return
    end

    local collected, required = resolveProgress(self)
    if required <= 0 then
        return
    end

    if collected >= required then
        if self.triggered then
            return
        end
        applyAction(self, entity)
        self.triggered = true
        return
    end

    -- Allow re-trigger only for non-one-shot setups after progress drops below threshold.
    if not self.oneShot then
        self.triggered = false
    end
end

return {
    onStart = onStart,
    onUpdate = onUpdate,
}
