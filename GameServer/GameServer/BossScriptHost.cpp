#include "pch.h"
#include "BossScriptHost.h"
#include "BossAIContext.h"
#include "Room.h"
#include "Enemy.h"
#include "Player.h"

#include <lua/lua.hpp>
#include <iostream>
#include <cmath>

// ---------------------------------------------------------------------------
// Context access via Lua extra space
// ---------------------------------------------------------------------------
static CBossAIContext* GetCtx(lua_State* L)
{
	return *reinterpret_cast<CBossAIContext**>(lua_getextraspace(L));
}

// ---------------------------------------------------------------------------
// Helper: find boss enemy raw pointer
// ---------------------------------------------------------------------------
static CEnemy* GetBoss(lua_State* L)
{
	return GetCtx(L)->room->GetBossEnemy();
}

// ---------------------------------------------------------------------------
// Lua API functions
// ---------------------------------------------------------------------------
static int lua_boss_get_hp(lua_State* L)
{
	CEnemy* boss = GetBoss(L);
	lua_pushinteger(L, boss ? boss->GetCurrentHp() : 0);
	return 1;
}

static int lua_boss_get_max_hp(lua_State* L)
{
	CEnemy* boss = GetBoss(L);
	lua_pushinteger(L, boss ? boss->GetMaxHp() : 0);
	return 1;
}

static int lua_boss_get_pos(lua_State* L)
{
	CEnemy* boss = GetBoss(L);
	if (!boss) { lua_pushnumber(L, 0); lua_pushnumber(L, 0); return 2; }
	auto pos = boss->GetPosition();
	lua_pushnumber(L, pos.x);
	lua_pushnumber(L, pos.z);
	return 2;
}

static int lua_boss_nearest_player(lua_State* L)
{
	CBossAIContext* ctx = GetCtx(L);
	CEnemy* boss = GetBoss(L);
	if (!boss) { lua_pushinteger(L, -1); return 1; }

	auto bossPos = boss->GetPosition();
	const auto& players = ctx->room->GetPlayers();

	lua_Integer nearestId = -1;
	float nearestDistSq = FLT_MAX;

	for (auto& [id, player] : players)
	{
		if (!player || !player->IsActive()) continue;
		if (!ctx->room->IsPositionInsideMegaGridNumber(player->GetPosition(), 5)) continue;

		auto pPos = player->GetPosition();
		float dx = pPos.x - bossPos.x;
		float dz = pPos.z - bossPos.z;
		float distSq = dx * dx + dz * dz;
		if (distSq < nearestDistSq)
		{
			nearestDistSq = distSq;
			nearestId = static_cast<lua_Integer>(id);
		}
	}

	lua_pushinteger(L, nearestId);
	return 1;
}

static int lua_boss_distance_to_player(lua_State* L)
{
	CBossAIContext* ctx = GetCtx(L);
	CEnemy* boss = GetBoss(L);
	lua_Integer pidArg = luaL_checkinteger(L, 1);

	if (!boss || pidArg < 0) { lua_pushnumber(L, FLT_MAX); return 1; }
	uint64 playerId = static_cast<uint64>(pidArg);

	const auto& players = ctx->room->GetPlayers();
	auto it = players.find(playerId);
	if (it == players.end() || !it->second) { lua_pushnumber(L, FLT_MAX); return 1; }

	auto bPos = boss->GetPosition();
	auto pPos = it->second->GetPosition();
	float dx = pPos.x - bPos.x;
	float dz = pPos.z - bPos.z;
	lua_pushnumber(L, std::sqrt(dx * dx + dz * dz));
	return 1;
}

static int lua_boss_face_player(lua_State* L)
{
	CBossAIContext* ctx = GetCtx(L);
	CEnemy* boss = GetBoss(L);
	lua_Integer pidArg = luaL_checkinteger(L, 1);

	if (!boss || pidArg < 0) return 0;
	uint64 playerId = static_cast<uint64>(pidArg);

	const auto& players = ctx->room->GetPlayers();
	auto it = players.find(playerId);
	if (it == players.end() || !it->second) return 0;

	auto bPos = boss->GetPosition();
	auto pPos = it->second->GetPosition();
	float dx = pPos.x - bPos.x;
	float dz = pPos.z - bPos.z;
	float yaw = std::atan2(dx, dz) * (180.0f / 3.14159265f);
	boss->SetYaw(yaw);
	return 0;
}

static int lua_boss_start_action(lua_State* L)
{
	CEnemy* boss = GetBoss(L);
	const char* name = luaL_checkstring(L, 1);
	if (!boss) return 0;

	Protocol::AnimationType anim = Protocol::ANIMATION_TYPE_IDLE;
	if (std::strcmp(name, "Melee") == 0)       anim = Protocol::ANIMATION_TYPE_ATTACK;
	else if (std::strcmp(name, "Die") == 0)    anim = Protocol::ANIMATION_TYPE_DIE;

	boss->SetAnimState(anim);
	return 0;
}

static int lua_boss_get_cooldown(lua_State* L)
{
	CBossAIContext* ctx = GetCtx(L);
	const char* name = luaL_checkstring(L, 1);
	lua_pushnumber(L, ctx->GetCooldown(name));
	return 1;
}

static int lua_boss_set_cooldown(lua_State* L)
{
	CBossAIContext* ctx = GetCtx(L);
	const char* name  = luaL_checkstring(L, 1);
	float       sec   = static_cast<float>(luaL_checknumber(L, 2));
	ctx->SetCooldown(name, sec);
	return 0;
}

static int lua_boss_rand_float(lua_State* L)
{
	CBossAIContext* ctx = GetCtx(L);
	float lo = static_cast<float>(luaL_checknumber(L, 1));
	float hi = static_cast<float>(luaL_checknumber(L, 2));
	lua_pushnumber(L, ctx->RandFloat(lo, hi));
	return 1;
}

static int lua_boss_log(lua_State* L)
{
	const char* msg = luaL_checkstring(L, 1);
	std::cout << "[BossLua] " << msg << "\n";
	return 0;
}

// ---------------------------------------------------------------------------
// CBossScriptHost
// ---------------------------------------------------------------------------
CBossScriptHost::CBossScriptHost()
{
	m_L = luaL_newstate();
	luaL_openlibs(m_L);
}

CBossScriptHost::~CBossScriptHost()
{
	if (m_L)
	{
		lua_close(m_L);
		m_L = nullptr;
	}
}

bool CBossScriptHost::LoadScript(const std::string& path)
{
	if (!m_L) return false;

	int result = luaL_loadfile(m_L, path.c_str());
	if (result != LUA_OK)
	{
		std::cerr << "[BossScriptHost] Load failed: " << lua_tostring(m_L, -1) << "\n";
		lua_pop(m_L, 1);
		return false;
	}

	result = lua_pcall(m_L, 0, 0, 0);
	if (result != LUA_OK)
	{
		std::cerr << "[BossScriptHost] Exec failed: " << lua_tostring(m_L, -1) << "\n";
		lua_pop(m_L, 1);
		return false;
	}

	m_bLoaded = true;
	return true;
}

void CBossScriptHost::RegisterBossAPI(CBossAIContext* ctx)
{
	*reinterpret_cast<CBossAIContext**>(lua_getextraspace(m_L)) = ctx;

	static const luaL_Reg kBossAPI[] = {
		{ "boss_get_hp",              lua_boss_get_hp },
		{ "boss_get_max_hp",          lua_boss_get_max_hp },
		{ "boss_get_pos",             lua_boss_get_pos },
		{ "boss_nearest_player",      lua_boss_nearest_player },
		{ "boss_distance_to_player",  lua_boss_distance_to_player },
		{ "boss_face_player",         lua_boss_face_player },
		{ "boss_start_action",        lua_boss_start_action },
		{ "boss_get_cooldown",        lua_boss_get_cooldown },
		{ "boss_set_cooldown",        lua_boss_set_cooldown },
		{ "boss_rand_float",          lua_boss_rand_float },
		{ "boss_log",                 lua_boss_log },
		{ nullptr, nullptr }
	};

	lua_getglobal(m_L, "_G");
	luaL_setfuncs(m_L, kBossAPI, 0);
	lua_pop(m_L, 1);
}
