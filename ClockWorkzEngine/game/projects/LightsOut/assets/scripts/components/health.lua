Properties = {
    { name = "hp",           type = "float", default = 10.0,  label = "HP" },
    { name = "maxHp",        type = "float", default = 10.0,  label = "Max HP" },
    { name = "regenRate",    type = "float", default = 0.0,   label = "Regen Rate" },
    { name = "invulnerable", type = "bool",  default = false,  label = "Invulnerable" },
    { name = "isDead",       type = "bool",  default = false,  label = "Is Dead" },
}

function onUpdate(self, entity, dt)
    if self.isDead then return end
    if self.regenRate > 0 then
        self.hp = math.min(self.maxHp, self.hp + self.regenRate * dt)
    end
    if self.hp <= 0 then
        self.isDead = true
        self.hp = 0
    end
end

return {
    onUpdate = onUpdate,
}
