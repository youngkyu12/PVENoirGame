//-----------------------------------------------------------------------------
// File: HealthComponent.h
//-----------------------------------------------------------------------------
#pragma once

#include "Component.h"
#include <algorithm>

class CHealthComponent final : public CComponentT<CHealthComponent>
{
public:
	explicit CHealthComponent(CGameObject* owner)
		: CComponentT(owner)
	{
	}

	void SetMaxHp(int maxHp, bool fillCurrent = true)
	{
		int newMax = maxHp;
		if ( newMax < 1 ) newMax = 1;
		m_maxHp = newMax;

		if ( fillCurrent )
			m_currentHp = m_maxHp;
		else
			m_currentHp = std::clamp(m_currentHp, 0, m_maxHp);
	}

	int GetMaxHp() const { return m_maxHp; }
	int GetCurrentHp() const { return m_currentHp; }

	float GetHpRatio() const
	{
		if ( m_maxHp <= 0 )
			return 0.0f;

		return std::clamp(
			static_cast< float >( m_currentHp ) / static_cast< float >( m_maxHp ),
			0.0f,
			1.0f
		);
	}

	bool IsDead() const { return m_currentHp <= 0; }

	void ResetToMax()
	{
		m_currentHp = m_maxHp;
	}

	bool TakeDamage(int damage)
	{
		if ( damage <= 0 )
			return false;

		if ( IsDead() )
			return false;

		m_currentHp -= damage;

		if ( m_currentHp < 0 )
			m_currentHp = 0;

		return true;
	}

	void Heal(int amount)
	{
		if ( amount <= 0 )
			return;

		m_currentHp += amount;

		if ( m_currentHp > m_maxHp )
			m_currentHp = m_maxHp;
	}

private:
	int m_maxHp = 1;
	int m_currentHp = 1;
};