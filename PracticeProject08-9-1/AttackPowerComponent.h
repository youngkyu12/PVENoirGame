//-----------------------------------------------------------------------------
// File: AttackPowerComponent.h
//-----------------------------------------------------------------------------
#pragma once

#include "Component.h"

class CAttackPowerComponent final : public CComponentT<CAttackPowerComponent>
{
public:
	explicit CAttackPowerComponent(CGameObject* owner)
		: CComponentT(owner)
	{
	}

	void SetAttackPower(int attackPower)
	{
		m_attackPower = attackPower;
	}

	int GetAttackPower() const
	{
		return m_attackPower;
	}

private:
	int m_attackPower = 0;
};