Properties = {
    { name = "maxExposureTime", type = "float", default = 10.0,  label = "Max Exposure Time" },
    { name = "isDangerous",     type = "bool",  default = false,  label = "Is Dangerous" },
    { name = "exposureTime",    type = "float", default = 0.0,   label = "Exposure Time" },
    { name = "inLight",         type = "bool",  default = false,  label = "In Light" },
}

-- inLight is set each frame by C++ lighting system via World.setScriptProp(entity.id, "inLight", true/false)
function onUpdate(self, entity, dt)
    if self.inLight then
        self.exposureTime = math.min(self.maxExposureTime, self.exposureTime + dt)
    else
        self.exposureTime = math.max(0.0, self.exposureTime - dt)
    end
    if self.isDangerous and self.exposureTime >= self.maxExposureTime then
        local hp = World.getScriptProp(entity.id, "hp")
        if hp then
            World.setScriptProp(entity.id, "hp", hp - 5 * dt)
        end
    end
end

return {
    onUpdate = onUpdate,
}
