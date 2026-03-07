-------------------------------------------------------------------------------
-- light_exposure_system.lua
-- Attach this script to a dedicated "LightExposureManager" entity.
-- Tracks whether LightExposure entities (e.g. the player) are inside any
-- active torch cone, accumulating exposure time.  isDangerous fires when
-- exposureTime reaches maxExposureTime.
--
-- Note: this is a simplified version (no occlusion raycasting).
--       Add solid-collider checks here if you need shadow blocking.
-------------------------------------------------------------------------------

local LightExposureSystem = {}

function LightExposureSystem:onStart(entity)
    print("[light_exposure_system] Started on entity " .. entity.id)
end

function LightExposureSystem:onUpdate(entity, dt)
    -- 1. Reset inLight flag for every target
    World.eachLightExposure(function(ent)
        if ent:isValid() then
            ent:setInLight(false)
        end
    end)

    -- 2. For each active torch, mark targets that fall inside its cone
    World.eachLight(function(torchEnt)
        if not torchEnt:isValid() then return end
        if not torchEnt:isLightActive() then return end
        if torchEnt:hasBattery() and torchEnt:getBatteryCharge() <= 0.0 then return end

        -- Torch world-space origin
        local rot  = torchEnt.rotation
        local c, s = math.cos(rot), math.sin(rot)
        local ox, oy = torchEnt:getLightOffsetX(), torchEnt:getLightOffsetY()
        local tx = torchEnt.position.x + ox * c - oy * s
        local ty = torchEnt.position.y + ox * s + oy * c

        local tdx = math.cos(rot)
        local tdy = math.sin(rot)
        local radius  = torchEnt:getLightRadius()
        local radiusSq = radius * radius
        local halfFov  = (torchEnt:getLightAngle() * math.pi / 180.0) * 0.5
        local cosHalf  = math.cos(halfFov)

        World.eachLightExposure(function(target)
            if not target:isValid() then return end
            if target:isInLight() then return end  -- already lit

            local dx = target.position.x - tx
            local dy = target.position.y - ty
            local distSq = dx * dx + dy * dy

            if distSq > radiusSq then return end

            local dist = math.sqrt(distSq)
            if dist > 0.0001 then
                local dirDot = tdx * (dx / dist) + tdy * (dy / dist)
                if dirDot >= cosHalf then
                    target:setInLight(true)
                end
            else
                target:setInLight(true)
            end
        end)
    end)

    -- 3. Accumulate / drain exposure time
    World.eachLightExposure(function(ent)
        if not ent:isValid() then return end

        local t    = ent:getLightExposureTime()
        local maxT = ent:getMaxLightExposureTime()

        if ent:isInLight() then
            t = math.min(t + dt, maxT)
        else
            t = math.max(t - dt, 0.0)
        end

        ent:setLightExposureTime(t)
    end)
end

return LightExposureSystem
