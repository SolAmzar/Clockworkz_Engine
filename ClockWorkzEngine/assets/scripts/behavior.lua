-- *****************************************************************************
-- file           behavior.lua
-- project        Lights Out
-- author(s)      [Syafiq] 100%
-- brief          lua script for behaviour aspect of GameObject

--All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
--******************************************************************************

local Behavior = {}

-- Properties that can be modified
Behavior.moveSpeed = 100
Behavior.direction = Vec2(1, 0)
Behavior.timer = 0
Behavior.switchInterval = 2.0

-- Called once when the component initializes
function Behavior:onStart(gameObject)
    Log("Behavior started for: " .. gameObject.name)
    Log("  Position: " .. tostring(gameObject.position))
    Log("  Type: " .. tostring(gameObject.type))

    -- Store initial position
    self.startPosition = gameObject.position
end

-- Called every frame
function Behavior:onUpdate(gameObject, dt)
    -- Example: Move back and forth
    self.timer = self.timer + dt

    if self.timer >= self.switchInterval then
        -- Reverse direction
        --self.direction = self.direction * -1
        --self.timer = 0
    end

    -- Calculate new velocity
    local velocity = self.direction * self.moveSpeed
    gameObject.velocity = velocity

    -- Modify position:
    local pos = gameObject.position
    pos.x = pos.x + self.moveSpeed * dt * self.direction.x
    gameObject.position = pos
end

-- Collision Logic
function Behavior:onCollision(gameObject, other)
    Log("[Behavior Lua] Collision! " .. gameObject.name .. " hit " .. other.name)

    -- Zero direction on wall collision
    if other.type == GameObjectType.Wall then
        self.direction = self.direction 
        self.timer = 0
    end

    --Check if hit by hazard
    if other.hazard then
        local health = gameObject:getHealth()
        if health then
            Log("  Taking damage! Current HP: " .. health:hp())
        end
    end
end

-- Called when the component is destroyed
function Behavior:onDestroy(gameObject)
    Log("[Behavior Lua] Destroyed Behavior for: " .. gameObject.name)
end

-- Return the behavior table
return Behavior
