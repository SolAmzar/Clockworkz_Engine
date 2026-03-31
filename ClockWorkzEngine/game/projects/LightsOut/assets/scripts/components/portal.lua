Properties = {
    { name = "levelName", type = "string",  default = "",  label = "Level Name"  },
    { name = "spawnX",    type = "float",   default = 0.0, label = "Spawn X"     },
    { name = "spawnY",    type = "float",   default = 0.0, label = "Spawn Y"     },
}

function onTriggerEnter(self, entity, other)
    if self.levelName == "" then return end
    if other:getTagValue() == "Player" then
        -- Store target spawn position in the persistent global Lua state.
        -- _G persists across scene loads because the Lua VM is never restarted.
        _G.nextSpawnPos = { x = self.spawnX, y = self.spawnY }
        World.loadLevel(self.levelName)
    end
end

return {
    onTriggerEnter = onTriggerEnter,
}
