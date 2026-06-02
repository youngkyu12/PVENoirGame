#pragma once

#include "ServerObject.h"

class CProjectile : public CServerObject
{
public:
	CProjectile() = default;
	~CProjectile() override = default;

	void Activate(const GameMath::Vec3& pos, const GameMath::Vec3& vel, int32 lifetimeTicks, uint64 lifetimeTickMs, uint64 ownerId, Protocol::BulletType type)
	{
		SetPosition(pos);
		SetVelocity(vel);
		m_remainingMs = (lifetimeTicks > 0) ? static_cast<uint64>(lifetimeTicks) * lifetimeTickMs : 0;
		m_ownerObjectId = ownerId;
		m_bulletType = type;
		m_active = true;
		SetObjectType(Protocol::OBJECT_TYPE_BULLET);
	}

	void Deactivate()
	{
		m_active = false;
		m_remainingMs = 0;
		m_ownerObjectId = 0;
		m_bulletType = Protocol::BULLET_TYPE_NONE;
		SetAttackPower(0);
		SetVelocity(GameMath::Vec3::Zero());
	}

	bool IsActive() const { return m_active; }
	uint64 GetOwnerId() const { return m_ownerObjectId; }
	Protocol::BulletType GetBulletType() const { return m_bulletType; }

	void Update(uint32 /*serverTick*/) override
	{
		Update(0.06f, 60);
	}

	void Update(float dtSec, uint64 elapsedMs)
	{
		if (!m_active)
			return;

		if (m_bulletType == Protocol::BULLET_TYPE_ARROW)
		{
			GameMath::Vec3 vel = GetVelocity();
			vel.y += kArrowGravityY * dtSec;
			SetVelocity(vel);
		}

		Move(GetVelocity() * dtSec);

		if (elapsedMs >= m_remainingMs)
		{
			Deactivate();
			return;
		}

		m_remainingMs -= elapsedMs;
	}

private:
	static constexpr float kArrowGravityY = -2.5f;

	bool m_active = false;
	uint64 m_ownerObjectId = 0;
	Protocol::BulletType m_bulletType = Protocol::BULLET_TYPE_NONE;
	uint64 m_remainingMs = 0;
};
