local MELEE_RANGE     = 7.0
local MELEE_COOLDOWN  = 2.0
local SPELL_COOLDOWN  = 3.5
local GLOBAL_COOLDOWN = 0.8
local MOVE_SPEED      = 6.0

function update(dt)
    local pid = boss_nearest_player()
    if pid < 0 then return end

    local dist = boss_distance_to_player(pid)
    boss_face_player(pid)

    if boss_get_cooldown("global") > 0 then return end

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
