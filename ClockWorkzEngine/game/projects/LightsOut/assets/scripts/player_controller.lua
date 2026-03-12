-- *****************************************************************************
-- file           player_controller.lua
-- project        Lights Out
-- author(s)      [Syafiq] 100%
-- brief          Unified player controller � movement, rotation, animation,
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
PlayerController._manualTorchOff = false
PlayerController._batteryDying   = false
PlayerController._flickerTimer   = 0.0
PlayerController._flickerStep    = 0.0
PlayerController._isDead         = false
PlayerController._deathTimer     = 0.0
local DEATH_RESPAWN_DELAY        = 2.8   -- seconds before level reloads
local BATTERY_DEATH_FLICKER_TIME = 0.9
local BATTERY_DEATH_FLICKER_MIN_STEP = 0.015
local BATTERY_DEATH_FLICKER_MAX_STEP = 0.08

function PlayerController:onStart(entity)
    entity.moveSpeed = self.walkSpeed
end

function PlayerController:onUpdate(entity, dt)
    if not entity:isValid() then return end
    if self._isDead then
        self._deathTimer = self._deathTimer - dt
        if self._deathTimer <= 0 then
            World.loadLevel("level_save")
        end
        return
    end

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
    -- Movement (rotation-based WASD)
    -- ----------------------------------------------------------------
    local forwardInput = 0
    local strafeInput  = 0
    if input:isKeyDown(Keys.W) then forwardInput = forwardInput + 1 end
    if input:isKeyDown(Keys.S) then forwardInput = forwardInput - 1 end
    if input:isKeyDown(Keys.D) then strafeInput = strafeInput - 1 end
    if input:isKeyDown(Keys.A) then strafeInput = strafeInput + 1 end

    local currentState = "idle"
    if forwardInput ~= 0 or strafeInput ~= 0 then
        local speedMul = isSprinting and self.sprintMultiplier or 1.0
        local speed    = self.walkSpeed * speedMul

        local angle = entity.rotation
        local forwardX = math.cos(angle)
        local forwardY = math.sin(angle)
        local rightX = -forwardY
        local rightY = forwardX

        local moveX = (forwardX * forwardInput) + (rightX * strafeInput)
        local moveY = (forwardY * forwardInput) + (rightY * strafeInput)
        local len = math.sqrt(moveX * moveX + moveY * moveY)
        if len > 0 then
            moveX = moveX / len
            moveY = moveY / len
        end

        entity.velocity = Vec2(moveX * speed, moveY * speed)
        currentState = isSprinting and "run" or "walk"

        -- Scale animation speed proportionally to walk speed
        if entity:hasSpriteAnimator() then
            local playbackSpeed = (self.baseWalkSpeed > 0)
                and (speed / self.baseWalkSpeed)
                or 1.0
            entity:setAnimationPlaybackSpeed(playbackSpeed)
        end

        -- Legacy footstep audio fallback. If a FootstepAudio component is present,
        -- the C++ runtime component handles playback instead.
        if not entity:hasFootstepAudio() then
            self._footstepTimer = self._footstepTimer - dt
            if self._footstepTimer <= 0.0 then
                local interval = self.footstepInterval
                if isSprinting then interval = interval * 0.6 end
                self._footstepTimer = interval
                local variation = math.random(1, 4)
                Audio.playOneShot("fs_concrete_" .. variation)
            end
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
            self._manualTorchOff = not self._torchActive
        end

        if (not self._batteryDying) and input:wasKeyPressed(Keys.F) then
            local charge = entity:hasBattery() and entity:getBatteryCharge() or 1.0
            if charge > 0.0 then
                self._torchActive = not self._torchActive
                self._manualTorchOff = not self._torchActive
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

        -- Battery recharges whenever the torch is off.
        if entity:hasBattery() then
            entity:setBatteryCharging(not self._torchActive)
            if self._torchActive and (not self._batteryDying) and entity:getBatteryCharge() <= 0.0 then
                self._batteryDying = true
                self._flickerTimer = BATTERY_DEATH_FLICKER_TIME
                self._flickerStep = 0.0
            end

            if self._batteryDying then
                self._flickerTimer = self._flickerTimer - dt
                self._flickerStep = self._flickerStep - dt

                if self._flickerStep <= 0.0 and self._flickerTimer > 0.0 then
                    self._flickerStep = BATTERY_DEATH_FLICKER_MIN_STEP +
                        math.random() * (BATTERY_DEATH_FLICKER_MAX_STEP - BATTERY_DEATH_FLICKER_MIN_STEP)
                    entity:setLightActive(not entity:isLightActive())
                end

                if self._flickerTimer <= 0.0 then
                    self._batteryDying = false
                    self._torchActive = false
                    self._manualTorchOff = false
                    entity:setLightActive(false)
                    Audio.playOneShot("flashlight_off")
                end
            end
        end

        -- Torch bobbing
        if self._torchActive and not self._batteryDying then
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
        if self._torchActive and not self._batteryDying then
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

