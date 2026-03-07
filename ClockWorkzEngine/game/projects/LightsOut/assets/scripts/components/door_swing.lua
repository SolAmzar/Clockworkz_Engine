Properties = {
    { name = "isOpen",          type = "bool",  default = false, label = "Is Open" },
    { name = "swingAngle",      type = "float", default = 90.0,  label = "Swing Angle" },
    { name = "clockwise",       type = "bool",  default = true,  label = "Clockwise" },
    { name = "speed",           type = "float", default = 90.0,  label = "Speed (deg/s)" },
    { name = "initialRotation", type = "float", default = 0.0,   label = "Initial Rotation" },
    { name = "_initialized",    type = "bool",  default = false,  label = "" },
}

function onStart(self, entity)
    if not self._initialized then
        self.initialRotation = entity.rotation
        self._initialized = true
    end
end

function onUpdate(self, entity, dt)
    local targetRot = self.isOpen
        and self.initialRotation + (self.clockwise and -self.swingAngle or self.swingAngle)
        or  self.initialRotation
    local dir = targetRot - entity.rotation
    local step = self.speed * dt
    if math.abs(dir) <= step then
        entity.rotation = targetRot
    else
        entity.rotation = entity.rotation + (dir > 0 and step or -step)
    end
end

return {
    onStart  = onStart,
    onUpdate = onUpdate,
}
