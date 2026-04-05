//-----------------------------------------------------------------------------
// File: MonsterWeaponHitboxComponent.h
//-----------------------------------------------------------------------------
#pragma once
#include "Component.h"
#include <unordered_set>
#include <string>
#include <vector>

class CGameObject;

class CMonsterWeaponHitboxComponent final : public CComponentT<CMonsterWeaponHitboxComponent>
{
public:
	explicit CMonsterWeaponHitboxComponent(CGameObject* owner);

	void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;
	void OnUpdate(float dt) override;

public:
	void BindAttacker(CGameObject* attacker) { m_pAttacker = attacker; }

	void SetAttackClipName(const std::string& clipName) { m_attackClipName = clipName; }
	void SetActiveWindow(float startNormalized, float endNormalized);

	bool CanHitTarget(CGameObject* target) const;
	void MarkHitTarget(CGameObject* target);
	void ClearHitTargets() { m_hitTargets.clear(); }

private:
	bool IsAttackWindowOpen() const;

private:
	CGameObject* m_pAttacker = nullptr;

	std::string m_attackClipName = "Attack";

	float m_activeStartNormalized = 0.20f;
	float m_activeEndNormalized = 0.55f;

	bool m_bPrevHitboxActive = false;
	std::unordered_set<CGameObject*> m_hitTargets;

public:
	void SetUseOwnerBoneWeaponCapsules(bool enable) { m_useOwnerBoneWeaponCapsules = enable; }

	void AddBoneWeaponConfig(
		const std::string& clipName,
		float startNormalized,
		float endNormalized,
		const std::vector<std::string>& rootBoneNames);

private:
	struct BoneWeaponConfig
	{
		std::string clipName;
		float startNormalized = 0.0f;
		float endNormalized = 1.0f;
		std::vector<std::string> rootBoneNames;
	};

	int FindActiveBoneWeaponConfigIndex() const;

private:
	bool m_useOwnerBoneWeaponCapsules = false;
	int m_activeBoneWeaponConfigIndex = -1;
	std::vector<BoneWeaponConfig> m_boneWeaponConfigs;
};