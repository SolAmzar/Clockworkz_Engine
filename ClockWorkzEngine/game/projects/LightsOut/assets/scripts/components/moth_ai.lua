-------------------------------------------------------------------------------
-- moth_ai.lua
-- Light-attracted enemy: stands still, CHASES PLAYER when light hits it.
-- Does NOT take damage from light. Kills player on contact.
-- Attach to moth/specter enemy entities.
-------------------------------------------------------------------------------

Properties = {
    { name = "chaseSpeed",        type = "float", default = 180.0, label = "Chase Speed" },
    -- Internal
    { name = "_state",            type = "string", default = "Idle",  label = "" }, -- Idle, Chase, Attack, Death
    { name = "_alpha",            type = "float",  default = 1.0,     label = "" },
    { name = "_deathSfxPlayed",   type = "bool",   default = false,   label = "" },
    { name = "_colliderDisabled", type = "bool",   default = false,   label = "" },
}

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

local Moth = {}

function Moth:onUpdate(entity, dt)
    if not entity:isValid() then return end

    ---------------------------------------------------------------------------
    -- Death state (from player killing it — not from light)
    ---------------------------------------------------------------------------
    if self._state == "Death" then
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

    ---------------------------------------------------------------------------
    -- Attack state: play attack animation, no movement, then return to Idle
    ---------------------------------------------------------------------------
    if self._state == "Attack" then
        entity.velocity = Vec2(0.0, 0.0)

        if entity:hasSpriteAnimator() then
            -- Ensure we are on the attack clip
            if entity:getCurrentClip() ~= "attack" then
                entity:playAnimation("attack")   -- must exist in your animator
            end
            -- When attack animation finishes, go back to Idle
            if not entity:isAnimationPlaying() then
                self._state = "Idle"
            end
        else
            -- No animator: just fall back to Idle immediately
            self._state = "Idle"
        end

        return
    end

    ---------------------------------------------------------------------------
    -- Find player
    ---------------------------------------------------------------------------
    local player      = World.findByTag("Player")
    local playerFound = player ~= nil and player:isValid()
    local px, py      = 0.0, 0.0

    if playerFound then
        if player:getCurrentClip() == "death" then
            playerFound = false
        else
            px, py = player.position.x, player.position.y
        end
    end

    local ex, ey = entity.position.x, entity.position.y

    ---------------------------------------------------------------------------
    -- Check if any active torch is shining on this enemy
    ---------------------------------------------------------------------------
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

    ---------------------------------------------------------------------------
    -- State: Idle ↔ Chase (light is the trigger, NOT proximity)
    ---------------------------------------------------------------------------
    if self._state == "Idle" then
        if inLight and playerFound then
            self._state = "Chase"
        end

        entity.velocity = Vec2(0.0, 0.0)

        -- IDLE ANIMATION
        if entity:hasSpriteAnimator() then
            if entity:getCurrentClip() ~= "idle" then
                entity:playAnimation("idle")      -- looped idle clip
            end
        end

    elseif self._state == "Chase" then
        -- Stop chasing if light turns off or player died
        if not inLight or not playerFound then
            self._state     = "Idle"
            entity.velocity = Vec2(0.0, 0.0)
            -- Let Idle section swap to idle clip next frame
            return
        end

        local dx, dy = norm(px-ex, py-ey)
        entity.velocity = Vec2(dx * self.chaseSpeed, dy * self.chaseSpeed)

        -- Flip sprite
        local sx = entity.scale.x
        if dx > 0.1 and sx < 0.0 then
            entity.scale = Vec2(-sx, entity.scale.y)
        elseif dx < -0.1 and sx > 0.0 then
            entity.scale = Vec2(-sx, entity.scale.y)
        end

        -- WALK / CHASE ANIMATION
        if entity:hasSpriteAnimator() then
            if entity:getCurrentClip() ~= "walk" then
                entity:playAnimation("walk")      -- looped walk/chase clip
            end
        end
    end
end

function Moth:onCollisionEnter(entity, other)
    if self._state == "Death" then return end
    if not other:isValid() then return end

    if other:hasPlayerController() then
        -- Enter ATTACK state for moth (plays attack animation)
        if entity:hasSpriteAnimator() then
            self._state = "Attack"
            if entity:getCurrentClip() ~= "attack" then
                entity:playAnimation("attack")
            end
        end

        -- Kill player and trigger game over (same as before)
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

return Moth
