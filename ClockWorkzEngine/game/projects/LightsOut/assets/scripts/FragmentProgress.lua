local FragmentProgress = _G.FragmentProgress or {
    currentGroupId = "",
    groups = {}
}

function FragmentProgress.ensureGroup(groupId, maxFragments)
    if not groupId or groupId == "" then
        return nil
    end

    local group = FragmentProgress.groups[groupId]
    if not group then
        group = {
            collectedIds = {},
            maxFragments = maxFragments or 0
        }
        FragmentProgress.groups[groupId] = group
    elseif maxFragments and maxFragments > group.maxFragments then
        group.maxFragments = maxFragments
    end

    return group
end

function FragmentProgress.setCurrentGroup(groupId, maxFragments)
    if groupId and groupId ~= "" then
        FragmentProgress.currentGroupId = groupId
        FragmentProgress.ensureGroup(groupId, maxFragments)
    else
        FragmentProgress.currentGroupId = ""
    end
end

function FragmentProgress.getCurrentGroupId()
    return FragmentProgress.currentGroupId
end

function FragmentProgress.markCollected(groupId, fragmentId)
    if not groupId or groupId == "" or not fragmentId or fragmentId == 0 then
        return false
    end

    local group = FragmentProgress.ensureGroup(groupId)
    if not group then
        return false
    end

    if group.collectedIds[fragmentId] then
        return false
    end

    group.collectedIds[fragmentId] = true
    return true
end

function FragmentProgress.isCollected(groupId, fragmentId)
    if not groupId or groupId == "" or not fragmentId or fragmentId == 0 then
        return false
    end

    local group = FragmentProgress.groups[groupId]
    return group ~= nil and group.collectedIds[fragmentId] == true
end

function FragmentProgress.getCollectedCount(groupId)
    local group = FragmentProgress.groups[groupId]
    if not group then
        return 0
    end

    local count = 0
    for _, collected in pairs(group.collectedIds) do
        if collected then
            count = count + 1
        end
    end
    return count
end

function FragmentProgress.getMaxCount(groupId)
    local group = FragmentProgress.groups[groupId]
    if not group then
        return 0
    end
    return group.maxFragments or 0
end

function FragmentProgress.resetGroup(groupId)
    if groupId and groupId ~= "" then
        FragmentProgress.groups[groupId] = nil
        if FragmentProgress.currentGroupId == groupId then
            FragmentProgress.currentGroupId = ""
        end
    end
end

function FragmentProgress.resetAll()
    FragmentProgress.currentGroupId = ""
    FragmentProgress.groups = {}
end

_G.FragmentProgress = FragmentProgress
return FragmentProgress
