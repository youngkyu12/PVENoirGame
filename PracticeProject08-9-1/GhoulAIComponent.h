//-----------------------------------------------------------------------------
// File: GhoulAIComponent.h
//-----------------------------------------------------------------------------

#pragma once

#include "MonsterAIComponent.h"

class CGhoulAIComponent final : public CMonsterAIComponent
{
public:
	explicit CGhoulAIComponent(CGameObject* owner);
	~CGhoulAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CGhoulAIComponent>();
	}

protected:
	bool AcquireTarget() override;
	void UpdateBehavior(float dt) override;
	bool TryPerformAttack() override;
};