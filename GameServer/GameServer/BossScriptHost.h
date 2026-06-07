#pragma once

#include <string>

struct lua_State;
struct CBossAIContext;

class CBossScriptHost
{
public:
	CBossScriptHost();
	~CBossScriptHost();

	bool LoadScript(const std::string& path);
	void RegisterBossAPI(CBossAIContext* ctx);

	bool IsLoaded() const { return m_L != nullptr && m_bLoaded; }
	lua_State* GetState() const { return m_L; }

private:
	lua_State* m_L = nullptr;
	bool m_bLoaded = false;
};
