Properties = {
    { name = "requiredFragments", type = "int",    default = 1,            label = "Required Fragments" },
    { name = "isOpen",            type = "bool",   default = false,         label = "Is Open" },
    { name = "openTexture",       type = "string", default = "door_open",   label = "Open Texture" },
    { name = "closedTexture",     type = "string", default = "door_close",  label = "Closed Texture" },
}

function onStart(self, entity) end

function onUpdate(self, entity, dt)
    local collector = World.findByTag("Player")
    if not collector then return end
    local count = World.getScriptProp(collector.id, "collectedCount") or 0
    local wasOpen = self.isOpen
    self.isOpen = count >= self.requiredFragments
    if self.isOpen ~= wasOpen then
        entity:setSpriteTextureId(self.isOpen and self.openTexture or self.closedTexture)
        entity:setColliderSolid(not self.isOpen)
    end
end

return {
    onStart  = onStart,
    onUpdate = onUpdate,
}
