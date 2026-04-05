-------------------------------------------------------------------------------
-- moth_ai.lua
-- Updated:
--  - Uses normalized movement + moveSpeed
--  - Cancels dash when hitting walls
--  - Stops pushing through obstacles
-------------------------------------------------------------------------------

Properties = {
    { name = "chaseSpeed",          type = "float",  default = 3.30,  label = "Chase Speed" },
    { name = "attackRange",         type = "float",  default = 182.0, label = "Attack Range" },
    { name = "dashSpeed",           type = "float",  default = 10.0,  label = "Dash Speed" },
    { name = "dashDuration",        type = "float",  default = 0.25,  label = "Dash Duration (s)" },

    { name = "idleStateSfxPrefix",  type = "string", default = "",     label = "Idle SFX Prefix" },
    { name = "chaseStateSfxPrefix", type = "string", default = "",     label = "Chase SFX Prefix" },
    { name = "deathStateSfxPrefix", type = "string", default = "",     label = "Death SFX Prefix" },

    { name = "idleStateSfx",        type = "string", default = "",     label = "" },
    { name = "chaseStateSfx",       type = "string", default = "",     label = "" },
    { name = "deathStateSfx",       type = "string", default = "",     label = "" },

    { name = "idleStateSfxVariations",  type = "int", default = 1,     label = "Idle SFX Variations" },
    { name = "chaseStateSfxVariations", type = "int", default = 1,     label = "Chase SFX Variations" },
    { name = "deathStateSfxVariations", type = "int", default = 1,     label = "Death SFX Variations" },

    { name = "_state",              type = "string", default = "Idle", label = "" },
    { name = "_alpha",              type = "float",  default = 1.0,    label = "" },
    { name = "_deathSfxPlayed",     type = "bool",   default = false,  label = "" },
    { name = "_colliderDisabled",   type = "bool",   default = false,  label = "" },
    { name = "_dashTimer",          type = "float",  default = 0.0,    label = "" },
    { name = "_dashDirX",           type = "float",  default = 0.0,    label = "" },
    { name = "_dashDirY",           type = "float",  default = 0.0,    label = "" },
    { name = "_initialized",        type = "bool",   default = false,  label = "" },
}

local function norm(dx, dy)
    local len = math.sqrt(dx * dx + dy * dy)
    if len < 0.0001 then
        return 0.0, 0.0
    end
    return dx / len, dy / len
end

local function inCone(px, py, tx, ty, rot, radius, fovDeg)
    local dx, dy = px - tx, py - ty
    if dx * dx + dy * dy > radius * radius then
        return false
    end

    local dist = math.sqrt(dx * dx + dy * dy)
    if dist < 0.0001 then
        return true
    end

    local dot = math.cos(rot) * (dx / dist) + math.sin(rot) * (dy / dist)
    return dot >= math.cos((fovDeg * math.pi / 180.0) * 0.5)
end

local function stateSfxId(self, state)
    local prefix = ""
    local legacy = ""
    local variations = 1

    if state == "Idle" then
        prefix = self.idleStateSfxPrefix or ""
        legacy = self.idleStateSfx or ""
        variations = tonumber(self.idleStateSfxVariations) or 1
    elseif state == "Chase" then
        prefix = self.chaseStateSfxPrefix or ""
        legacy = self.chaseStateSfx or ""
        variations = tonumber(self.chaseStateSfxVariations) or 1
    elseif state == "Death" then
        prefix = self.deathStateSfxPrefix or ""
        legacy = self.deathStateSfx or ""
        variations = tonumber(self.deathStateSfxVariations) or 1
    end

    if prefix ~= "" then
        variations = math.max(variations, 1)
        if variations <= 1 then
            return prefix
        end
        return prefix .. tostring(math.random(1, variations))
    end

    return legacy
end

local function playStateSfx(self, state)
    local sfx = stateSfxId(self, state)
    if sfx ~= nil and sfx ~= "" then
        Audio.playOneShot(sfx)
    end
end

local function setState(self, entity, newState)
    if self._state == newState then
        return false
    end

    self._state = newState
    playStateSfx(self, newState)
    return true
end

local function killPlayer(player)
    local isDead = World.getScriptProp(player.id, "_isDead")
    local invincible = World.getScriptProp(player.id, "_invincible")
    if isDead or invincible then
        return
    end

    World.setScriptProp(player.id, "_isDead", true)
    if player:hasSpriteAnimator() and player:getCurrentClip() ~= "death" then
        player:playAnimation("death")
        player:setColliderSolid(false)
    end
    Engine.triggerGameOver()
end

local Moth = {}

function Moth:onStart(entity)
    if not self._initialized then
        self._initialized = true
        self._dashTimer = 0.0
        self._dashDirX = 0.0
        self._dashDirY = 0.0
    end
end

function Moth:onUpdate(entity, dt)
    if not entity:isValid() then
        return
    end

    -- DEATH
    if self._state == "Death" then
        entity.velocity = Vec2(0.0, 0.0)
        entity.moveSpeed = 0.0

        if not self._colliderDisabled then
            self._colliderDisabled = true
            entity:setColliderSolid(false)
        end

        if not self._deathSfxPlayed then
            self._deathSfxPlayed = true
            if (self.deathStateSfxPrefix == nil or self.deathStateSfxPrefix == "") and
               (self.deathStateSfx == nil or self.deathStateSfx == "") then
                Audio.playOneShot("enemy_death_" .. math.random(1, 3))
            end
        end

        self._alpha = math.max(0.0, self._alpha - 2.0 * dt)
        entity:setSpriteAlpha(self._alpha)
        return
    end

    -- ATTACK: stop, play attack animation, then dash
    if self._state == "Attack" then
        entity.velocity = Vec2(0.0, 0.0)
        entity.moveSpeed = 0.0

        if entity:hasSpriteAnimator() then
            if entity:getCurrentClip() ~= "attack" then
                entity:playAnimation("attack")
            end

            if not entity:isAnimationPlaying() and entity:getCurrentClip() == "attack" then
                self._dashTimer = self.dashDuration
                self._state = "Dash"
            end
        else
            self._dashTimer = self.dashDuration
            self._state = "Dash"
        end

        return
    end

    -- DASH: normalized direction + moveSpeed
    if self._state == "Dash" then
        self._dashTimer = self._dashTimer - dt

        if self._dashTimer > 0.0 then
            entity.velocity = Vec2(self._dashDirX, self._dashDirY)
            entity.moveSpeed = self.dashSpeed
        else
            entity.velocity = Vec2(0.0, 0.0)
            entity.moveSpeed = 0.0
            self._state = "Chase"
        end

        return
    end

    -- Find player
    local player = World.findByTag("Player")
    local playerFound = player ~= nil and player:isValid()
    local px, py = 0.0, 0.0

    if playerFound then
        if player:getCurrentClip() == "death" then
            playerFound = false
        else
            px, py = player.position.x, player.position.y
        end
    end

    local ex, ey = entity.position.x, entity.position.y

    -- Check light cones
    local inLight = false
    World.eachLight(function(t)
        if inLight or not t:isValid() then return end
        if not t:isLightActive() then return end
        if t:hasBattery() and t:getBatteryCharge() <= 0.0 then return end

        local rot = t.rotation
        local c, s = math.cos(rot), math.sin(rot)
        local ox, oy = t:getLightOffsetX(), t:getLightOffsetY()
        local tx = t.position.x + ox * c - oy * s
        local ty = t.position.y + ox * s + oy * c

        if inCone(ex, ey, tx, ty, rot, t:getLightRadius(), t:getLightAngle()) then
            inLight = true
        end
    end)

    -- IDLE
    if self._state == "Idle" then
        entity.velocity = Vec2(0.0, 0.0)
        entity.moveSpeed = 0.0

        if entity:hasSpriteAnimator() and entity:getCurrentClip() ~= "idle" then
            entity:playAnimation("idle")
        end

        if inLight and playerFound then
            setState(self, entity, "Chase")
        end

    -- CHASE
    elseif self._state == "Chase" then
        if not inLight or not playerFound then
            self._state = "Idle"
            entity.velocity = Vec2(0.0, 0.0)
            entity.moveSpeed = 0.0
            return
        end

        local dxp = px - ex
        local dyp = py - ey
        local dist = math.sqrt(dxp * dxp + dyp * dyp)

        if dist <= self.attackRange then
            self._dashDirX = dxp / (dist > 0.0001 and dist or 1.0)
            self._dashDirY = dyp / (dist > 0.0001 and dist or 1.0)
            self._state = "Attack"
            entity.velocity = Vec2(0.0, 0.0)
            entity.moveSpeed = 0.0

            if entity:hasSpriteAnimator() then
                entity:playAnimation("attack")
            end
            return
        end

        local dx, dy = norm(dxp, dyp)
        entity.velocity = Vec2(dx, dy)
        entity.moveSpeed = self.chaseSpeed

        local sx = entity.scale.x
        if dx > 0.1 and sx < 0.0 then
            entity.scale = Vec2(-sx, entity.scale.y)
        elseif dx < -0.1 and sx > 0.0 then
            entity.scale = Vec2(-sx, entity.scale.y)
        end

        if entity:hasSpriteAnimator() and entity:getCurrentClip() ~= "walk" then
            entity:playAnimation("walk")
        end
    else
        entity.velocity = Vec2(0.0, 0.0)
        entity.moveSpeed = 0.0
    end
end

function Moth:onCollisionEnter(entity, other)
    if self._state == "Death" then return end
    if not other:isValid() then return end

    if other:hasPlayerController() then
        if self._state ~= "Attack" and self._state ~= "Dash" then
            local ex, ey = entity.position.x, entity.position.y
            local px, py = other.position.x, other.position.y
            self._dashDirX, self._dashDirY = norm(px - ex, py - ey)
            self._state = "Attack"
            entity.velocity = Vec2(0.0, 0.0)
            entity.moveSpeed = 0.0
            if entity:hasSpriteAnimator() then
                entity:playAnimation("attack")
            end
        end

        killPlayer(other)
        return
    end

    -- Hit wall or obstacle: stop pushing.
    entity.velocity = Vec2(0.0, 0.0)
    entity.moveSpeed = 0.0

    if self._state == "Dash" then
        self._dashTimer = 0.0
        self._state = "Chase"
    elseif self._state == "Chase" then
        self._state = "Idle"
    elseif self._state == "Attack" then
        self._state = "Idle"
    end
end

return Moth