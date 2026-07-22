-------------------------------------------------------------------------------
-- moth_ai.lua
-- Updated:
--  - Uses normalized movement + moveSpeed
--  - Cancels dash when hitting walls
--  - Stops pushing through obstacles
-------------------------------------------------------------------------------

Properties = {
    { name = "chaseSpeed",          type = "float",  default = 3.30,  label = "Chase Speed" },
    { name = "attackRange",         type = "float",  default = 182.0, label = "Attack Range" },
    { name = "dashSpeed",           type = "float",  default = 10.0,  label = "Dash Speed" },
    { name = "dashDuration",        type = "float",  default = 0.25,  label = "Dash Duration (s)" },
    { name = "pathInterval",        type = "float",  default = 0.5,   label = "Path Recalc Interval (s)" },

    { name = "idleStateSfxPrefix",  type = "string", default = "",     label = "Idle SFX Prefix" },
    { name = "chaseStateSfxPrefix", type = "string", default = "",     label = "Chase SFX Prefix" },
    { name = "deathStateSfxPrefix", type = "string", default = "",     label = "Death SFX Prefix" },

    { name = "idleStateSfx",        type = "string", default = "",     label = "" },
    { name = "chaseStateSfx",       type = "string", default = "",     label = "" },
    { name = "deathStateSfx",       type = "string", default = "",     label = "" },

    { name = "idleStateSfxVariations",  type = "int", default = 1,     label = "Idle SFX Variations" },
    { name = "chaseStateSfxVariations", type = "int", default = 1,     label = "Chase SFX Variations" },
    { name = "deathStateSfxVariations", type = "int", default = 1,     label = "Death SFX Variations" },

    { name = "_state",              type = "string", default = "Idle", label = "" },
    { name = "_alpha",              type = "float",  default = 1.0,    label = "" },
    { name = "_deathSfxPlayed",     type = "bool",   default = false,  label = "" },
    { name = "_colliderDisabled",   type = "bool",   default = false,  label = "" },
    { name = "_dashTimer",          type = "float",  default = 0.0,    label = "" },
    { name = "_dashDirX",           type = "float",  default = 0.0,    label = "" },
    { name = "_dashDirY",           type = "float",  default = 0.0,    label = "" },
    { name = "_initialized",        type = "bool",   default = false,  label = "" },
}

local function norm(dx, dy)
    local len = math.sqrt(dx * dx + dy * dy)
    if len < 0.0001 then
        return 0.0, 0.0
    end
    return dx / len, dy / len
end

-------------------------------------------------------------------------------
-- A* pathfinding (mirrors enemy_ai.lua; see that file for detailed notes).
-- Per-moth state via chunk locals — each moth re-executes this file in its own
-- sandboxed environment, so these are not shared between instances.
-------------------------------------------------------------------------------
local CELL_SIZE  = 100.0
local EPS        = 0.001
local _grid      = nil
local _path      = nil
local _pathIdx   = 1
local _pathTimer = 0.0
local _navVersion = -1  -- World.navVersion() the grid was last built at

local function worldToCell(x, y)
    return math.floor(x / CELL_SIZE), math.floor(y / CELL_SIZE)
end

local function isBlocked(grid, cx, cy)
    return grid[cx] ~= nil and grid[cx][cy] == true
end

local function markCell(grid, cx, cy)
    if not grid[cx] then grid[cx] = {} end
    grid[cx][cy] = true
end

-- Nearest walkable cell to (cx,cy), spiralling out up to maxRing.
local function nearestFreeCell(grid, cx, cy, maxRing)
    if not isBlocked(grid, cx, cy) then return cx, cy end
    for r = 1, maxRing do
        for dx = -r, r do
            for dy = -r, r do
                if math.abs(dx) == r or math.abs(dy) == r then
                    if not isBlocked(grid, cx + dx, cy + dy) then
                        return cx + dx, cy + dy
                    end
                end
            end
        end
    end
    return nil, nil
end

-- Build blocked-cell set from wall AABBs (marks every overlapped cell so merged
-- multi-tile wall colliders are fully represented).
local function buildGrid()
    local grid = {}
    World.eachWall(function(e, minX, minY, maxX, maxY)
        if minX == nil then
            if e and e:isValid() then
                local pos = e.position
                markCell(grid, worldToCell(pos.x, pos.y))
            end
            return
        end
        local c0 = math.floor(minX / CELL_SIZE)
        local c1 = math.floor((maxX - EPS) / CELL_SIZE)
        local r0 = math.floor(minY / CELL_SIZE)
        local r1 = math.floor((maxY - EPS) / CELL_SIZE)
        for cx = c0, c1 do
            for cy = r0, r1 do
                markCell(grid, cx, cy)
            end
        end
    end)
    return grid
end

local function heuristic(ax, ay, bx, by)
    local dx = math.abs(ax - bx)
    local dy = math.abs(ay - by)
    return dx + dy + (1.414 - 2.0) * math.min(dx, dy)
end

local function findPath(grid, startX, startY, goalX, goalY)
    local sx, sy = worldToCell(startX, startY)
    local gx, gy = worldToCell(goalX, goalY)

    if sx == gx and sy == gy then return {} end
    if isBlocked(grid, gx, gy) then
        gx, gy = nearestFreeCell(grid, gx, gy, 3)
        if gx == nil then return nil end
        if sx == gx and sy == gy then return {} end
    end

    local function cellKey(cx, cy) return cx .. "," .. cy end

    local dirs = {
        {1,0,1.0}, {-1,0,1.0}, {0,1,1.0}, {0,-1,1.0},
        {1,1,1.414}, {1,-1,1.414}, {-1,1,1.414}, {-1,-1,1.414},
    }

    local startKey = cellKey(sx, sy)
    local open     = {{ cx=sx, cy=sy, g=0.0, f=heuristic(sx,sy,gx,gy), parent=nil }}
    local gScore   = { [startKey] = 0.0 }
    local closed   = {}

    local MAX_ITER = 400
    local iter = 0

    while #open > 0 and iter < MAX_ITER do
        iter = iter + 1

        local bestIdx = 1
        for i = 2, #open do
            if open[i].f < open[bestIdx].f then bestIdx = i end
        end
        local cur = table.remove(open, bestIdx)
        local curKey = cellKey(cur.cx, cur.cy)

        if not closed[curKey] then
            closed[curKey] = true

            if cur.cx == gx and cur.cy == gy then
                local path = {}
                local node = cur
                while node do
                    table.insert(path, 1, { x = node.cx * CELL_SIZE + CELL_SIZE * 0.5, y = node.cy * CELL_SIZE + CELL_SIZE * 0.5 })
                    node = node.parent
                end
                return path
            end

            for _, dir in ipairs(dirs) do
                local nx, ny  = cur.cx + dir[1], cur.cy + dir[2]
                local nKey    = cellKey(nx, ny)
                local diagonal = dir[1] ~= 0 and dir[2] ~= 0

                local passable = not closed[nKey] and not isBlocked(grid, nx, ny)
                if passable and diagonal then
                    passable = not isBlocked(grid, cur.cx + dir[1], cur.cy)
                           and not isBlocked(grid, cur.cx, cur.cy + dir[2])
                end

                if passable then
                    local newG = cur.g + dir[3]
                    if not gScore[nKey] or newG < gScore[nKey] then
                        gScore[nKey] = newG
                        table.insert(open, {
                            cx = nx, cy = ny, g = newG,
                            f = newG + heuristic(nx, ny, gx, gy),
                            parent = cur,
                        })
                    end
                end
            end
        end
    end

    return nil
end

local function inCone(px, py, tx, ty, rot, radius, fovDeg)
    local dx, dy = px - tx, py - ty
    if dx * dx + dy * dy > radius * radius then
        return false
    end

    local dist = math.sqrt(dx * dx + dy * dy)
    if dist < 0.0001 then
        return true
    end

    local dot = math.cos(rot) * (dx / dist) + math.sin(rot) * (dy / dist)
    return dot >= math.cos((fovDeg * math.pi / 180.0) * 0.5)
end

local function stateSfxId(self, state)
    local prefix = ""
    local legacy = ""
    local variations = 1

    if state == "Idle" then
        prefix = self.idleStateSfxPrefix or ""
        legacy = self.idleStateSfx or ""
        variations = tonumber(self.idleStateSfxVariations) or 1
    elseif state == "Chase" then
        prefix = self.chaseStateSfxPrefix or ""
        legacy = self.chaseStateSfx or ""
        variations = tonumber(self.chaseStateSfxVariations) or 1
    elseif state == "Death" then
        prefix = self.deathStateSfxPrefix or ""
        legacy = self.deathStateSfx or ""
        variations = tonumber(self.deathStateSfxVariations) or 1
    end

    if prefix ~= "" then
        variations = math.max(variations, 1)
        if variations <= 1 then
            return prefix
        end
        return prefix .. tostring(math.random(1, variations))
    end

    return legacy
end

local function playStateSfx(self, state)
    local sfx = stateSfxId(self, state)
    if sfx ~= nil and sfx ~= "" then
        Audio.playOneShot(sfx)
    end
end

local function setState(self, entity, newState)
    if self._state == newState then
        return false
    end

    self._state = newState
    playStateSfx(self, newState)
    return true
end

local function killPlayer(player)
    local isDead = World.getScriptProp(player.id, "_isDead")
    local invincible = World.getScriptProp(player.id, "_invincible")
    if isDead or invincible then
        return
    end

    World.setScriptProp(player.id, "_isDead", true)
    if player:hasSpriteAnimator() and player:getCurrentClip() ~= "death" then
        player:playAnimation("death")
        player:setColliderSolid(false)
    end
    Engine.triggerGameOver()
end

local Moth = {}

function Moth:onStart(entity)
    if not self._initialized then
        self._initialized = true
        self._dashTimer = 0.0
        self._dashDirX = 0.0
        self._dashDirY = 0.0
    end
    -- Build navigation grid from wall geometry (see enemy_ai.lua).
    _grid      = buildGrid()
    _navVersion = (World.navVersion and World.navVersion()) or 0
    _path      = nil
    _pathIdx   = 1
    _pathTimer = 0.0
end

function Moth:onUpdate(entity, dt)
    if not entity:isValid() then
        return
    end

    -- DEATH
    if self._state == "Death" then
        entity.velocity = Vec2(0.0, 0.0)
        entity.moveSpeed = 0.0

        if not self._colliderDisabled then
            self._colliderDisabled = true
            entity:setColliderSolid(false)
        end

        if not self._deathSfxPlayed then
            self._deathSfxPlayed = true
            if (self.deathStateSfxPrefix == nil or self.deathStateSfxPrefix == "") and
               (self.deathStateSfx == nil or self.deathStateSfx == "") then
                Audio.playOneShot("enemy_death_" .. math.random(1, 3))
            end
        end

        self._alpha = math.max(0.0, self._alpha - 2.0 * dt)
        entity:setSpriteAlpha(self._alpha)
        return
    end

    -- ATTACK: stop, play attack animation, then dash
    if self._state == "Attack" then
        entity.velocity = Vec2(0.0, 0.0)
        entity.moveSpeed = 0.0

        if entity:hasSpriteAnimator() then
            if entity:getCurrentClip() ~= "attack" then
                entity:playAnimation("attack")
            end

            if not entity:isAnimationPlaying() and entity:getCurrentClip() == "attack" then
                self._dashTimer = self.dashDuration
                self._state = "Dash"
            end
        else
            self._dashTimer = self.dashDuration
            self._state = "Dash"
        end

        return
    end

    -- DASH: normalized direction + moveSpeed
    if self._state == "Dash" then
        self._dashTimer = self._dashTimer - dt

        if self._dashTimer > 0.0 then
            entity.velocity = Vec2(self._dashDirX, self._dashDirY)
            entity.moveSpeed = self.dashSpeed
        else
            entity.velocity = Vec2(0.0, 0.0)
            entity.moveSpeed = 0.0
            self._state = "Chase"
        end

        return
    end

    -- Find player
    local player = World.findByTag("Player")
    local playerFound = player ~= nil and player:isValid()
    local px, py = 0.0, 0.0

    if playerFound then
        if player:getCurrentClip() == "death" then
            playerFound = false
        else
            px, py = player.position.x, player.position.y
        end
    end

    local ex, ey = entity.position.x, entity.position.y

    -- Check light cones
    local inLight = false
    World.eachLight(function(t)
        if inLight or not t:isValid() then return end
        if not t:isLightActive() then return end
        if t:hasBattery() and t:getBatteryCharge() <= 0.0 then return end

        local rot = t.rotation
        local c, s = math.cos(rot), math.sin(rot)
        local ox, oy = t:getLightOffsetX(), t:getLightOffsetY()
        local tx = t.position.x + ox * c - oy * s
        local ty = t.position.y + ox * s + oy * c

        if inCone(ex, ey, tx, ty, rot, t:getLightRadius(), t:getLightAngle()) then
            inLight = true
        end
    end)

    -- IDLE
    if self._state == "Idle" then
        entity.velocity = Vec2(0.0, 0.0)
        entity.moveSpeed = 0.0

        if entity:hasSpriteAnimator() and entity:getCurrentClip() ~= "idle" then
            entity:playAnimation("idle")
        end

        if inLight and playerFound then
            setState(self, entity, "Chase")
        end

    -- CHASE
    elseif self._state == "Chase" then
        if not inLight or not playerFound then
            self._state = "Idle"
            entity.velocity = Vec2(0.0, 0.0)
            entity.moveSpeed = 0.0
            return
        end

        local dxp = px - ex
        local dyp = py - ey
        local dist = math.sqrt(dxp * dxp + dyp * dyp)

        if dist <= self.attackRange then
            self._dashDirX = dxp / (dist > 0.0001 and dist or 1.0)
            self._dashDirY = dyp / (dist > 0.0001 and dist or 1.0)
            self._state = "Attack"
            entity.velocity = Vec2(0.0, 0.0)
            entity.moveSpeed = 0.0

            if entity:hasSpriteAnimator() then
                entity:playAnimation("attack")
            end
            return
        end

        -- Follow an A* path around walls instead of steering straight at the
        -- player; recalc periodically and fall back to direct steering when no
        -- path is found (e.g. player unreachable within the iteration budget).
        _pathTimer = _pathTimer - dt
        if _pathTimer <= 0.0 or _path == nil then
            _pathTimer = self.pathInterval
            -- Rebuild the grid if a door opened / an obstacle changed since we
            -- last built it (cheap version check; rebuild only on real change).
            local nv = (World.navVersion and World.navVersion()) or 0
            if nv ~= _navVersion then
                _grid = buildGrid()
                _navVersion = nv
            end
            _path      = findPath(_grid, ex, ey, px, py)
            _pathIdx   = 2 -- index 1 is the start cell; begin from 2
        end

        local dx, dy
        if _path and _pathIdx <= #_path then
            local wp = _path[_pathIdx]
            local wdx, wdy = wp.x - ex, wp.y - ey
            if wdx * wdx + wdy * wdy < 50.0 * 50.0 then
                _pathIdx = _pathIdx + 1
            end
            local target = _path[_pathIdx]
            if target then
                dx, dy = norm(target.x - ex, target.y - ey)
            else
                dx, dy = norm(dxp, dyp)
            end
        else
            dx, dy = norm(dxp, dyp)
        end

        entity.velocity = Vec2(dx, dy)
        entity.moveSpeed = self.chaseSpeed

        local sx = entity.scale.x
        if dx > 0.1 and sx < 0.0 then
            entity.scale = Vec2(-sx, entity.scale.y)
        elseif dx < -0.1 and sx > 0.0 then
            entity.scale = Vec2(-sx, entity.scale.y)
        end

        if entity:hasSpriteAnimator() and entity:getCurrentClip() ~= "walk" then
            entity:playAnimation("walk")
        end
    else
        entity.velocity = Vec2(0.0, 0.0)
        entity.moveSpeed = 0.0
    end
end

function Moth:onCollisionEnter(entity, other)
    if self._state == "Death" then return end
    if not other:isValid() then return end

    if other:hasPlayerController() then
        if self._state ~= "Attack" and self._state ~= "Dash" then
            local ex, ey = entity.position.x, entity.position.y
            local px, py = other.position.x, other.position.y
            self._dashDirX, self._dashDirY = norm(px - ex, py - ey)
            self._state = "Attack"
            entity.velocity = Vec2(0.0, 0.0)
            entity.moveSpeed = 0.0
            if entity:hasSpriteAnimator() then
                entity:playAnimation("attack")
            end
        end

        killPlayer(other)
        return
    end

    -- Hit wall or obstacle: stop pushing.
    entity.velocity = Vec2(0.0, 0.0)
    entity.moveSpeed = 0.0

    if self._state == "Dash" then
        self._dashTimer = 0.0
        self._state = "Chase"
    elseif self._state == "Chase" then
        -- Bumped a wall mid-chase: force an immediate A* recalc so we reroute
        -- around it instead of abandoning the chase.
        _path      = nil
        _pathTimer = 0.0
    elseif self._state == "Attack" then
        self._state = "Idle"
    end
end

return Moth