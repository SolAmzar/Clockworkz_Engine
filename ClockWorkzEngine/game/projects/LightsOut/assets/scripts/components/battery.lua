Properties = {
    { name = "charge",     type = "float", default = 100.0, label = "Charge" },
    { name = "maxCharge",  type = "float", default = 100.0, label = "Max Charge" },
    { name = "drainRate",  type = "float", default = 5.0,   label = "Drain Rate" },
    { name = "chargeRate", type = "float", default = 20.0,  label = "Charge Rate" },
    { name = "isCharging", type = "bool",  default = false,  label = "Is Charging" },
    { name = "_unlimited", type = "bool",  default = false,  label = "Unlimited" },
}

function onUpdate(self, entity, dt)
    if self._unlimited then
        self.charge = self.maxCharge
        return
    end
    if self.isCharging then
        self.charge = math.min(self.maxCharge, self.charge + self.chargeRate * dt)
    else
        self.charge = math.max(0.0, self.charge - self.drainRate * dt)
    end
end

return {
    onUpdate = onUpdate,
}
