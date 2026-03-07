Properties = {
    { name = "levelName", type = "string", default = "", label = "Level Name" },
}

-- Portal just holds data; transition is triggered externally by the C++ collision/trigger system.
function onUpdate(self, entity, dt)
end

return {
    onUpdate = onUpdate,
}
