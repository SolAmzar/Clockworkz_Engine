-------------------------------------------------------------------------------
-- fragment_system.lua
-- Attach this script to a dedicated "FragmentManager" entity in the scene.
-- Handles: fragment bobbing animation, light-cone visibility fade,
--          and proximity collection by the player's FragmentCollector.
-------------------------------------------------------------------------------

local FragmentSystem = {}

FragmentSystem.fadeSpeed = 5.0

function FragmentSystem:onStart(entity)
    print("[fragment_system] Started on entity " .. entity.id)
end

function FragmentSystem:onUpdate(entity, dt)
    -- Find player for collection checks
    local player = World.findByTag("Player")
    local hasCollector = player and player:isValid() and player:hasFragmentCollector()

    World.eachFragment(function(frag)
        if not frag:isValid() then return end

        -- Post-collect: wait briefly then hide
        if frag:isFragmentCollected() then
            local timer = frag:getFragmentAnimTimer() + dt
            frag:setFragmentAnimTimer(timer)
            if timer > 0.5 then
                frag:setSpriteAlpha(0.0)
            end
            return
        end

        -- Bobbing animation
        local oldTimer = frag:getFragmentAnimTimer()
        local newTimer = oldTimer + dt
        frag:setFragmentAnimTimer(newTimer)

        local bobSpeed  = frag:getFragmentBobSpeed()
        local bobAmount = frag:getFragmentBobAmount()
        local oldOffset = math.sin(oldTimer * bobSpeed) * bobAmount
        local newOffset = math.sin(newTimer * bobSpeed) * bobAmount

        local pos = frag.position
        frag.position = Vec2(pos.x, pos.y + (newOffset - oldOffset))

        -- Visibility (light-cone check)
        local targetAlpha = 0.0

        if not frag:getFragmentRequiresLight() then
            targetAlpha = 1.0
        else
            World.eachTorch(function(torchEnt)
                if targetAlpha >= 1.0 then return end
                if not torchEnt:isValid() then return end
                if not torchEnt:isTorchActive() then return end
                if torchEnt:hasBattery() and torchEnt:getBatteryCharge() <= 0.0 then return end

                local rot  = torchEnt.rotation
                local c, s = math.cos(rot), math.sin(rot)
                local ox, oy = torchEnt:getTorchOffsetX(), torchEnt:getTorchOffsetY()
                local tx = torchEnt.position.x + ox * c - oy * s
                local ty = torchEnt.position.y + ox * s + oy * c

                local fragPos = frag.position
                local dx = fragPos.x - tx
                local dy = fragPos.y - ty
                local distSq = dx * dx + dy * dy
                local radius = torchEnt:getTorchRadius()

                if distSq <= radius * radius then
                    local dist = math.sqrt(distSq)
                    if dist > 0.0001 then
                        local fdx, fdy = dx / dist, dy / dist
                        local dotProd  = math.cos(rot) * fdx + math.sin(rot) * fdy
                        local halfFov  = (torchEnt:getTorchAngle() * math.pi / 180.0) * 0.5
                        if dotProd >= math.cos(halfFov) then
                            targetAlpha = 1.0
                        end
                    else
                        targetAlpha = 1.0
                    end
                end
            end)
        end

        -- Smooth fade
        local curAlpha = frag:getFragmentAlpha()
        if curAlpha < targetAlpha then
            curAlpha = math.min(curAlpha + self.fadeSpeed * dt, targetAlpha)
        elseif curAlpha > targetAlpha then
            curAlpha = math.max(curAlpha - self.fadeSpeed * dt, targetAlpha)
        end
        frag:setFragmentAlpha(curAlpha)
        if frag:hasSpriteRenderer() then
            frag:setSpriteAlpha(curAlpha)
            frag:setSpriteUnlit(true)
        end

        -- Collection check
        if hasCollector then
            local playerPos = player.position
            local fragPos2  = frag.position
            local dx2 = fragPos2.x - playerPos.x
            local dy2 = fragPos2.y - playerPos.y
            local dist2 = math.sqrt(dx2 * dx2 + dy2 * dy2)

            if dist2 <= player:getPickupRadius() then
                frag:setFragmentCollected(true)
                frag:setFragmentAnimTimer(0.0)
                player:setCollectedCount(player:getCollectedCount() + 1)
                print("[fragment_system] Fragment collected! Count: " .. player:getCollectedCount())
            end
        end
    end)
end

return FragmentSystem
