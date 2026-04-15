#pragma once
#include "pch.h"





extern Vector<float> FireRateMap;

void MakeFireRateMap();

class CWeapon
{
public:
	CWeapon();
	~CWeapon();

	void SetWeapon(Protocol::WeaponType&& type, uint32&& currnetBullets);
	void SetBullet(Protocol::BulletType&& type, uint32& currentBullets);

	bool CanFire(uint32 serverTick) const;
	void OnFired(uint32 serverTick);

	Protocol::WeaponType GetWeaponState() const { return weaponType; }
	Protocol::BulletType GetBulletState() const { return bulletType; }

private:
	Protocol::WeaponType weaponType = Protocol::WEAPON_TYPE_NONE;
	float fireRate = 0.f; // 발사 속도 (초당 발사 수

	Protocol::BulletType bulletType = Protocol::BULLET_TYPE_NONE;
	uint32 currentBullets = 0;
	uint32 m_lastFireTick = 0;
};

