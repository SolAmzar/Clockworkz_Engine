-- *****************************************************************************
-- file           behavior.lua
-- project        Lights Out
-- author(s)      [Syafiq] 100%
-- brief          Lua script for player movement
--
--All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
--******************************************************************************


local PlayerMovement = {}

-- Speed multipliers (match PlayerController defaults)
PlayerMovement.walkMultiplier   = 1.0
PlayerMovement.sprintMultiplier = 2.0

function PlayerMovement:onStart(entity)
    print("[player_movement] onStart — entity " .. entity.id)
end

function PlayerMovement:onUpdate(entity, dt)
    if not entity:isValid() then return end

    local input = Engine.input()

    -- Forward/backward input (W / S)
    local forwardInput = 0
    if input:isKeyDown(Keys.W) then forwardInput = forwardInput + 1 end
    if input:isKeyDown(Keys.S) then forwardInput = forwardInput - 1 end

    if forwardInput ~= 0 then
        -- Sprint check
        local isSprinting = input:isKeyDown(Keys.LEFT_SHIFT)
                         or input:isKeyDown(Keys.RIGHT_SHIFT)

        local speedMul    = isSprinting and self.sprintMultiplier
                                        or  self.walkMultiplier
        local currentSpeed = entity.moveSpeed * speedMul

        -- Forward direction based on entity rotation
        local angle = entity.rotation
        local fx = math.cos(angle)
        local fy = math.sin(angle)

        -- Movement direction
        local mx = fx * forwardInput
        local my = fy * forwardInput

        -- Normalize
        local len = math.sqrt(mx * mx + my * my)
        if len > 0 then
            mx = mx / len
            my = my / len
        end

        entity.velocity = Vec2(mx * currentSpeed, my * currentSpeed)
    else
        entity.velocity = Vec2(0, 0)
    end
end

return PlayerMovement
