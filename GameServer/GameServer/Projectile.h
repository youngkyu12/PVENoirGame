#pragma once

#include "ServerObject.h"

class CProjectile : public CServerObject
{
public:
	CProjectile() = default;
	~CProjectile() override = default;

	void Activate(const GameMath::Vec3& pos, const GameMath::Vec3& vel, int32 lifetimeTicks, uint64 ownerId, Protocol::BulletType type)
	{
		SetPosition(pos);
		SetVelocity(vel);
		m_remainingTicks = (lifetimeTicks > 0) ? lifetimeTicks : 0;
		m_ownerObjectId = ownerId;
		m_bulletType = type;
		m_active = true;
		SetObjectType(Protocol::OBJECT_TYPE_BULLET);
	}

	void Deactivate()
	{
		m_active = false;
		m_remainingTicks = 0;
		m_ownerObjectId = 0;
		m_bulletType = Protocol::BULLET_TYPE_NONE;
		SetVelocity(GameMath::Vec3::Zero());
	}

	bool IsActive() const { return m_active; }
	uint64 GetOwnerId() const { return m_ownerObjectId; }
	Protocol::BulletType GetBulletType() const { return m_bulletType; }

	void Update(uint32 /*serverTick*/) override
	{
		if (!m_active)
			return;

		constexpr float kServerTickDtSec = 0.03f;
		Move(GetVelocity() * kServerTickDtSec);

		if (m_remainingTicks > 0)
			--m_remainingTicks;

		if (m_remainingTicks <= 0)
			Deactivate();
	}

private:
	bool m_active = false;
	uint64 m_ownerObjectId = 0;
	Protocol::BulletType m_bulletType = Protocol::BULLET_TYPE_NONE;
	int32 m_remainingTicks = 0;
};
