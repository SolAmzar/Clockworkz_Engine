-- *****************************************************************************
-- file           cheat_manager.lua
-- project        Lights Out
-- brief          allow cheat input to register
-- author         [Syafiq] 
-- call CheatManager.update(dt) every frame from a game-manager entity's onUpdate
-- *****************************************************************************

local CheatManager = {}

-- Human-readable input names for keyboard key
-- Different key than normal controls to not allow interference when playin giwht regular
local keyMap = {
    Up     = Keys.UP,
    Down   = Keys.DOWN,
    Left   = Keys.LEFT,
    Right  = Keys.RIGHT,
    A      = Keys.Z,
    B      = Keys.X,
    X      = Keys.C,
    Y      = Keys.V,
    L1     = Keys.Q,
    R1     = Keys.E,
    Num1   = Keys.NUM_1,
    Num2   = Keys.NUM_2,
    Num3   = Keys.NUM_3,
    Num4   = Keys.NUM_4,
    Num5   = Keys.NUM_5,
    Num6   = Keys.NUM_6,
    Select = Keys.BACKSPACE,
    Start  = Keys.BACKSLASH,
}

-- Human-readable input names for  gamepad named actions
local padMap = {
    Up     = "dpad_up",
    Down   = "dpad_down",
    Left   = "dpad_left",
    Right  = "dpad_right",
    A      = "btn_a",
    B      = "btn_b",
    X      = "btn_x",
    Y      = "btn_y",
    L1     = "lb",
    R1     = "rb",
    L2     = "lt",
    Select = "select",
    Start  = "start",
}

local BUFFER_MAX    = 32
local INPUT_TIMEOUT = 1.0   

local m_cheats = {}   
local m_buffer = {}   
local m_timer  = 0.0 

-- Register a cheat sequence.
function CheatManager.register(sequence, callback, name)
    table.insert(m_cheats, {
        sequence = sequence,
        callback = callback,
        name     = name or "",
    })
end

-- Clear the input buffer and reset the timeout timer.
function CheatManager.reset()
    m_buffer = {}
    m_timer  = 0.0
end

function CheatManager.update(dt)
    local input = Engine.input()

    -- Detect one new input this frame
    local detected = nil

    for name, key in pairs(keyMap) do
        if input:wasKeyPressed(key) then
            detected = name
            break
        end
    end

    if not detected then
        for name, action in pairs(padMap) do
            if Gamepad:isActionPressed(action) then
                detected = name
                break
            end
        end
    end

    if detected then
        m_timer = 0.0

        if #m_buffer >= BUFFER_MAX then
            table.remove(m_buffer, 1)
        end
        table.insert(m_buffer, detected)

        for _, cheat in ipairs(m_cheats) do
            local seq    = cheat.sequence
            local seqLen = #seq
            if #m_buffer >= seqLen then
                local match  = true
                local offset = #m_buffer - seqLen
                for i = 1, seqLen do
                    if m_buffer[offset + i] ~= seq[i] then
                        match = false
                        break
                    end
                end
                if match then
                    cheat.callback()
                    m_buffer = {}
                    m_timer  = 0.0
                    break
                end
            end
        end
    else
        -- No input this frame
        m_timer = m_timer + dt
        if m_timer >= INPUT_TIMEOUT then
            m_buffer = {}
            m_timer  = 0.0
        end
    end
end

return CheatManager
