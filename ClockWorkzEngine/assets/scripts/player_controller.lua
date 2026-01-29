-- *****************************************************************************
-- file           player_controller.lua
-- project        Lights Out
-- author(s)      [Syafiq] 100%
-- brief          lua script for player_controller

--All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
--******************************************************************************

local PlayerController = {}

-- Configurable properties
PlayerController.moveSpeed = 200
PlayerController.sprintMultiplier = 1.5

-- Internal state
PlayerController.isMoving = false
PlayerController.vel = nil

function PlayerController:onStart(gameObject)
    Log("[Controller Lua] Player controller initialized for: " .. gameObject.name)
    gameObject.moveSpeed = self.moveSpeed
    -- Create reusable vec2 once
    self.vel = Vec2(0, 0)
end

function PlayerController:onUpdate(gameObject, dt)
    local input = Engine.input()

    local velX = 0
    local velY = 0

    -- W = forward, S = backward
    if input:isKeyDown(Keys.W) then
        velY = velY + 1
    end
    if input:isKeyDown(Keys.S) then
        velY = velY - 1
    end

    -- Normalize and apply speed using raw math
    local len = math.sqrt(velX * velX + velY * velY)
    if len > 0 then
        velX = velX / len
        velY = velY / len

        local speed = self.moveSpeed
        if input:isKeyDown(Keys.LEFT_SHIFT) then
            speed = speed * self.sprintMultiplier
        end

        velX = velX * speed
        velY = velY * speed
        self.isMoving = true
    else
        self.isMoving = false
    end

    -- Reuse existing vec2 object
    self.vel:set(velX, velY)
    gameObject.velocity = self.vel
end

function PlayerController:onCollision(gameObject, other)
    -- Handle collision with hazards
    if other.hazard then
        local health = gameObject:getHealth()
        if health then
            local dealer = other:getDamageDealer()
            local damage = 1
            if dealer then
                damage = dealer.damage
            end
            health:damage(gameObject, damage)
            Log("[Controller Lua] Player took " .. damage .. " damage. Current health is " .. health:hp())
        end
    end

    -- Handle collision with collectibles
    if other.type == GameObjectType.Fragment then
        Log("[Controller Lua] Fragment Collected!")
        other.active = false
    end
end

function PlayerController:onDestroy(gameObject)
    Log("[Controller Lua] Player controller destroyed")
end

return PlayerController
