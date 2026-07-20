//-----------------------------------------------------------------------------
// File: HealthComponent.h
//-----------------------------------------------------------------------------
#pragma once

#include "Component.h"
#include <algorithm>

class CAudioManager;

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

	void SetCurrentHp(int hp)
	{
		m_currentHp = std::clamp(hp, 0, m_maxHp);
	}

	void ResetToMax()
	{
		m_currentHp = m_maxHp;
	}

	void SetIncomingDamageScale(float scale)
	{
		m_incomingDamageScale = scale < 0.0f ? 0.0f : scale;
	}

	float GetIncomingDamageScale() const { return m_incomingDamageScale; }

	bool TakeDamage(int damage, bool playHitSfx = true)
	{
		if ( damage <= 0 )
			return false;

		if ( IsDead() )
			return false;

		int effectiveDamage = damage;

		if ( m_incomingDamageScale != 1.0f )
		{
			effectiveDamage = static_cast< int >( std::lround(static_cast< float >( damage ) * m_incomingDamageScale) );

			if ( damage > 0 && m_incomingDamageScale > 0.0f && effectiveDamage < 1 )
				effectiveDamage = 1;
		}

		if ( effectiveDamage <= 0 )
			return false;

		const int hpBefore = m_currentHp;

		m_currentHp -= effectiveDamage;

		if ( m_currentHp < 0 )
			m_currentHp = 0;

		const bool actuallyDamaged = ( m_currentHp < hpBefore );

		if ( actuallyDamaged && playHitSfx )
			PlayHitSfx();

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
	float m_incomingDamageScale = 1.0f;

public:
	void SetAudioManager(CAudioManager* audioManager) { m_audioManager = audioManager; }
	void RequestHitSfx()
	{
		PlayHitSfx();
	}
private:
	void PlayHitSfx();

	static const char* SelectHitSfxPath();

private:
	CAudioManager* m_audioManager = nullptr;
};
