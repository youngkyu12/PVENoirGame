#pragma once
#include "ServerObject.h"
#include "CWeapon.h"
#include "ColliderComponent.h"

class Player : public CServerObject
{
public:
	uint64					playerId = 0;
	string					name;
	Protocol::PlayerType	type = Protocol::PLAYER_TYPE_NONE;
	GameSessionRef			ownerSession;
public:
	Player() = default;
	Player(uint64 id, const string& name, Protocol::PlayerType type, GameSessionRef session)
		: playerId(id), name(name), type(type), ownerSession(session) {
		CServerObject::SetObjectId(id);
	}

public:
	virtual void Update(uint32 serverTick) override;
	void Build();
	void ApplyHit(uint32 serverTick, int damage, uint32 hitDurationTicks = 10);
	void Respawn(uint32 serverTick);

	void SetLastMoveKeyCodes(int32 keyCodes) { m_lastMoveKeyCodes = keyCodes; }
	int32 GetLastMoveKeyCodes() const { return m_lastMoveKeyCodes; }
	void SetRollMoveKeyCodes(int32 keyCodes) { m_rollMoveKeyCodes = keyCodes; }
	int32 GetRollMoveKeyCodes() const { return m_rollMoveKeyCodes; }
	void ClearMoveKeyCodes()
	{
		m_lastMoveKeyCodes = 0;
		m_rollMoveKeyCodes = 0;
	}

	bool HasPendingPortalTeleport() const { return m_pendingPortalTeleport.active; }
	void SetPendingPortalTeleport(
		const GameMath::Vec3& destination,
		float yaw,
		float forcedYawDelta = 0.0f,
		bool hasForcedYawDelta = false)
	{
		m_pendingPortalTeleport.active = true;
		m_pendingPortalTeleport.destination = destination;
		m_pendingPortalTeleport.yaw = yaw;
		m_pendingPortalTeleport.forcedYawDelta = forcedYawDelta;
		m_pendingPortalTeleport.hasForcedYawDelta = hasForcedYawDelta;
	}
	bool ConsumePendingPortalTeleport(
		GameMath::Vec3& outDestination,
		float& outYaw,
		float* outForcedYawDelta = nullptr,
		bool* outHasForcedYawDelta = nullptr)
	{
		if (!m_pendingPortalTeleport.active)
			return false;

		outDestination = m_pendingPortalTeleport.destination;
		outYaw = m_pendingPortalTeleport.yaw;
		if (outForcedYawDelta)
			*outForcedYawDelta = m_pendingPortalTeleport.forcedYawDelta;
		if (outHasForcedYawDelta)
			*outHasForcedYawDelta = m_pendingPortalTeleport.hasForcedYawDelta;
		m_pendingPortalTeleport = PendingPortalTeleport{};
		return true;
	}
	void ClearPendingPortalTeleport() { m_pendingPortalTeleport = PendingPortalTeleport{}; }

private:
	CWeapon weapon;
	uint32 m_hitEndTick = 0;
	uint32 m_deathTick = 0;
	int32 m_lastMoveKeyCodes = 0;
	int32 m_rollMoveKeyCodes = 0;

	struct PendingPortalTeleport
	{
		bool active = false;
		GameMath::Vec3 destination = GameMath::Vec3::Zero();
		float yaw = 0.0f;
		float forcedYawDelta = 0.0f;
		bool hasForcedYawDelta = false;
	};
	PendingPortalTeleport m_pendingPortalTeleport;

public:
	void SetWeapon(Protocol::WeaponType& type, uint32& currentBullets)
	{
		weapon.CancelAttack();
		weapon.SetWeapon(std::move(type), std::move(currentBullets));
	}
	void SetWeapon(Protocol::WeaponType&& type, uint32&& currentBullets)
	{
		weapon.CancelAttack();
		weapon.SetWeapon(std::move(type), std::move(currentBullets));
	}

	Protocol::WeaponType GetWeaponState() const { return weapon.GetWeaponState(); }
	Protocol::BulletType GetBulletState() const { return weapon.GetBulletState(); }
	CWeapon& GetWeapon() { return weapon; }
	const CWeapon& GetWeapon() const { return weapon; }

	void SetBullet(Protocol::BulletType&& type, uint32& currentBullets)
	{
		weapon.CancelAttack();
		weapon.SetBullet(std::move(type), currentBullets);
	}

	bool CanFire(uint32 serverTick) const { return weapon.CanFire(serverTick); }
	void OnFired(uint32 serverTick) { weapon.OnFired(serverTick); }

public:
	enum class EPlayerLifeState : uint8
	{
		Alive = 0,
		DeadAnimating,
		Respawning
	};

	EPlayerLifeState GetLifeState() const { return m_lifeState; }
	bool IsInputBlocked() const { return m_lifeState != EPlayerLifeState::Alive; }

	void OnDeathEnter(uint32 serverTick);
	void OnRespawnEnter(uint32 serverTick);

private:
	EPlayerLifeState m_lifeState = EPlayerLifeState::Alive;
};
