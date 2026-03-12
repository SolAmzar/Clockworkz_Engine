-------------------------------------------------------------------------------
-- enemy_ai.lua
-- Normal enemy: patrols, chases player, DIES when light shines on it.
-- Attach to standard shadow-creature enemy entities.
-------------------------------------------------------------------------------

Properties = {
    { name = "state",            type = "string", default = "Patrol", label = "State" },
    { name = "maxHp",            type = "float",  default = 100.0,    label = "Max HP" },
    { name = "detectionRadius",  type = "float",  default = 300.0,    label = "Detection Radius" },
    { name = "chaseSpeed",       type = "float",  default = 150.0,    label = "Chase Speed" },
    { name = "patrolSpeed",      type = "float",  default = 50.0,     label = "Patrol Speed" },
    { name = "alertDuration",    type = "float",  default = 3.0,      label = "Alert Duration" },
    { name = "lightSensitivity", type = "float",  default = 0.3,      label = "Light Sensitivity (speed mult in light)" },
    { name = "wanderInterval",   type = "float",  default = 2.0,      label = "Wander Interval" },
    -- Internal
    { name = "_hp",               type = "float", default = 100.0,    label = "" },
    { name = "_stateTimer",       type = "float", default = 0.0,      label = "" },
    { name = "_wanderTimer",      type = "float", default = 0.0,      label = "" },
    { name = "_wanderDirX",       type = "float", default = 1.0,      label = "" },
    { name = "_wanderDirY",       type = "float", default = 0.0,      label = "" },
    { name = "_targetX",          type = "float", default = 0.0,      label = "" },
    { name = "_targetY",          type = "float", default = 0.0,      label = "" },
    { name = "_alpha",            type = "float", default = 1.0,      label = "" },
    { name = "_deathSfxPlayed",   type = "bool",  default = false,    label = "" },
    { name = "_colliderDisabled", type = "bool",  default = false,    label = "" },
    { name = "_initialized",      type = "bool",  default = false,    label = "" },
}

local function dist2(ax, ay, bx, by)
    local dx, dy = ax - bx, ay - by
    return dx * dx + dy * dy
end
local function norm(dx, dy)
    local len = math.sqrt(dx*dx + dy*dy)
    if len < 0.0001 then return 0.0, 0.0 end
    return dx/len, dy/len
end
local function inCone(px, py, tx, ty, rot, radius, fovDeg)
    local dx, dy = px-tx, py-ty
    if dx*dx + dy*dy > radius*radius then return false end
    local dist = math.sqrt(dx*dx + dy*dy)
    if dist < 0.0001 then return true end
    local dot = math.cos(rot)*(dx/dist) + math.sin(rot)*(dy/dist)
    return dot >= math.cos((fovDeg*math.pi/180.0)*0.5)
end

local Enemy = {}

function Enemy:onStart(entity)
    if not self._initialized then
        self._hp          = self.maxHp
        self._initialized = true
    end
end

function Enemy:onUpdate(entity, dt)
    if not entity:isValid() then return end

    -- Find player
    local player      = World.findByTag("Player")
    local playerFound = player ~= nil and player:isValid()
    local px, py      = 0.0, 0.0
    if playerFound then
        if player:getCurrentClip() == "death" then playerFound = false
        else px, py = player.position.x, player.position.y end
    end

    local ex, ey = entity.position.x, entity.position.y

    -- Light cone detection
    local inLight = false
    World.eachLight(function(t)
        if inLight or not t:isValid() then return end
        if not t:isLightActive() then return end
        if t:hasBattery() and t:getBatteryCharge() <= 0.0 then return end
        local rot = t.rotation
        local c, s = math.cos(rot), math.sin(rot)
        local ox, oy = t:getLightOffsetX(), t:getLightOffsetY()
        local tx = t.position.x + ox*c - oy*s
        local ty = t.position.y + ox*s + oy*c
        if inCone(ex, ey, tx, ty, rot, t:getLightRadius(), t:getLightAngle()) then
            inLight = true
        end
    end)

    -- Light damage + slowdown
    local speedMul = 1.0
    if inLight then
        speedMul   = self.lightSensitivity
        self._hp   = self._hp - 20.0 * dt
        if self._hp <= 0.0 then
            self._hp   = 0.0
            self.state = "Death"
        end
    end

    -- Death
    if self.state == "Death" then
        entity.velocity = Vec2(0.0, 0.0)
        if not self._colliderDisabled then
            self._colliderDisabled = true
            entity:setColliderSolid(false)
        end
        if not self._deathSfxPlayed then
            self._deathSfxPlayed = true
            Audio.playOneShot("enemy_death_" .. math.random(1, 3))
        end
        self._alpha = math.max(0.0, self._alpha - 2.0*dt)
        entity:setSpriteAlpha(self._alpha)
        return
    end

    -- Player died while chasing
    if not playerFound and self.state == "Chase" then
        self.state = "Patrol"
        self._wanderTimer = 0.0
        entity.velocity = Vec2(0.0, 0.0)
    end

    -- State machine
    if self.state == "Patrol" then
        if playerFound and dist2(ex, ey, px, py) < self.detectionRadius*self.detectionRadius then
            self.state = "Chase"
            return
        end
        self._wanderTimer = self._wanderTimer - dt
        if self._wanderTimer <= 0.0 then
            local angle = math.random() * math.pi * 2.0
            self._wanderDirX  = math.cos(angle)
            self._wanderDirY  = math.sin(angle)
            self._wanderTimer = self.wanderInterval
        end
        entity.velocity  = Vec2(self._wanderDirX, self._wanderDirY)
        entity.moveSpeed = self.patrolSpeed * speedMul

    elseif self.state == "Chase" then
        if not playerFound then
            self.state = "Alert"; self._stateTimer = self.alertDuration
            self._targetX = ex; self._targetY = ey; return
        end
        local threshold = self.detectionRadius * 1.5
        if dist2(ex, ey, px, py) > threshold*threshold then
            self.state = "Alert"; self._stateTimer = self.alertDuration
            self._targetX = px; self._targetY = py; return
        end
        local dx, dy = norm(px-ex, py-ey)
        entity.velocity  = Vec2(dx, dy)
        entity.moveSpeed = self.chaseSpeed * speedMul
        self._targetX = px; self._targetY = py

    elseif self.state == "Alert" then
        if playerFound and dist2(ex, ey, px, py) < self.detectionRadius*self.detectionRadius then
            self.state = "Chase"; return
        end
        self._stateTimer = self._stateTimer - dt
        if self._stateTimer <= 0.0 then
            self.state = "Patrol"; self._wanderTimer = 0.0; return
        end
        if dist2(ex, ey, self._targetX, self._targetY) > 20.0*20.0 then
            local dx, dy = norm(self._targetX-ex, self._targetY-ey)
            entity.velocity  = Vec2(dx, dy)
            entity.moveSpeed = self.chaseSpeed * speedMul
        else
            entity.velocity = Vec2(0.0, 0.0)
        end
    end

    -- Animation + flip
    local vx, vy = entity.velocity.x, entity.velocity.y
    if math.abs(vx) > 0.1 or math.abs(vy) > 0.1 then
        entity:playAnimation("walk")
        local sx = entity.scale.x
        if vx > 0.1 and sx < 0.0 then entity.scale = Vec2(-sx, entity.scale.y)
        elseif vx < -0.1 and sx > 0.0 then entity.scale = Vec2(-sx, entity.scale.y) end
    end
end

function Enemy:onCollisionEnter(entity, other)
    if self.state == "Death" then return end
    if not other:isValid() then return end
    if other:hasPlayerController() then
        local isDead = World.getScriptProp(other.id, "_isDead")
        if not isDead then
            World.setScriptProp(other.id, "_isDead", true)
            if other:hasSpriteAnimator() and other:getCurrentClip() ~= "death" then
                other:playAnimation("death")
                other:setColliderSolid(false)
            end
            Engine.triggerGameOver()
        end
    end
end

return Enemy
