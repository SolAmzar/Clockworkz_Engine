Properties = {
    { name = "state",            type = "string", default = "Patrol", label = "State" },
    { name = "detectionRadius",  type = "float",  default = 300.0,    label = "Detection Radius" },
    { name = "chaseSpeed",       type = "float",  default = 10.0,    label = "Chase Speed" },
    { name = "patrolSpeed",      type = "float",  default = 5.0,     label = "Patrol Speed" },
    { name = "alertDuration",    type = "float",  default = 3.0,      label = "Alert Duration" },
    { name = "lightSensitivity", type = "float",  default = 0.3,      label = "Light Sensitivity" },
    { name = "wanderInterval",   type = "float",  default = 2.0,      label = "Wander Interval" },
    { name = "_stateTimer",      type = "float",  default = 0.0,      label = "" },
    { name = "_wanderTimer",     type = "float",  default = 0.0,      label = "" },
}

function onUpdate(self, entity, dt)
    local player = World.findByTag("Player")
    if not player then return end
    local diff = player.position - entity.position
    local dist = diff:length()

    if self.state == "Patrol" then
        self._wanderTimer = self._wanderTimer + dt
        if dist < self.detectionRadius then
            self.state = "Chase"
        end
        entity.moveSpeed = self.patrolSpeed

    elseif self.state == "Chase" then
        if dist > self.detectionRadius * 1.5 then
            self.state = "Alert"
            self._stateTimer = 0
        end
        local dir = diff:normalized()
        entity.velocity = dir * self.chaseSpeed

    elseif self.state == "Alert" then
        self._stateTimer = self._stateTimer + dt
        entity.velocity = Vec2(0, 0)
        if self._stateTimer >= self.alertDuration then
            self.state = "Patrol"
        end
    end
end

return {
    onUpdate = onUpdate,
}
