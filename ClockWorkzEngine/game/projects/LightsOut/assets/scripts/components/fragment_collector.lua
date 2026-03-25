Properties = {
    { name = "requiredFragments", type = "int",   default = 0,    label = "Required Fragments" },
    { name = "collectedCount",    type = "int",   default = 0,    label = "Collected Count" },
    { name = "pickupRadius",      type = "float", default = 50.0, label = "Pickup Radius" },
}

function onUpdate(self, entity, dt)
    World.eachByTag("Fragment", function(frag)
        if World.getScriptProp(frag.id, "collected") then return end
        local diff = frag.position - entity.position
        local dist = diff:length()
        if dist <= self.pickupRadius then
            World.setScriptProp(frag.id, "collected", true)
            self.collectedCount = self.collectedCount + 1

            local fragId = World.getScriptProp(frag.id, "fragmentId")
            local groupId = _G.GameProgress and _G.GameProgress.CurrentLevelGroup
            if groupId and groupId ~= "" and fragId and fragId ~= 0 then
                _G.GameProgress.Groups[groupId].collectedIds[fragId] = true
            end

            local pickupSfx = World.getScriptProp(frag.id, "pickupSfx") or ""
            if pickupSfx ~= "" then
                Audio.playOneShot(pickupSfx)
            end
        end
    end)
end

return {
    onUpdate = onUpdate,
}
