Properties = {
    { name = "active",      type = "bool",   default = true,   label = "Active" },
    { name = "triggerTag",  type = "string", default = "Player", label = "Trigger Tag (empty = any)" },
    { name = "actionType",  type = "string", default = "None", label = "Action Type" },
    { name = "payload",     type = "string", default = "",     label = "Payload" },
    { name = "targetTag",   type = "string", default = "",     label = "Target Text Tag" },
    { name = "setText",     type = "bool",   default = false,  label = "Set Text" },
    { name = "textValue",   type = "string", default = "",     label = "Text Value" },
    { name = "setVisible",  type = "bool",   default = false,  label = "Set Visible" },
    { name = "visibleValue",type = "bool",   default = true,   label = "Visible Value" },
    { name = "restoreVisibilityAfterDelay", type = "bool", default = false, label = "Restore Visibility After Delay" },
    { name = "visibilityRestoreDelay", type = "float", default = 1.0, label = "Visibility Restore Delay" },
    { name = "setFontSize", type = "bool",   default = false,  label = "Set Font Size" },
    { name = "fontSizeValue", type = "float", default = 1.0,   label = "Font Size Value" },
    { name = "oneShot",     type = "bool",   default = false,  label = "One Shot" },
    { name = "triggered",   type = "bool",   default = false,  label = "Triggered" },
}

-- Reset transient/runtime-only fields each time the script instance starts.
-- This prevents stale trigger/timer state from carrying across editor play sessions.
function onStart(self, entity)
    self.triggered = false
    self._visibilityRestoreRemaining = nil
    self._originalTextVisible = nil
end

local function isAllowedTrigger(self, other)
    if other == nil or not other:isValid() then
        return false
    end

    local requiredTag = self.triggerTag or "Player"
    if requiredTag == "" then
        return true
    end

    if not other:hasTag() then
        return false
    end

    return other:getTagValue() == requiredTag
end

local function findTargetTextEntity(self)
    if self.targetTag == nil or self.targetTag == "" then
        return nil
    end

    local target = World.findByTag(self.targetTag)
    if target == nil or not target:isValid() or not target:hasTextRenderer() then
        return nil
    end

    return target
end

local function applyTextRendererChange(self)
    local target = findTargetTextEntity(self)
    if target == nil then
        return
    end

    if self.setText then
        target:setText(self.textValue or "")
    end
    if self.setVisible then
        if self.restoreVisibilityAfterDelay then
            if self._visibilityRestoreRemaining == nil or self._visibilityRestoreRemaining <= 0 then
                self._originalTextVisible = target:isTextVisible()
            end
            self._visibilityRestoreRemaining = math.max(self.visibilityRestoreDelay or 0.0, 0.0)
        else
            self._visibilityRestoreRemaining = nil
            self._originalTextVisible = nil
        end
        target:setTextVisible(self.visibleValue == true)
    end
    if self.setFontSize then
        target:setTextFontSize(self.fontSizeValue or 1.0)
    end
end

-- onTriggerEnter is called by the C++ collision system when a trigger fires.
function onTriggerEnter(self, entity, other)
    if not self.active then return end
    if not isAllowedTrigger(self, other) then return end
    if self.oneShot and self.triggered then return end
    self.triggered = true
    if self.actionType == "PlayAudio" then
        Audio.play(self.payload)
    elseif self.actionType == "ChangeScene" then
        if self.payload ~= nil and self.payload ~= "" then
            World.loadLevel(self.payload)
        end
    elseif self.actionType == "ResetFragments" then
        World.resetFragments()
    elseif self.actionType == "ChangeTextRenderer" then
        applyTextRendererChange(self)
    end
end

function onUpdate(self, entity, dt)
    if self._visibilityRestoreRemaining == nil then
        return
    end

    self._visibilityRestoreRemaining = self._visibilityRestoreRemaining - dt
    if self._visibilityRestoreRemaining > 0 then
        return
    end

    local target = findTargetTextEntity(self)
    if target ~= nil and self._originalTextVisible ~= nil then
        target:setTextVisible(self._originalTextVisible == true)
    end

    self._visibilityRestoreRemaining = nil
    self._originalTextVisible = nil
end

return {
    onStart        = onStart,
    onTriggerEnter = onTriggerEnter,
    onUpdate       = onUpdate,
}
