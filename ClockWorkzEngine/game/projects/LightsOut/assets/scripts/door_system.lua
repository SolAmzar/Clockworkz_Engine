-------------------------------------------------------------------------------
-- door_system.lua
-- Attach this script to a dedicated "DoorManager" entity in the scene.
-- Handles: door unlock when enough fragments collected,
--          sprite texture swap, and DoorSwing animation.
-------------------------------------------------------------------------------

local DoorSystem = {}

function DoorSystem:onStart(entity)
    print("[door_system] Started on entity " .. entity.id)
end

function DoorSystem:onUpdate(entity, dt)
    -- Get the total collected fragments from the player
    local totalCollected = 0
    local player = World.findByTag("Player")
    if player and player:isValid() and player:hasFragmentCollector() then
        totalCollected = player:getCollectedCount()
    end

    -- Unlock doors when fragment requirement is met
    World.eachDoor(function(door)
        if not door:isValid() then return end
        if door:isDoorOpen() then return end  -- already open, skip

        local required = door:getDoorRequiredFragments()
        if totalCollected >= required then
            door:setDoorOpen(true)

            -- Swap to open texture
            if door:hasSpriteRenderer() then
                door:setSpriteTextureId(door:getDoorOpenTexture())
            end

            -- Allow physics to move the door (pushed by player / swings)
            if door:hasCollider() then
                door:setColliderMoveable(true)
            end

            -- Trigger swing animation if door also has a DoorSwing component
            if door:hasDoorSwing() then
                door:setDoorSwingOpen(true)
            end

            print("[door_system] Door unlocked! Required=" .. required)
        else
            -- Ensure closed state is maintained (handles scene reloads)
            if door:hasSpriteRenderer() then
                door:setSpriteTextureId(door:getDoorClosedTexture())
            end
            if door:hasCollider() then
                door:setColliderMoveable(false)
            end
        end
    end)

    -- Animate all DoorSwing entities (interpolate rotation toward target)
    World.eachDoorSwing(function(swing)
        if not swing:isValid() then return end

        -- Capture initial rotation on first frame
        if not swing:isDoorSwingInitialized() then
            swing:setDoorSwingInitialRotation(swing.rotation)
            swing:setDoorSwingInitialized(true)
        end

        -- Target rotation depends on isOpen flag
        local swingRad   = swing:getDoorSwingAngle() * math.pi / 180.0
        local initialRot = swing:getDoorSwingInitialRotation()
        local targetRot  = initialRot

        if swing:isDoorSwingOpen() then
            if swing:isDoorSwingClockwise() then
                targetRot = initialRot - swingRad
            else
                targetRot = initialRot + swingRad
            end
        end

        -- Step current rotation toward target
        local speedRad  = swing:getDoorSwingSpeed() * math.pi / 180.0 * dt
        local currentRot = swing.rotation
        local diff = targetRot - currentRot

        if math.abs(diff) > 0.001 then
            if math.abs(diff) <= speedRad then
                swing.rotation = targetRot
            else
                local sign = diff > 0 and 1.0 or -1.0
                swing.rotation = currentRot + sign * speedRad
            end
        end
    end)
end

return DoorSystem
