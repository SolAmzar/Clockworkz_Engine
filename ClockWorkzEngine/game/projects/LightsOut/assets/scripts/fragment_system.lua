-------------------------------------------------------------------------------
-- fragment_system.lua
-- Attach to a "FragmentManager" entity. Handles light-cone visibility fade
-- for all entities that have components/fragment.lua attached.
-- Bobbing is handled by fragment.lua itself. Collection by fragment_collector.lua.
-------------------------------------------------------------------------------

local FragmentSystem = {}

function FragmentSystem:onStart(entity)
    print("[fragment_system] Started on entity " .. entity.id)
end

function FragmentSystem:onUpdate(entity, dt)
    World.eachWithScript("components/fragment.lua", function(frag)
        if not frag:isValid() then return end

        local collected = World.getScriptProp(frag.id, "collected") or false
        if collected then
            frag:setSpriteAlpha(0.0)
            return
        end

        local requiresLight = World.getScriptProp(frag.id, "requiresLight")
        if requiresLight == nil then requiresLight = true end

        -- Determine target alpha based on whether fragment needs to be in light
        local targetAlpha = 0.0
        if not requiresLight then
            targetAlpha = 1.0
        else
            World.eachLight(function(torchEnt)
                if targetAlpha >= 1.0 then return end
                if not torchEnt:isValid() then return end
                if not torchEnt:isLightActive() then return end
                -- Skip if battery is dead
                local charge = World.getScriptProp(torchEnt.id, "charge")
                if charge ~= nil and charge <= 0.0 then return end

                local rot  = torchEnt.rotation
                local c, s = math.cos(rot), math.sin(rot)
                local ox   = torchEnt:getLightOffsetX()
                local oy   = torchEnt:getLightOffsetY()
                local tx   = torchEnt.position.x + ox * c - oy * s
                local ty   = torchEnt.position.y + ox * s + oy * c

                local fragPos = frag.position
                local dx   = fragPos.x - tx
                local dy   = fragPos.y - ty
                local distSq = dx * dx + dy * dy
                local radius = torchEnt:getLightRadius()

                if distSq <= radius * radius then
                    local dist = math.sqrt(distSq)
                    if dist > 0.0001 then
                        local fdx = dx / dist
                        local fdy = dy / dist
                        local dotProd = math.cos(rot) * fdx + math.sin(rot) * fdy
                        local halfFov = (torchEnt:getLightAngle() * math.pi / 180.0) * 0.5
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
        local fadeSpeed = World.getScriptProp(frag.id, "fadeSpeed") or 5.0
        local curAlpha  = World.getScriptProp(frag.id, "_alpha") or 1.0
        if curAlpha < targetAlpha then
            curAlpha = math.min(curAlpha + fadeSpeed * dt, targetAlpha)
        elseif curAlpha > targetAlpha then
            curAlpha = math.max(curAlpha - fadeSpeed * dt, targetAlpha)
        end
        World.setScriptProp(frag.id, "_alpha", curAlpha)
        if frag:hasSpriteRenderer() then
            frag:setSpriteAlpha(curAlpha)
            frag:setSpriteUnlit(true)
        end
    end)
end

return FragmentSystem
