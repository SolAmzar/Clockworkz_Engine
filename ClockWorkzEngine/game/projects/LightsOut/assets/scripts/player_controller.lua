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
    { name = "pushSpriteSheet",   type = "texture",   default = "",     label = "Push Sprite Sheet"   },
    { name = "pushAnimFile",      type = "animation", default = "",     label = "Push Anim File"       },
    { name = "pushClipName",      type = "string",    default = "push", label = "Push Clip Name"       },
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

PlayerController.pushSpriteSheet    = ""
PlayerController.pushAnimFile       = ""
PlayerController.pushClipName       = "push"
PlayerController._isPushing         = false
PlayerController._pushTarget        = nil
PlayerController._origAnimFile      = ""
PlayerController._origTexture       = ""

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
PlayerController._usingGamepad   = false
PlayerController._prevMouseX     = nil   -- nil until first frame so no false delta
PlayerController._prevMouseY     = nil
local DEATH_RESPAWN_DELAY        = 2.8   -- seconds before level reloads
local BATTERY_DEATH_FLICKER_TIME = 0.9
local BATTERY_DEATH_FLICKER_MIN_STEP = 0.015
local BATTERY_DEATH_FLICKER_MAX_STEP = 0.08

function PlayerController:onStart(entity)
    entity.moveSpeed = self.walkSpeed
    if entity:hasSpriteAnimator() then
        self._origAnimFile = entity:getAnimationFile()
    end
    if entity:hasSpriteRenderer() then
        self._origTexture = entity:getSpriteTextureId()
    end
end

function PlayerController:onUpdate(entity, dt)
    if self._isDead then
        if entity:isValid() then
            entity.velocity = Vec2(0, 0)
        end
        return
    end

    local input        = Engine.input()
    local isSprinting  = input:isKeyDown(Keys.LEFT_SHIFT)
                      or input:isKeyDown(Keys.RIGHT_SHIFT)
                      or Gamepad:isActionDown("sprint")

    -- ----------------------------------------------------------------
    -- Gamepad axes (resolved before movement and rotation are computed)
    -- ----------------------------------------------------------------
    local gpMoveX   = Gamepad:getActionAxis("move_x")
    local gpMoveY   = Gamepad:getActionAxis("move_y")
    local rx        = Gamepad:getActionAxis("look_x")
    local ry        = Gamepad:getActionAxis("look_y")
    local hasGpMove = (math.abs(gpMoveX) > 0.01 or math.abs(gpMoveY) > 0.01)
    local stickMag  = math.sqrt(rx * rx + ry * ry)

    -- Release cooldown: the PS5 spring bounces past zero at ~0.4 magnitude
    -- for 1-3 frames after release. When the stick drops from high (>0.5)
    -- to low, suppress input for 4 frames to let the spring settle.
    if not self._stickWasHigh    then self._stickWasHigh    = false end
    if not self._releaseCooldown then self._releaseCooldown = 0     end
    local stickHigh = (stickMag > 0.5)
    if self._stickWasHigh and not stickHigh then
        self._releaseCooldown = 4
    end
    if self._releaseCooldown > 0 then
        self._releaseCooldown = self._releaseCooldown - 1
        rx = 0; ry = 0; stickMag = 0
    end
    self._stickWasHigh = stickHigh

    -- ----------------------------------------------------------------
    -- Device tracking: switch to gamepad on any stick input,
    -- switch back to mouse when the mouse actually moves (> 2px world-space)
    -- ----------------------------------------------------------------
    if stickMag > 0.3 or hasGpMove or Gamepad:isActionPressed("flashlight") then
        self._usingGamepad = true
    end

    -- Use screen-space position for the "did mouse move?" check so that
    -- camera movement (from player movement) does not falsely trigger
    -- a switch back to mouse mode.
    local ms = Engine.input():mousePosition()
    if self._prevMouseX ~= nil then
        local mdx = ms.x - self._prevMouseX
        local mdy = ms.y - self._prevMouseY
        if math.sqrt(mdx * mdx + mdy * mdy) > 2.0 then
            self._usingGamepad = false
        end
    end
    self._prevMouseX = ms.x
    self._prevMouseY = ms.y

    -- ----------------------------------------------------------------
    -- Rotation:
    --   gamepad mode + right stick active → face stick direction
    --   gamepad mode + right stick at rest → hold last rotation
    --   mouse mode → face mouse cursor
    -- ----------------------------------------------------------------
    if self._usingGamepad then
        if stickMag > 0.3 then
            entity.rotation = math.atan(ry, rx)
        end
        -- right stick at rest: rotation unchanged
    else
        local mw = Engine.mouseWorldPos()
        local dx = mw.x - entity.position.x
        local dy = mw.y - entity.position.y
        if math.sqrt(dx * dx + dy * dy) > 10.0 then
            entity.rotation = math.atan(dy, dx)
        end
    end

    -- ----------------------------------------------------------------
    -- Movement:
    --   gamepad left stick → world-space (screen-relative)
    --   keyboard WASD      → rotation-relative (existing behaviour)
    -- ----------------------------------------------------------------
    local forwardInput = 0
    local strafeInput  = 0
    if input:isKeyDown(Keys.W) then forwardInput = forwardInput + 1 end
    if input:isKeyDown(Keys.S) then forwardInput = forwardInput - 1 end
    if input:isKeyDown(Keys.D) then strafeInput = strafeInput - 1 end
    if input:isKeyDown(Keys.A) then strafeInput = strafeInput + 1 end

    local currentState = "idle"
    if hasGpMove then
        -- Screen-space: stick up = up on screen, stick right = right on screen
        local speedMul = isSprinting and self.sprintMultiplier or 1.0
        local speed    = self.walkSpeed * speedMul
        entity.velocity = Vec2(gpMoveX * speed, gpMoveY * speed)
        currentState = isSprinting and "run" or "walk"
    elseif forwardInput ~= 0 or strafeInput ~= 0 then
        local speedMul = isSprinting and self.sprintMultiplier or 1.0
        local speed    = self.walkSpeed * speedMul

        local angle    = entity.rotation
        local forwardX = math.cos(angle)
        local forwardY = math.sin(angle)
        local rightX   = -forwardY
        local rightY   = forwardX

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

        if (not self._batteryDying) and (input:wasKeyPressed(Keys.F) or Gamepad:isActionPressed("flashlight")) then
            self._torchActive = not self._torchActive
            self._manualTorchOff = not self._torchActive
            
            local charge = entity:hasBattery() and entity:getBatteryCharge() or 1.0
            if self._torchActive then
                if charge > 0.0 then
                    entity:setLightActive(true)
                else
                    entity:setLightActive(false)
                end
                Audio.playOneShot("flashlight_on")
            else
                entity:setLightActive(false)
                Audio.playOneShot("flashlight_off")
            end
        end

        -- Battery recharges whenever the torch is off.
        if entity:hasBattery() then
            entity:setBatteryCharging(not self._torchActive)
            if self._torchActive and (not self._batteryDying) and entity:isLightActive() then
                local charge = entity:getBatteryCharge()
                local maxCharge = entity:getBatteryMaxCharge()
                local pct = charge / maxCharge
                
                -- Base intensity follows battery percentage
                local targetIntensity = pct
                
                -- Add flickering when battery is low (below 25%)
                if pct < 0.25 then
                    local lowBatSeverity = 1.0 - (pct / 0.25) -- 0.0 at 25%, 1.0 at 0%
                    if math.random() < (0.1 + lowBatSeverity * 0.4) then
                        -- Random dip in intensity
                        targetIntensity = targetIntensity * (0.2 + math.random() * 0.5)
                    end
                end
                
                entity:setLightIntensity(targetIntensity)
            end

            if entity:getBatteryCharge() <= 0.0 and self._torchActive and entity:isLightActive() and (not self._batteryDying) then
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
                    -- The switch stays ON, player must turn it off manually to recharge
                    entity:setLightActive(false)
                    entity:setLightIntensity(1.0) -- Reset for next use
                end
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
        if not self._isPushing then  -- <-- add this guard
            if entity:getCurrentClip() ~= currentState then
                entity:playAnimation(currentState)
            end
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

    elseif other:isColliderMoveable() then
        if not self._isPushing then
            self._isPushing  = true
            self._pushTarget = other
            print("[PC] Push start: pushAnimFile='" .. self.pushAnimFile .. "' pushClipName='" .. self.pushClipName .. "' pushSpriteSheet='" .. self.pushSpriteSheet .. "'")
            if self.pushSpriteSheet ~= "" then
                entity:setSpriteTextureId(self.pushSpriteSheet)
            end
            if entity:hasSpriteAnimator() and self.pushAnimFile ~= "" then
                entity:loadAnimationFile(self.pushAnimFile)
                entity:playAnimation(self.pushClipName)
                print("[PC] After load: currentClip='" .. entity:getCurrentClip() .. "'")
            else
                print("[PC] Push skipped: hasSpriteAnimator=" .. tostring(entity:hasSpriteAnimator()) .. " pushAnimFile='" .. self.pushAnimFile .. "'")
            end
        end
    end
end

function PlayerController:onCollisionExit(entity, other)
    print("[PC] onCollisionExit: other tag='" .. other:getTagValue() .. "' moveable=" .. tostring(other:isColliderMoveable()))
    if other:isColliderMoveable() and self._isPushing then
        self._isPushing  = false
        self._pushTarget = nil
        if self._origTexture ~= "" then
            entity:setSpriteTextureId(self._origTexture)
        end
        if entity:hasSpriteAnimator() and self._origAnimFile ~= "" then
            entity:loadAnimationFile(self._origAnimFile)
            entity:playAnimation("idle")
        end
    end
end

return PlayerController

