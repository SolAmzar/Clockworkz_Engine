#!/usr/bin/env python3
"""
migrate_scenes.py
Migrates LightsOut scene JSON files to add LuaScript entries for game-specific
C++ components (battery, fragment, door, etc.) so the engine can drive them
from Lua scripts instead of C++ structs.

JSON structure: objects[] -> { "components": { "luaScript": [...], "battery": {...}, ... } }
"""
import json
import os

SCENES_DIR = r"C:\Users\amiru\source\repos\csd2401f25_team_clockworkz\ClockWorkzEngine\game\projects\LightsOut\assets\scenes"

ACTIVATION_ZONE_LUA_EXISTS = True  # verified

def has_script(lua_scripts, path):
    if not lua_scripts:
        return False
    for entry in lua_scripts:
        if isinstance(entry, dict) and entry.get("scriptPath") == path:
            return True
    return False

def add_script(lua_scripts, path, properties):
    entry = {"scriptPath": path, "enabled": True}
    if properties:
        entry["properties"] = properties
    lua_scripts.append(entry)

def migrate_entity(obj, changes):
    comps = obj.get("components", {})
    if not isinstance(comps, dict):
        return False

    # luaScript is a component inside "components"
    lua_script = comps.get("luaScript")
    if lua_script is None:
        lua_script = []
    elif isinstance(lua_script, dict):
        lua_script = [lua_script]
    elif not isinstance(lua_script, list):
        lua_script = []

    entity_name = obj.get("name", "?")
    modified = False

    # --- battery ---
    if "battery" in comps and not has_script(lua_script, "components/battery.lua"):
        b = comps["battery"]
        props = {
            "charge":     float(b.get("charge", 100.0)),
            "maxCharge":  float(b.get("maxCharge", 100.0)),
            "drainRate":  float(b.get("drainRate", 5.0)),
            "chargeRate": float(b.get("chargeRate", 20.0)),
            "isCharging": bool(b.get("isCharging", False)),
        }
        add_script(lua_script, "components/battery.lua", props)
        changes.append(f"  + components/battery.lua -> '{entity_name}'")
        modified = True

    # --- fragmentCollector ---
    if "fragmentCollector" in comps and not has_script(lua_script, "components/fragment_collector.lua"):
        fc = comps["fragmentCollector"]
        props = {
            "requiredFragments": int(fc.get("requiredFragments", 1)),
            "collectedCount":    int(fc.get("collectedCount", 0)),
            "pickupRadius":      float(fc.get("pickupRadius", 50.0)),
        }
        add_script(lua_script, "components/fragment_collector.lua", props)
        changes.append(f"  + components/fragment_collector.lua -> '{entity_name}'")
        modified = True

    # --- health ---
    if "health" in comps and not has_script(lua_script, "components/health.lua"):
        h = comps["health"]
        props = {
            "hp":           float(h.get("hp", 10.0)),
            "maxHp":        float(h.get("maxHp", 10.0)),
            "regenRate":    float(h.get("regenRate", 0.0)),
            "invulnerable": bool(h.get("invulnerable", False)),
            "isDead":       bool(h.get("isDead", False)),
        }
        add_script(lua_script, "components/health.lua", props)
        changes.append(f"  + components/health.lua -> '{entity_name}'")
        modified = True

    # --- enemyAI ---
    if "enemyAI" in comps and not has_script(lua_script, "components/enemy_ai.lua"):
        ai = comps["enemyAI"]
        props = {
            "detectionRadius":  float(ai.get("detectionRadius", 300.0)),
            "chaseSpeed":       float(ai.get("chaseSpeed", 100.0)),
            "patrolSpeed":      float(ai.get("patrolSpeed", 50.0)),
            "state":            "Patrol",
            "alertDuration":    3.0,
            "lightSensitivity": 0.3,
            "wanderInterval":   2.0,
            "_stateTimer":      0.0,
            "_wanderTimer":     0.0,
        }
        add_script(lua_script, "components/enemy_ai.lua", props)
        changes.append(f"  + components/enemy_ai.lua -> '{entity_name}'")
        modified = True

    # --- lightExposure ---
    if "lightExposure" in comps and not has_script(lua_script, "components/light_exposure.lua"):
        le = comps["lightExposure"]
        props = {
            "maxExposureTime": float(le.get("maxExposureTime", 10.0)),
            "isDangerous":     bool(le.get("isDangerous", False)),
            "exposureTime":    float(le.get("exposureTime", 0.0)),
            "inLight":         bool(le.get("inLight", False)),
        }
        add_script(lua_script, "components/light_exposure.lua", props)
        changes.append(f"  + components/light_exposure.lua -> '{entity_name}'")
        modified = True

    # --- fragment ---
    if "fragment" in comps and not has_script(lua_script, "components/fragment.lua"):
        fr = comps["fragment"]
        frag_id = fr.get("fragmentId", 0)
        if isinstance(frag_id, str):
            parts = frag_id.split("_")
            try:
                frag_id = int(parts[-1])
            except ValueError:
                frag_id = 0
        props = {
            "fragmentId":    int(frag_id),
            "collected":     bool(fr.get("collected", False)),
            "requiresLight": bool(fr.get("requiresLight", True)),
            "bobSpeed":      float(fr.get("bobSpeed", 2.0)),
            "bobAmount":     float(fr.get("bobAmount", 10.0)),
            "fadeSpeed":     float(fr.get("fadeSpeed", 5.0)),
            "_baseY":        0.0,
            "_timer":        0.0,
            "_alpha":        1.0,
            "_initialized":  False,
        }
        add_script(lua_script, "components/fragment.lua", props)
        changes.append(f"  + components/fragment.lua -> '{entity_name}'")
        modified = True
        # Rename entity to "Fragment" if not already
        if entity_name != "Fragment":
            obj["name"] = "Fragment"
            changes.append(f"  ~ Renamed '{entity_name}' -> 'Fragment'")

    # --- portal ---
    if "portal" in comps and not has_script(lua_script, "components/portal.lua"):
        po = comps["portal"]
        props = {
            "levelName": str(po.get("levelName", "")),
        }
        add_script(lua_script, "components/portal.lua", props)
        changes.append(f"  + components/portal.lua -> '{entity_name}'")
        modified = True
        # Ensure entity has a trigger collider
        if "collider" not in comps:
            comps["collider"] = {"isSolid": False, "isTrigger": True}
            changes.append(f"  + collider(trigger) added for portal entity '{entity_name}'")

    # --- door ---
    if "door" in comps and not has_script(lua_script, "components/door.lua"):
        d = comps["door"]
        props = {
            "requiredFragments": int(d.get("requiredFragments", 1)),
            "isOpen":            bool(d.get("isOpen", False)),
            "openTexture":       str(d.get("openTexture", "door_open")),
            "closedTexture":     str(d.get("closedTexture", "door_close")),
        }
        add_script(lua_script, "components/door.lua", props)
        changes.append(f"  + components/door.lua -> '{entity_name}'")
        modified = True

    # --- doorSwing ---
    if "doorSwing" in comps and not has_script(lua_script, "components/door_swing.lua"):
        ds = comps["doorSwing"]
        props = {
            "isOpen":          bool(ds.get("isOpen", False)),
            "swingAngle":      float(ds.get("swingAngle", 90.0)),
            "clockwise":       bool(ds.get("clockwise", True)),
            "speed":           float(ds.get("speed", 90.0)),
            "initialRotation": float(ds.get("initialRotation", 0.0)),
            "_initialized":    bool(ds.get("initialized", False)),
        }
        add_script(lua_script, "components/door_swing.lua", props)
        changes.append(f"  + components/door_swing.lua -> '{entity_name}'")
        modified = True

    # --- playerController ---
    if "playerController" in comps and not has_script(lua_script, "player_controller.lua"):
        add_script(lua_script, "player_controller.lua", {})
        changes.append(f"  + player_controller.lua -> '{entity_name}'")
        modified = True

    # --- activationZone ---
    if "activationZone" in comps and ACTIVATION_ZONE_LUA_EXISTS and not has_script(lua_script, "components/activation_zone.lua"):
        add_script(lua_script, "components/activation_zone.lua", {})
        changes.append(f"  + components/activation_zone.lua -> '{entity_name}'")
        modified = True

    if modified:
        comps["luaScript"] = lua_script
        obj["components"] = comps

    return modified

def migrate_scene(filepath):
    with open(filepath, "r", encoding="utf-8") as f:
        try:
            data = json.load(f)
        except json.JSONDecodeError as e:
            print(f"  [SKIP] JSON parse error in {os.path.basename(filepath)}: {e}")
            return False

    objects = data.get("objects", [])
    if not objects:
        return False

    changes = []
    any_modified = False

    for obj in objects:
        if migrate_entity(obj, changes):
            any_modified = True

    if any_modified:
        with open(filepath, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
        print(f"\n[MIGRATED] {os.path.basename(filepath)}")
        for c in changes:
            print(c)

    return any_modified

def main():
    print("=== Scene JSON Migration ===\n")
    total_modified = 0
    for fname in sorted(os.listdir(SCENES_DIR)):
        if not fname.endswith(".json"):
            continue
        fpath = os.path.join(SCENES_DIR, fname)
        if migrate_scene(fpath):
            total_modified += 1

    print(f"\n=== Done. {total_modified} scene(s) modified. ===")

if __name__ == "__main__":
    main()
