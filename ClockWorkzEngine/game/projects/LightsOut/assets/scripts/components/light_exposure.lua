Properties = {
    { name = "maxExposureTime", type = "float", default = 10.0,  label = "Max Exposure Time" },
    { name = "isDangerous",     type = "bool",  default = false,  label = "Is Dangerous" },
    { name = "exposureTime",    type = "float", default = 0.0,   label = "Exposure Time" },
    { name = "inLight",         type = "bool",  default = false,  label = "In Light" },
}

-- Damage per second applied when the entity is fully exposed to light
local LIGHT_DAMAGE_RATE = 20.0

function onUpdate(self, entity, dt)
    -- Detect whether this entity is inside any active torch cone
    self.inLight = false
    World.eachLight(function(torch)
        if self.inLight then return end                          -- already lit, skip
        if not torch:isLightActive() then return end
        if torch:hasBattery() and torch:getBatteryCharge() <= 0.0 then return end

        -- Compute torch world position (entity transform + rotated light offset)
        local angle = torch.rotation
        local c     = math.cos(angle)
        local s     = math.sin(angle)
        local offX  = torch:getLightOffsetX()
        local offY  = torch:getLightOffsetY()
        local torchPos = Vec2(torch.position.x + offX * c - offY * s,
                              torch.position.y + offX * s + offY * c)

        local toEntity = Vec2(entity.position.x - torchPos.x,
                              entity.position.y - torchPos.y)
        local dist = toEntity:length()

        if dist > torch:getLightRadius() then return end        -- out of range

        -- Angle check: dot product of torch direction vs direction to entity
        local torchDir = Vec2(c, s)
        if dist > 0.0001 then
            local dir      = toEntity:normalized()
            local dot      = torchDir.x * dir.x + torchDir.y * dir.y
            local halfFov  = math.rad(torch:getLightAngle()) * 0.5
            if dot >= math.cos(halfFov) then
                self.inLight = true
            end
        else
            self.inLight = true
        end
    end)

    -- Accumulate / deplete exposure time
    if self.inLight then
        self.exposureTime = math.min(self.maxExposureTime, self.exposureTime + dt)
    else
        self.exposureTime = math.max(0.0, self.exposureTime - dt)
    end

    -- Deal damage when fully exposed
    if self.exposureTime >= self.maxExposureTime then
        local hp = World.getScriptProp(entity.id, "hp")
        if hp then
            World.setScriptProp(entity.id, "hp", math.max(0.0, hp - LIGHT_DAMAGE_RATE * dt))
        end
    end
end

return {
    onUpdate = onUpdate,
}
