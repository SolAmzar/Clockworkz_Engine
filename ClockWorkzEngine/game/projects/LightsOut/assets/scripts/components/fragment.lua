Properties = {
    { name = "fragmentId",    type = "int",    default = 0,     label = "Fragment ID" },
    { name = "collected",     type = "bool",   default = false, label = "Collected" },
    { name = "requiresLight", type = "bool",   default = true,  label = "Requires Light" },
    { name = "pickupSfx",     type = "string", default = "",   label = "Pickup SFX ID" },
    { name = "bobSpeed",      type = "float",  default = 2.0,   label = "Bob Speed" },
    { name = "bobAmount",     type = "float",  default = 10.0,  label = "Bob Amount" },
    { name = "fadeSpeed",     type = "float",  default = 5.0,   label = "Fade Speed" },
    { name = "_baseY",        type = "float",  default = 0.0,   label = "" },
    { name = "_timer",        type = "float",  default = 0.0,   label = "" },
    { name = "_alpha",        type = "float",  default = 1.0,   label = "" },
    { name = "_initialized",  type = "bool",   default = false, label = "" },
}

function onStart(self, entity)
    self._baseY = entity.position.y
    self._initialized = true
end

function onUpdate(self, entity, dt)
    if self.collected then return end
    self._timer = self._timer + dt
    local bob = math.sin(self._timer * self.bobSpeed) * self.bobAmount
    local pos = entity.position
    entity.position = Vec2(pos.x, self._baseY + bob)
end

return {
    onStart  = onStart,
    onUpdate = onUpdate,
}
