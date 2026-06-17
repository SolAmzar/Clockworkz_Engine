-------------------------------------------------------------------------------
-- enemy_ai.lua
-- Normal enemy: patrols, chases player using A* pathfinding, dies in light.
-------------------------------------------------------------------------------

Properties = {
    { name = "state",                    type = "string", default = "Patrol", label = "Current State" },
    { name = "maxHp",                    type = "float",  default = 100.0,    label = "Max HP" },
    { name = "detectionRadius",          type = "float",  default = 300.0,    label = "Detection Radius" },
    { name = "chaseSpeed",               type = "float",  default = 150.0,    label = "Chase Speed" },
    { name = "patrolSpeed",              type = "float",  default = 50.0,     label = "Patrol Speed" },
    { name = "alertDuration",            type = "float",  default = 3.0,      label = "Alert Duration" },
    { name = "lightSensitivity",         type = "float",  default = 0.3,      label = "Light Sensitivity (speed mult in light)" },
    { name = "wanderInterval",           type = "float",  default = 2.0,      label = "Wander Interval" },
    { name = "pathInterval",             type = "float",  default = 0.5,      label = "Path Recalc Interval (s)" },

    { name = "patrolStateSfxVariations", type = "int",    default = 1,        label = "Patrol SFX Variations" },
    { name = "chaseStateSfxVariations",  type = "int",    default = 1,        label = "Chase SFX Variations" },
    { name = "alertStateSfxVariations",  type = "int",    default = 1,        label = "Alert SFX Variations" },
    { name = "attackStateSfxVariations", type = "int",    default = 1,        label = "Attack SFX Variations" },
    { name = "deathStateSfxVariations",  type = "int",    default = 1,        label = "Death SFX Variations" },

    { name = "patrolStateSfxPrefix",     type = "string", default = "",       label = "Patrol SFX Prefix" },
    { name = "chaseStateSfxPrefix",      type = "string", default = "",       label = "Chase SFX Prefix" },
    { name = "alertStateSfxPrefix",      type = "string", default = "",       label = "Alert SFX Prefix" },
    { name = "attackStateSfxPrefix",     type = "string", default = "",       label = "Attack SFX Prefix" },
    { name = "deathStateSfxPrefix",      type = "string", default = "",       label = "Death SFX Prefix" },

    { name = "patrolStateSfx",           type = "string", default = "",       label = "" },
    { name = "chaseStateSfx",            type = "string", default = "",       label = "" },
    { name = "alertStateSfx",            type = "string", default = "",       label = "" },
    { name = "attackStateSfx",           type = "string", default = "",       label = "" },
    { name = "deathStateSfx",            type = "string", default = "",       label = "" },

    { name = "stateSfxVolume",           type = "float",  default = 1.0,      label = "State SFX Volume" },
    { name = "usePositionalStateSfx",    type = "bool",   default = true,     label = "Use Positional State SFX" },

    -- Internal
    { name = "_hp",                      type = "float",  default = 100.0,    label = "" },
    { name = "_stateTimer",              type = "float",  default = 0.0,      label = "" },
    { name = "_wanderTimer",             type = "float",  default = 0.0,      label = "" },
    { name = "_wanderDirX",              type = "float",  default = 1.0,      label = "" },
    { name = "_wanderDirY",              type = "float",  default = 0.0,      label = "" },
    { name = "_targetX",                 type = "float",  default = 0.0,      label = "" },
    { name = "_targetY",                 type = "float",  default = 0.0,      label = "" },
    { name = "_alpha",                   type = "float",  default = 1.0,      label = "" },
    { name = "_deathSfxPlayed",          type = "bool",   default = false,    label = "" },
    { name = "_colliderDisabled",        type = "bool",   default = false,    label = "" },
    { name = "_initialized",             type = "bool",   default = false,    label = "" },
}

local Enemy = {}

-------------------------------------------------------------------------------
-- A* pathfinding state (per enemy instance via Lua closure)
-------------------------------------------------------------------------------
local CELL_SIZE  = 100.0   -- matches wall tile world size
local _grid      = nil     -- sparse blocked-cell table built at onStart
local _path      = nil     -- array of {x,y} waypoints, nil = no path
local _pathIdx   = 1       -- index of next waypoint to head toward
local _pathTimer = 0.0     -- seconds until next recalculation

-- Walls sit at centres 50, 150, 250 … 650 (every 100 units, offset +50).
-- Floor-dividing maps each 100-unit band [n*100, n*100+100) to cell n,
-- so a wall centred at 650 → cell 6, occupying world [600, 700). No rounding.
local function worldToCell(x, y)
    return math.floor(x / CELL_SIZE), math.floor(y / CELL_SIZE)
end

local function isBlocked(grid, cx, cy)
    return grid[cx] ~= nil and grid[cx][cy] == true
end

-- Build a sparse set of blocked cells from wall entities.
-- Relies on World.eachWall added in ScriptSystem.cpp.
local function buildGrid()
    local grid = {}
    World.eachWall(function(e)
        if not e:isValid() then return end
        local pos = e.position
        local cx, cy = worldToCell(pos.x, pos.y)
        if not grid[cx] then grid[cx] = {} end
        grid[cx][cy] = true
    end)
    return grid
end

-- Octile distance heuristic (admissible for 8-direction movement).
local function heuristic(ax, ay, bx, by)
    local dx = math.abs(ax - bx)
    local dy = math.abs(ay - by)
    return dx + dy + (1.414 - 2.0) * math.min(dx, dy)
end

-- Run A* from (startX,startY) to (goalX,goalY) in world coordinates.
-- Returns an array of {x,y} waypoints (world coords), or nil if no path.
local function findPath(grid, startX, startY, goalX, goalY)
    local sx, sy = worldToCell(startX, startY)
    local gx, gy = worldToCell(goalX, goalY)

    if sx == gx and sy == gy then return {} end
    if isBlocked(grid, gx, gy) then return nil end

    local function cellKey(cx, cy) return cx .. "," .. cy end

    -- 8-directional movement: {dx, dy, cost}
    local dirs = {
        {1,0,1.0}, {-1,0,1.0}, {0,1,1.0}, {0,-1,1.0},
        {1,1,1.414}, {1,-1,1.414}, {-1,1,1.414}, {-1,-1,1.414},
    }

    local startKey = cellKey(sx, sy)
    local open     = {{ cx=sx, cy=sy, g=0.0, f=heuristic(sx,sy,gx,gy), parent=nil }}
    local gScore   = { [startKey] = 0.0 }
    local closed   = {}

    local MAX_ITER = 400  -- cap to prevent frame spike on unsolvable maps
    local iter = 0

    while #open > 0 and iter < MAX_ITER do
        iter = iter + 1

        -- Linear scan for lowest f (sufficient for small game levels)
        local bestIdx = 1
        for i = 2, #open do
            if open[i].f < open[bestIdx].f then bestIdx = i end
        end
        local cur = table.remove(open, bestIdx)
        local curKey = cellKey(cur.cx, cur.cy)

        if not closed[curKey] then
            closed[curKey] = true

            if cur.cx == gx and cur.cy == gy then
                -- Reconstruct path in forward order
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
                -- Prevent cutting corners diagonally through walls
                if passable and diagonal then
                    passable = not isBlocked(grid, cur.cx + dir[1], cur.cy)
                           and not isBlocked(grid, cur.cx, cur.cy + dir[2])
                end

                if passable then
                    local newG = cur.g + dir[3]
                    if not gScore[nKey] or newG < gScore[nKey] then
                        gScore[nKey] = newG
                        table.insert(open, {
                            cx     = nx,
                            cy     = ny,
                            g      = newG,
                            f      = newG + heuristic(nx, ny, gx, gy),
                            parent = cur,
                        })
                    end
                end
            end
        end
    end

    return nil  -- no path found within iteration budget
end

-------------------------------------------------------------------------------
-- Shared helpers
-------------------------------------------------------------------------------
local function dist2(ax, ay, bx, by)
    local dx, dy = bx - ax, by - ay
    return dx * dx + dy * dy
end

local function norm(dx, dy)
    local len = math.sqrt(dx * dx + dy * dy)
    if len < 0.0001 then return 0.0, 0.0 end
    return dx / len, dy / len
end

local function inCone(px, py, tx, ty, rot, radius, fovDeg)
    local dx, dy = px - tx, py - ty
    if dx * dx + dy * dy > radius * radius then return false end
    local dist = math.sqrt(dx * dx + dy * dy)
    if dist < 0.0001 then return true end
    local dot = math.cos(rot) * (dx / dist) + math.sin(rot) * (dy / dist)
    return dot >= math.cos((fovDeg * math.pi / 180.0) * 0.5)
end

local function stateSfxId(self, state)
    local prefix, legacy, variations = "", "", 1
    if state == "Patrol" then
        prefix = self.patrolStateSfxPrefix or ""; legacy = self.patrolStateSfx or ""
        variations = tonumber(self.patrolStateSfxVariations) or 1
    elseif state == "Chase" then
        prefix = self.chaseStateSfxPrefix or ""; legacy = self.chaseStateSfx or ""
        variations = tonumber(self.chaseStateSfxVariations) or 1
    elseif state == "Alert" then
        prefix = self.alertStateSfxPrefix or ""; legacy = self.alertStateSfx or ""
        variations = tonumber(self.alertStateSfxVariations) or 1
    elseif state == "Attack" then
        prefix = self.attackStateSfxPrefix or ""; legacy = self.attackStateSfx or ""
        variations = tonumber(self.attackStateSfxVariations) or 1
    elseif state == "Death" then
        prefix = self.deathStateSfxPrefix or ""; legacy = self.deathStateSfx or ""
        variations = tonumber(self.deathStateSfxVariations) or 1
    end
    if prefix ~= "" then
        variations = math.max(variations, 1)
        return prefix .. tostring(math.random(1, variations))
    end
    return legacy
end

local function playStateSfx(self, entity, state)
    local sfx = stateSfxId(self, state)
    if sfx ~= nil and sfx ~= "" then
        local volume = tonumber(self.stateSfxVolume) or 1.0
        if self.usePositionalStateSfx then
            Audio.playOneShotAt(sfx, entity.position.x, entity.position.y, volume)
        else
            Audio.playOneShot(sfx)
        end
    end
end

local function setState(self, entity, newState)
    if self.state == newState then return false end
    -- Clear path when leaving Chase so it recalculates fresh on re-entry
    if newState ~= "Chase" then
        _path = nil
        _pathTimer = 0.0
    end
    self.state = newState
    playStateSfx(self, entity, newState)
    return true
end

local function chooseRandomPatrolDir(self)
    local angle = math.random() * math.pi * 2.0
    self._wanderDirX = math.cos(angle)
    self._wanderDirY = math.sin(angle)
    self._wanderTimer = self.wanderInterval
end

-------------------------------------------------------------------------------
-- Lifecycle
-------------------------------------------------------------------------------
function Enemy:onStart(entity)
    if not self._initialized then
        self._hp             = self.maxHp
        self._alpha          = 1.0
        self._deathSfxPlayed = false
        self._colliderDisabled = false
        self._wanderTimer    = 0.0
        self._initialized    = true
    end
    -- Build navigation grid from wall geometry
    _grid      = buildGrid()
    _path      = nil
    _pathIdx   = 1
    _pathTimer = 0.0
    -- DEBUG: count blocked cells so we can verify grid is populated
    local _dbgCells = 0
    for _, col in pairs(_grid) do
        for _ in pairs(col) do _dbgCells = _dbgCells + 1 end
    end
    print("[EnemyAI] Grid built — blocked cells: " .. _dbgCells)
end

function Enemy:onUpdate(entity, dt)
    if not entity:isValid() then return end

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

    -- Light detection
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

    local speedMul = 1.0
    if inLight then
        speedMul = self.lightSensitivity
        self._hp = self._hp - 20.0 * dt
        if self._hp <= 0.0 then
            self._hp = 0.0
            setState(self, entity, "Death")
        end
    end

    -- Death state
    if self.state == "Death" then
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
                local fallbackId = "enemy_death_" .. math.random(1, 3)
                local volume = tonumber(self.stateSfxVolume) or 1.0
                if self.usePositionalStateSfx then
                    Audio.playOneShotAt(fallbackId, entity.position.x, entity.position.y, volume)
                else
                    Audio.playOneShot(fallbackId)
                end
            end
        end
        self._alpha = math.max(0.0, self._alpha - 2.0 * dt)
        entity:setSpriteAlpha(self._alpha)
        return
    end

    if not playerFound and self.state == "Chase" then
        if setState(self, entity, "Patrol") then
            self._wanderTimer = 0.0
        end
        entity.velocity = Vec2(0.0, 0.0)
        entity.moveSpeed = 0.0
    end

    -- Patrol state
    if self.state == "Patrol" then
        if playerFound and dist2(ex, ey, px, py) < self.detectionRadius * self.detectionRadius then
            setState(self, entity, "Chase")
            return
        end
        self._wanderTimer = self._wanderTimer - dt
        if self._wanderTimer <= 0.0 then
            chooseRandomPatrolDir(self)
        end
        entity.velocity = Vec2(self._wanderDirX, self._wanderDirY)
        entity.moveSpeed = self.patrolSpeed * speedMul

    -- Chase state — A* pathfinding
    elseif self.state == "Chase" then
        if not playerFound then
            if setState(self, entity, "Alert") then
                self._stateTimer = self.alertDuration
                self._targetX = ex
                self._targetY = ey
            end
            entity.velocity = Vec2(0.0, 0.0)
            entity.moveSpeed = 0.0
            return
        end

        local threshold = self.detectionRadius * 1.5
        if dist2(ex, ey, px, py) > threshold * threshold then
            if setState(self, entity, "Alert") then
                self._stateTimer = self.alertDuration
                self._targetX = px
                self._targetY = py
            end
            entity.velocity = Vec2(0.0, 0.0)
            entity.moveSpeed = 0.0
            return
        end

        -- Recalculate path periodically
        _pathTimer = _pathTimer - dt
        if _pathTimer <= 0.0 or _path == nil then
            _pathTimer = self.pathInterval
            _path      = findPath(_grid, ex, ey, px, py)
            _pathIdx   = 2  -- index 1 is the start cell; begin from 2
            print("[EnemyAI] Path recalc — waypoints: " .. (_path and #_path or "nil") .. " | enemy(" .. math.floor(ex) .. "," .. math.floor(ey) .. ") player(" .. math.floor(px) .. "," .. math.floor(py) .. ")")
        end

        -- Steer toward current waypoint; fall back to direct line when no path
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
                dx, dy = norm(px - ex, py - ey)
            end
        else
            dx, dy = norm(px - ex, py - ey)
        end

        entity.velocity = Vec2(dx, dy)
        entity.moveSpeed = self.chaseSpeed * speedMul
        self._targetX = px
        self._targetY = py

    -- Alert state
    elseif self.state == "Alert" then
        if playerFound and dist2(ex, ey, px, py) < self.detectionRadius * self.detectionRadius then
            setState(self, entity, "Chase")
            return
        end
        self._stateTimer = self._stateTimer - dt
        if self._stateTimer <= 0.0 then
            if setState(self, entity, "Patrol") then
                self._wanderTimer = 0.0
            end
            entity.velocity = Vec2(0.0, 0.0)
            entity.moveSpeed = 0.0
            return
        end
        if dist2(ex, ey, self._targetX, self._targetY) > 20.0 * 20.0 then
            local dx, dy = norm(self._targetX - ex, self._targetY - ey)
            entity.velocity = Vec2(dx, dy)
            entity.moveSpeed = self.chaseSpeed * speedMul
        else
            entity.velocity = Vec2(0.0, 0.0)
            entity.moveSpeed = 0.0
        end
    else
        entity.velocity = Vec2(0.0, 0.0)
        entity.moveSpeed = 0.0
    end

    -- Animation
    local vx, vy = entity.velocity.x, entity.velocity.y
    if math.abs(vx) > 0.1 or math.abs(vy) > 0.1 then
        entity:playAnimation("walk")
        local sx = entity.scale.x
        if vx > 0.1 and sx < 0.0 then
            entity.scale = Vec2(-sx, entity.scale.y)
        elseif vx < -0.1 and sx > 0.0 then
            entity.scale = Vec2(-sx, entity.scale.y)
        end
    elseif entity:hasSpriteAnimator() and entity:getCurrentClip() ~= "idle" then
        entity:playAnimation("idle")
    end
end

function Enemy:onCollisionEnter(entity, other)
    if self.state == "Death" then return end
    if not other:isValid() then return end

    if other:hasPlayerController() then
        local attackSfx = stateSfxId(self, "Attack")
        if attackSfx ~= nil and attackSfx ~= "" then
            local volume = tonumber(self.stateSfxVolume) or 1.0
            if self.usePositionalStateSfx then
                Audio.playOneShotAt(attackSfx, entity.position.x, entity.position.y, volume)
            else
                Audio.playOneShot(attackSfx)
            end
        end
        local isDead     = World.getScriptProp(other.id, "_isDead")
        local invincible = World.getScriptProp(other.id, "_invincible")
        if not isDead and not invincible then
            World.setScriptProp(other.id, "_isDead", true)
            if other:hasSpriteAnimator() and other:getCurrentClip() ~= "death" then
                other:playAnimation("death")
                other:setColliderSolid(false)
            end
            Engine.triggerGameOver()
        end
        return
    end

    -- Hit a wall: A* should have routed around it, but force a recalculation
    -- in case the grid was stale or the enemy was physically pushed off-path.
    entity.velocity = Vec2(0.0, 0.0)
    entity.moveSpeed = 0.0

    if self.state == "Patrol" then
        chooseRandomPatrolDir(self)
    elseif self.state == "Chase" then
        _path      = nil
        _pathTimer = 0.0
    elseif self.state == "Alert" then
        self._stateTimer = 0.0
    end
end

return Enemy
