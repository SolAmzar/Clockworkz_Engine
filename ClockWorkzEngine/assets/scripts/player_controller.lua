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

function PlayerController:onStart(gameObject)
    Log("[Controller Lua] Player controller initialized for: " .. gameObject.name)
    gameObject.moveSpeed = self.moveSpeed
end

function PlayerController:onUpdate(gameObject, dt)
    -- Get input manager from Engine
    local input = Engine.input()
    local vel = Vec2(0, 0)

    -- WS movement
    if input:isKeyDown(Keys.W) then vel.y = vel.y + 1 end
    if input:isKeyDown(Keys.S) then vel.y = vel.y - 1 end

    -- Normalize and apply speed
    if vel:length() > 0 then
        vel = vel:normalized()

        local speed = self.moveSpeed
        if input:isKeyDown(Keys.LEFT_SHIFT) then
            speed = speed * self.sprintMultiplier
        end

        vel = vel * speed
        self.isMoving = true
    else
        self.isMoving = false
    end

    gameObject.velocity = vel
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
