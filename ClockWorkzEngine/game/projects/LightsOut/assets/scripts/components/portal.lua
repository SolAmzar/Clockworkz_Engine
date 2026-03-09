Properties = {
    { name = "levelName", type = "string", default = "", label = "Level Name" },
}

function onTriggerEnter(self, entity, other)
    if self.levelName == "" then return end
    if other:getTagValue() == "Player" then
        World.loadLevel(self.levelName)
    end
end

return {
    onTriggerEnter = onTriggerEnter,
}
