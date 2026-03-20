#pragma once
#include "pch.h"

enum WeaponType : uint16_t
{
	WEAPON_TYPE_NONE = 0,
	WEAPON_TYPE_SWORD,
	WEAPON_TYPE_BOW,
	WEAPON_TYPE_AXE,
	WEAPON_TYPE_CANON,
};

enum BulletType : uint16_t
{
	BULLET_TYPE_NONE = 0,
	BULLET_TYPE_ARROW,
	BULLET_TYPE_CANNONBALL,
};



extern Vector<float> FireRateMap;

void MakeFireRateMap();

class CWeapon
{
public:
	CWeapon();
	~CWeapon();

	void SetWeapon(WeaponType& type, uint32& currnetBullets);
	void SetBullet(BulletType& type, uint32& currentBullets);

private:
	WeaponType weaponType = WEAPON_TYPE_NONE;
	float fireRate = 0.f; // 발사 속도 (초당 발사 수

	BulletType bulletType = BULLET_TYPE_NONE;
	uint32 currentBullets = 0;
};

