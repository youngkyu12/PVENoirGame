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

private:
	CWeapon weapon;
	uint32 m_hitEndTick = 0;
	uint32 m_deathTick = 0;

public:
	void SetWeapon(Protocol::WeaponType& type, uint32& currentBullets)
	{
		weapon.SetWeapon(std::move(type), std::move(currentBullets));
	}
	void SetWeapon(Protocol::WeaponType&& type, uint32&& currentBullets)
	{
		weapon.SetWeapon(std::move(type), std::move(currentBullets));
	}

	Protocol::WeaponType GetWeaponState() const { return weapon.GetWeaponState(); }
	Protocol::BulletType GetBulletState() const { return weapon.GetBulletState(); }
	CWeapon& GetWeapon() { return weapon; }
	const CWeapon& GetWeapon() const { return weapon; }

	void SetBullet(Protocol::BulletType&& type, uint32& currentBullets)
	{
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