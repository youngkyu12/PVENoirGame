local MELEE_RANGE     = 7.0
local MELEE_COOLDOWN  = 2.0
local SPELL_COOLDOWN  = 3.5
local GLOBAL_COOLDOWN = 0.8
local MOVE_SPEED      = 6.0

local threshold_75_done = false
local threshold_50_done = false
local threshold_25_done = false
local pending_calls = 0

function update(dt)
    local hp     = boss_get_hp()
    local max_hp = boss_get_max_hp()

    if not threshold_75_done and hp * 4 <= max_hp * 3 then
        threshold_75_done = true
        pending_calls = pending_calls + 1
        boss_log("Call threshold 75% triggered, pending=" .. pending_calls)
    end
    if not threshold_50_done and hp * 4 <= max_hp * 2 then
        threshold_50_done = true
        pending_calls = pending_calls + 1
        boss_log("Call threshold 50% triggered, pending=" .. pending_calls)
    end
    if not threshold_25_done and hp * 4 <= max_hp * 1 then
        threshold_25_done = true
        pending_calls = pending_calls + 1
        boss_log("Call threshold 25% triggered, pending=" .. pending_calls)
    end

    if boss_is_calling() then return end

    local pid = boss_nearest_player()
    if pid < 0 then return end

    local dist = boss_distance_to_player(pid)
    boss_face_player(pid)

    if boss_get_cooldown("global") > 0 then return end

    if pending_calls > 0 then
        pending_calls = pending_calls - 1
        boss_start_action("Call")
        boss_set_cooldown("global", GLOBAL_COOLDOWN)
        boss_log("Call triggered, remaining=" .. pending_calls)
        return
    end

    if dist <= MELEE_RANGE and boss_get_cooldown("melee") <= 0 then
        boss_start_action("Melee")
        boss_set_cooldown("melee",  MELEE_COOLDOWN)
        boss_set_cooldown("global", GLOBAL_COOLDOWN)
        boss_log("Melee -> player " .. pid)
        return
    end

    if dist > MELEE_RANGE and boss_get_cooldown("spell") <= 0 then
        boss_start_action("Spell")
        boss_set_cooldown("spell",  SPELL_COOLDOWN)
        boss_set_cooldown("global", GLOBAL_COOLDOWN)
        boss_log("Spell -> player " .. pid)
        return
    end

    if dist > MELEE_RANGE then
        boss_move_towards_player(pid, MOVE_SPEED, dt)
    end
end
