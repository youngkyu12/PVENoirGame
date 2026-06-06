local MELEE_RANGE     = 7.0
local MELEE_COOLDOWN  = 2.0
local GLOBAL_COOLDOWN = 0.8

function update(dt)
    local pid = boss_nearest_player()
    if pid < 0 then return end

    if boss_get_cooldown("global") > 0 then return end

    local dist = boss_distance_to_player(pid)
    boss_face_player(pid)

    if dist <= MELEE_RANGE and boss_get_cooldown("melee") <= 0 then
        boss_start_action("Melee")
        boss_set_cooldown("melee",  MELEE_COOLDOWN)
        boss_set_cooldown("global", GLOBAL_COOLDOWN)
        boss_log("Melee -> player " .. pid)
        return
    end
end
