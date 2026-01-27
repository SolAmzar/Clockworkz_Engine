-- *****************************************************************************
-- file           test_binding.lua
-- project        Lights Out
-- author(s)      [Syafiq] 100%
-- brief          lua script for testing lua-sol2 binding 

--All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
--******************************************************************************

Log("=== Testing Lua Bindings ===")

-- Test vec2 creation and operations
Log("Testing vec2...")
local v1 = Vec2(10, 20)
Log("  Created vec2: " .. tostring(v1))
Log("  x = " .. v1.x .. ", y = " .. v1.y)

local v2 = Vec2(5, 5)
local sum = v1 + v2
Log("  v1 + v2 = " .. tostring(sum))

local scaled = v1 * 2
Log("  v1 * 2 = " .. tostring(scaled))

local len = v1:length()
Log("  v1:length() = " .. len)

local normalized = v1:normalized()
Log("  v1:normalized() = " .. tostring(normalized))

-- Test vec2 helper functions
local zero = vec2_zero()
local one = vec2_one()
local up = vec2_up()
Log("  vec2_zero() = " .. tostring(zero))
Log("  vec2_one() = " .. tostring(one))
Log("  vec2_up() = " .. tostring(up))

-- Test Clamp and Lerp
Log("Testing math helpers...")
local clamped = Clamp(15, 0, 10)
Log("  Clamp(15, 0, 10) = " .. clamped)

local lerped = Lerp(0, 100, 0.5)
Log("  Lerp(0, 100, 0.5) = " .. lerped)

-- Test Keys table
Log("Testing Keys table...")
Log("  Keys.W = " .. Keys.W)
Log("  Keys.A = " .. Keys.A)
Log("  Keys.S = " .. Keys.S)
Log("  Keys.D = " .. Keys.D)
Log("  Keys.SPACE = " .. Keys.SPACE)

-- Test Mouse table
Log("Testing Mouse table...")
Log("  Mouse.LEFT = " .. Mouse.LEFT)
Log("  Mouse.RIGHT = " .. Mouse.RIGHT)

-- Test GameObjectType enum
Log("Testing GameObjectType enum...")
Log("  GameObjectType.Player = " .. tostring(GameObjectType.Player))
Log("  GameObjectType.Enemy = " .. tostring(GameObjectType.Enemy))
Log("  GameObjectType.Wall = " .. tostring(GameObjectType.Wall))

Log("=== All binding tests passed! ===")
