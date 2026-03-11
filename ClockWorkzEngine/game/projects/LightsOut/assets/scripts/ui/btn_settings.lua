-- *****************************************************************************
-- file           btn_settings.lua
-- project        Lights Out
-- brief          UIButton script: pushes the settings state onto the stack.
--
-- All content (c) 2025 DigiPen Institute of Technology Singapore. All rights reserved.
-- *****************************************************************************

function on_click()
    GSM.push_settings()
end
