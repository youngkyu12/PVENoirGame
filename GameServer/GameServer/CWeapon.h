#pragma once
#include "pch.h"

void MakeFireRateMap();

struct WeaponTuning
{
	float fireRate = 0.0f;

	uint32 bowLoadTicks = 8;
	uint32 bowReleaseTicks = 4;
	uint32 bowRecoveryTicks = 4;

	float arrowSpeed = 12.0f;
	uint32 arrowLifeTicks = 200;
};

struct WeaponFireRequest
{
	bool fire = false;
	Protocol::BulletType bulletType = Protocol::BULLET_TYPE_NONE;
	float speed = 0.0f;
	uint32 lifeTicks = 0;
};

enum class EWeaponAttackPhase : uint8
{
	None,
	BowLoad,
	BowRelease,
	BowRecovery,
};

class CWeapon
{
public:
	CWeapon();
	~CWeapon();

	void SetWeapon(Protocol::WeaponType&& type, uint32&& currnetBullets);
	void SetBullet(Protocol::BulletType&& type, uint32& currentBullets);

	bool CanFire(uint32 serverTick) const;
	void OnFired(uint32 serverTick);

	void SetTuning(const WeaponTuning* tuning) { m_tuning = tuning; }
	const WeaponTuning& GetTuning() const;

	bool BeginAttack(uint32 serverTick);
	WeaponFireRequest UpdateAttack(uint32 serverTick);
	void CancelAttack();
	bool IsAttacking() const { return m_attackPhase != EWeaponAttackPhase::None; }

	Protocol::WeaponType GetWeaponState() const { return weaponType; }
	Protocol::BulletType GetBulletState() const { return bulletType; }

private:
	Protocol::WeaponType weaponType = Protocol::WEAPON_TYPE_NONE;
	const WeaponTuning* m_tuning = nullptr;

	EWeaponAttackPhase m_attackPhase = EWeaponAttackPhase::None;
	uint32 m_attackStartTick = 0;
	bool m_releaseFired = false;

	Protocol::BulletType bulletType = Protocol::BULLET_TYPE_NONE;
	uint32 currentBullets = 0;
	uint32 m_lastFireTick = 0;
};