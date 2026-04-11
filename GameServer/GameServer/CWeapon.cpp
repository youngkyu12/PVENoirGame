#include "CWeapon.h"

Vector<float> FireRateMap;

void MakeFireRateMap()
{
	FireRateMap.resize(Protocol::WEAPON_TYPE_CANON + 1);

	FireRateMap[Protocol::WEAPON_TYPE_NONE] = 0.f;
	FireRateMap[Protocol::WEAPON_TYPE_SWORD] = 1.f; // 1초에 1회 공격
	FireRateMap[Protocol::WEAPON_TYPE_BOW] = 2.f;   // 1초에 2회 공격
	FireRateMap[Protocol::WEAPON_TYPE_AXE] = 0.5f;  // 1초에 0.5회 공격 (2초에 1회 공격)
	FireRateMap[Protocol::WEAPON_TYPE_CANON] = 0.2f; // 1초에 0.2회 공격 (5초에 1회 공격)
}


CWeapon::CWeapon()
	: bulletType(Protocol::BULLET_TYPE_NONE), weaponType(Protocol::WEAPON_TYPE_NONE)
{
}

CWeapon::~CWeapon()
{
}

void CWeapon::SetWeapon(Protocol::WeaponType&& type, uint32&& currnetBullets)
{
	weaponType = type;
	fireRate = FireRateMap[type];
	switch (type)
	{
	case Protocol::WEAPON_TYPE_NONE:
		currentBullets = 0;
		break;
	case Protocol::WEAPON_TYPE_SWORD:
		currentBullets = 0; // 근접 무기는 탄환이 없음
		break;
	case Protocol::WEAPON_TYPE_BOW:
		currentBullets = 30; // 예시: 활은 30발의 화살을 가짐
		break;
	case Protocol::WEAPON_TYPE_AXE:
		currentBullets = 0; // 근접 무기는 탄환이 없음
		break;
	case Protocol::WEAPON_TYPE_CANON:
		currentBullets = 10; // 예시: 대포는 10발의 포탄을 가짐
		break;
	default:
		currentBullets = 0;
		break;
	}
}

bool CWeapon::CanFire(uint32 serverTick) const
{
	if (fireRate <= 0.f)
		return false;

	if (currentBullets == 0 && bulletType != Protocol::BULLET_TYPE_NONE)
		return false;

	const float ticksPerShot = 33.3f / fireRate;
	const uint32 requiredTicks = static_cast<uint32>(ticksPerShot);
	if (m_lastFireTick == 0)
		return true;

	return (serverTick >= m_lastFireTick + requiredTicks);
}

void CWeapon::OnFired(uint32 serverTick)
{
	m_lastFireTick = serverTick;

	if (currentBullets > 0)
		--currentBullets;
}

void CWeapon::SetBullet(Protocol::BulletType&& type, uint32& currentBullets)
{
	bulletType = type;
	switch (type)
	{
	case Protocol::BULLET_TYPE_NONE:
		currentBullets = 0;
		break;
	case Protocol::BULLET_TYPE_ARROW:
		currentBullets = 30; // 예시: 화살은 30발
		break;
	case Protocol::BULLET_TYPE_CANNONBALL:
		currentBullets = 10; // 예시: 포탄은 10발
		break;
	default:
		currentBullets = 0;
		break;
	}
}

