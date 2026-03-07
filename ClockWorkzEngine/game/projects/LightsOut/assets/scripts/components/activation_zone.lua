Properties = {
    { name = "active",      type = "bool",   default = true,   label = "Active" },
    { name = "actionType",  type = "string", default = "None", label = "Action Type" },
    { name = "payload",     type = "string", default = "",     label = "Payload" },
    { name = "oneShot",     type = "bool",   default = false,  label = "One Shot" },
    { name = "triggered",   type = "bool",   default = false,  label = "Triggered" },
}

-- onTriggerEnter is called by the C++ collision system when a trigger fires.
function onTriggerEnter(self, entity, other)
    if not self.active then return end
    if self.oneShot and self.triggered then return end
    self.triggered = true
    if self.actionType == "PlayAudio" then
        Audio.play(self.payload)
    end
end

function onUpdate(self, entity, dt) end

return {
    onTriggerEnter = onTriggerEnter,
    onUpdate       = onUpdate,
}
