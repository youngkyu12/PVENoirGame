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
	void ApplyHit(uint32 serverTick, int damage, uint32 hitDurationTicks, uint64 serverMs);
	void Respawn(uint32 serverTick);
	void SetInitialSpawnPosition(const GameMath::Vec3& pos) { m_initialSpawnPosition = pos; }
	const GameMath::Vec3& GetInitialSpawnPosition() const { return m_initialSpawnPosition; }

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
		int32 forcedTransformReason = 0,
		bool suppressTerrainSnap = false)
	{
		m_pendingPortalTeleport.active = true;
		m_pendingPortalTeleport.destination = destination;
		m_pendingPortalTeleport.yaw = yaw;
		m_pendingPortalTeleport.forcedYawDelta = forcedYawDelta;
		m_pendingPortalTeleport.forcedTransformReason = forcedTransformReason;
		m_pendingPortalTeleport.suppressTerrainSnap = suppressTerrainSnap;
	}
	bool ConsumePendingPortalTeleport(
		GameMath::Vec3& outDestination,
		float& outYaw,
		float* outForcedYawDelta = nullptr,
		int32* outForcedTransformReason = nullptr,
		bool* outSuppressTerrainSnap = nullptr)
	{
		if (!m_pendingPortalTeleport.active)
			return false;

		outDestination = m_pendingPortalTeleport.destination;
		outYaw = m_pendingPortalTeleport.yaw;
		if (outForcedYawDelta)
			*outForcedYawDelta = m_pendingPortalTeleport.forcedYawDelta;
		if (outForcedTransformReason)
			*outForcedTransformReason = m_pendingPortalTeleport.forcedTransformReason;
		if (outSuppressTerrainSnap)
			*outSuppressTerrainSnap = m_pendingPortalTeleport.suppressTerrainSnap;
		m_pendingPortalTeleport = PendingPortalTeleport{};
		return true;
	}
	void ClearPendingPortalTeleport() { m_pendingPortalTeleport = PendingPortalTeleport{}; }
	void ClearPendingForcedTransform() { m_pendingForcedTransform = PendingForcedTransform{}; }
	void SetTerrainSnapSuppressed(bool suppressed) { m_suppressTerrainSnap = suppressed; }
	bool IsTerrainSnapSuppressed() const { return m_suppressTerrainSnap; }

	void QueueForcedTransformYawDelta(float yawDelta, int32 reason)
	{
		m_pendingForcedTransform.active = true;
		m_pendingForcedTransform.yawDelta = yawDelta;
		m_pendingForcedTransform.reason = reason;
	}
	bool ConsumeForcedTransformYawDelta(float& outYawDelta, int32& outReason)
	{
		if (!m_pendingForcedTransform.active)
			return false;

		outYawDelta = m_pendingForcedTransform.yawDelta;
		outReason = m_pendingForcedTransform.reason;
		m_pendingForcedTransform = PendingForcedTransform{};
		return true;
	}
public:
	static constexpr int kInventorySlotCount = 4;
private:
	CWeapon weapon;
	uint32 m_hitEndTick = 0;
	uint32 m_deathTick = 0;
	GameMath::Vec3 m_initialSpawnPosition = GameMath::Vec3::Zero();
	int32 m_lastMoveKeyCodes = 0;
	int32 m_rollMoveKeyCodes = 0;

	static constexpr uint64 kBuffDurationMs   = 10000;
	static constexpr int    kHealPotionAmount = 20;
	static constexpr int    kAttackBuffDamageMultiplier = 2;
	static constexpr float  kDefenseBuffIncomingDamageScale = 0.5f;
	static constexpr float  kSpeedBuffMoveMultiplier = 2.0f;

	std::array<int, kInventorySlotCount> m_inventoryCounts = {};
	uint64 m_attackBuffEndMs  = 0;
	uint64 m_defenseBuffEndMs = 0;
	uint64 m_speedBuffEndMs   = 0;

	struct PendingPortalTeleport
	{
		bool active = false;
		GameMath::Vec3 destination = GameMath::Vec3::Zero();
		float yaw = 0.0f;
		float forcedYawDelta = 0.0f;
		int32 forcedTransformReason = 0;
		bool suppressTerrainSnap = false;
	};
	PendingPortalTeleport m_pendingPortalTeleport;
	bool m_suppressTerrainSnap = false;

	struct PendingForcedTransform
	{
		bool active = false;
		float yawDelta = 0.0f;
		int32 reason = 0;
	};
	PendingForcedTransform m_pendingForcedTransform;

public:

	int  GetInventoryCount(Protocol::ItemType kind) const;
	void AddInventoryItem(Protocol::ItemType kind);
	bool UseInventoryItem(Protocol::ItemType kind, uint64 serverMs);

	bool IsAttackBuffActive(uint64 serverMs)  const { return serverMs < m_attackBuffEndMs; }
	bool IsDefenseBuffActive(uint64 serverMs) const { return serverMs < m_defenseBuffEndMs; }
	bool IsSpeedBuffActive(uint64 serverMs)   const { return serverMs < m_speedBuffEndMs; }
	int ApplyAttackBuffToDamage(int damage, uint64 serverMs) const;
	int ApplyDefenseBuffToIncomingDamage(int damage, uint64 serverMs) const;
	float GetMoveSpeedMultiplier(uint64 serverMs) const;

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
	bool IsRollInvincible() const { return m_animState == Protocol::ANIMATION_TYPE_ROLL; }

	void OnDeathEnter(uint32 serverTick);
	void OnRespawnEnter(uint32 serverTick);

private:
	EPlayerLifeState m_lifeState = EPlayerLifeState::Alive;
};
