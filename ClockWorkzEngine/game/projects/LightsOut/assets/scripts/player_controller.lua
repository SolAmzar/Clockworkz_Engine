-- *****************************************************************************
-- file           player_controller.lua
-- project        Lights Out
-- author(s)      [Syafiq] 100%
-- brief          Unified player controller — movement, rotation, animation,
--                torch, battery, torch bobbing, and footstep audio.
--
-- All content (c) 2025 DigiPen Institute of Technology Singapore. All rights reserved.
-- *****************************************************************************

-- Inspector-visible properties
Properties = {
    { name = "walkSpeed",         type = "float",  default = 10.0,  label = "Walk Speed"          },
    { name = "sprintMultiplier",  type = "float",  default = 2.0,    label = "Sprint Multiplier"   },
    { name = "baseWalkSpeed",     type = "float",  default = 10.0,  label = "Base Walk Speed"     },
    { name = "bobAmount",         type = "float",  default = 8.0,    label = "Torch Bob Amount"    },
    { name = "bobSpeed",          type = "float",  default = 6.0,    label = "Torch Bob Speed"     },
    { name = "footstepInterval",  type = "float",  default = 0.6,    label = "Footstep Interval s" },
}

local PlayerController = {}

-- Runtime state (not inspector properties)
PlayerController._isDead          = false   -- guard: prevents double game-over trigger
PlayerController.walkSpeed        = 10.0
PlayerController.sprintMultiplier = 2.0
PlayerController.baseWalkSpeed    = 10.0
PlayerController.bobAmount        = 8.0
PlayerController.bobSpeed         = 6.0
PlayerController.footstepInterval = 0.6

-- Internal
PlayerController._bobTimer       = 0.0
PlayerController._footstepTimer  = 0.0
PlayerController._baseTorchOffX  = nil   -- captured once on first torch use
PlayerController._baseTorchOffY  = nil
PlayerController._torchActive    = false

function PlayerController:onStart(entity)
    entity.moveSpeed = self.walkSpeed
end

function PlayerController:onUpdate(entity, dt)
    if not entity:isValid() then return end

    local input        = Engine.input()
    local isSprinting  = input:isKeyDown(Keys.LEFT_SHIFT)
                      or input:isKeyDown(Keys.RIGHT_SHIFT)

    -- ----------------------------------------------------------------
    -- Mouse-facing rotation
    -- ----------------------------------------------------------------
    local mouseWorld = Engine.mouseWorldPos()
    local pos        = entity.position
    local dx         = mouseWorld.x - pos.x
    local dy         = mouseWorld.y - pos.y
    local dist       = math.sqrt(dx * dx + dy * dy)
    if dist > 10.0 then
        entity.rotation = math.atan(dy, dx)
    end

    -- ----------------------------------------------------------------
    -- Movement (rotation-based forward / backward)
    -- ----------------------------------------------------------------
    local forwardInput = 0
    if input:isKeyDown(Keys.W) then forwardInput = forwardInput + 1 end
    if input:isKeyDown(Keys.S) then forwardInput = forwardInput - 1 end

    local currentState = "idle"
    if forwardInput ~= 0 then
        local speedMul = isSprinting and self.sprintMultiplier or 1.0
        local speed    = self.walkSpeed * speedMul

        local angle = entity.rotation
        local fx = math.cos(angle) * forwardInput
        local fy = math.sin(angle) * forwardInput
        local len = math.sqrt(fx * fx + fy * fy)
        if len > 0 then
            fx = fx / len
            fy = fy / len
        end

        entity.velocity = Vec2(fx * speed, fy * speed)
        currentState = isSprinting and "run" or "walk"

        -- Scale animation speed proportionally to walk speed
        if entity:hasSpriteAnimator() then
            local playbackSpeed = (self.baseWalkSpeed > 0)
                and (speed / self.baseWalkSpeed)
                or 1.0
            entity:setAnimationPlaybackSpeed(playbackSpeed)
        end

        -- Footstep audio
        self._footstepTimer = self._footstepTimer - dt
        if self._footstepTimer <= 0.0 then
            local interval = self.footstepInterval
            if isSprinting then interval = interval * 0.6 end
            self._footstepTimer = interval
            local variation = math.random(1, 4)
            Audio.playOneShot("fs_concrete_" .. variation)
        end
    else
        entity.velocity = Vec2(0, 0)
        self._footstepTimer = 0.05
        if entity:hasSpriteAnimator() then
            entity:setAnimationPlaybackSpeed(1.0)
        end
    end

    -- ----------------------------------------------------------------
    -- Torch toggle (F key)
    -- ----------------------------------------------------------------
    if entity:hasLight() then
        -- Capture base torch offset once
        if self._baseTorchOffX == nil then
            self._baseTorchOffX = entity:getLightOffsetX()
            self._baseTorchOffY = entity:getLightOffsetY()
            self._torchActive   = entity:isLightActive()
        end

        if input:wasKeyPressed(Keys.F) then
            local charge = entity:hasBattery() and entity:getBatteryCharge() or 1.0
            if charge > 0.0 then
                self._torchActive = not self._torchActive
                entity:setLightActive(self._torchActive)
                if self._torchActive then
                    Audio.playOneShot("flashlight_on")
                else
                    Audio.playOneShot("flashlight_off")
                end
            elseif not self._torchActive then
                Audio.playOneShot("flashlight_off")
            end
        end

        -- Battery: set isCharging flag so BatterySystem applies drain/charge math.
        -- Force torch off if battery hits zero.
        if entity:hasBattery() then
            entity:setBatteryCharging(not self._torchActive)
            if self._torchActive and entity:getBatteryCharge() <= 0.0 then
                self._torchActive = false
                entity:setLightActive(false)
                Audio.playOneShot("flashlight_off")
            end
        end

        -- Torch bobbing
        if self._torchActive then
            local stateOffX = 0.0
            if currentState == "run" then
                stateOffX = 10.0
                self._bobTimer = self._bobTimer + self.bobSpeed * 1.5 * dt
            elseif currentState == "walk" then
                stateOffX = 5.0
                self._bobTimer = self._bobTimer + self.bobSpeed * dt
            else
                self._bobTimer = 0.0
            end
            local bobOffset = math.sin(self._bobTimer) * self.bobAmount
            entity:setLightOffsetX(self._baseTorchOffX + stateOffX)
            entity:setLightOffsetY(self._baseTorchOffY + bobOffset)
        end

        -- Append _torch suffix to animation state
        if self._torchActive then
            currentState = currentState .. "_torch"
        end
    end

    -- ----------------------------------------------------------------
    -- Animation
    -- ----------------------------------------------------------------
    if entity:hasSpriteAnimator() then
        if entity:getCurrentClip() ~= currentState then
            entity:playAnimation(currentState)
        end
    end
end

function PlayerController:onCollisionEnter(entity, other)
    if other:getTagValue() == "Enemy" then
        if not self._isDead then
            self._isDead = true
            if entity:hasSpriteAnimator() and entity:getCurrentClip() ~= "death" then
                entity:playAnimation("death")
                entity:setColliderSolid(false)
            end
            Engine.triggerGameOver()
        end
    end
end

return PlayerController
