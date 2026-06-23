#include "CWeapon.h"

namespace
{
	WeaponTuning g_defaultWeaponTunings[Protocol::WEAPON_TYPE_CANON + 1];

	constexpr float kServerTickMs = 60.0f;
	constexpr float kServerTicksPerSec = 1000.0f / kServerTickMs;

	const WeaponTuning& GetDefaultTuning(Protocol::WeaponType type)
	{
		if (type < Protocol::WEAPON_TYPE_NONE || type > Protocol::WEAPON_TYPE_CANON)
			return g_defaultWeaponTunings[Protocol::WEAPON_TYPE_NONE];

		return g_defaultWeaponTunings[type];
	}
}

void MakeFireRateMap()
{
	g_defaultWeaponTunings[Protocol::WEAPON_TYPE_NONE].fireRate = 0.0f;
	g_defaultWeaponTunings[Protocol::WEAPON_TYPE_SWORD].fireRate = 1.0f;
	g_defaultWeaponTunings[Protocol::WEAPON_TYPE_AXE].fireRate = 0.5f;
	g_defaultWeaponTunings[Protocol::WEAPON_TYPE_CANON].fireRate = 1.667f;

	WeaponTuning& bow = g_defaultWeaponTunings[Protocol::WEAPON_TYPE_BOW];
	bow.fireRate = 2.0f;
	bow.bowLoadTicks = 8;
	bow.bowReleaseTicks = 4;
	bow.bowRecoveryTicks = 4;
	bow.arrowSpeed = 12.0f;
	bow.arrowLifeTicks = 200;
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
	CancelAttack();
	weaponType = type;
	switch (type)
	{
	case Protocol::WEAPON_TYPE_NONE:
		currentBullets = 0;
		break;
	case Protocol::WEAPON_TYPE_SWORD:
		currentBullets = 0;
		break;
	case Protocol::WEAPON_TYPE_BOW:
		currentBullets = 30;
		break;
	case Protocol::WEAPON_TYPE_AXE:
		currentBullets = 0;
		break;
	case Protocol::WEAPON_TYPE_CANON:
		currentBullets = 10;
		break;
	default:
		currentBullets = 0;
		break;
	}
}

bool CWeapon::CanFire(uint32 serverTick) const
{
	const WeaponTuning& tuning = GetTuning();
	if (tuning.fireRate <= 0.0f)
		return false;

	if (currentBullets == 0 && bulletType != Protocol::BULLET_TYPE_NONE)
		return false;

	const float ticksPerShot = kServerTicksPerSec / tuning.fireRate;
	const uint32 requiredTicks = static_cast<uint32>(ceilf(ticksPerShot));
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
	CancelAttack();
	bulletType = type;
	switch (type)
	{
	case Protocol::BULLET_TYPE_NONE:
		this->currentBullets = 0;
		break;
	case Protocol::BULLET_TYPE_ARROW:
		this->currentBullets = 30;
		break;
	case Protocol::BULLET_TYPE_CANNONBALL:
		this->currentBullets = 10;
		break;
	default:
		this->currentBullets = 0;
		break;
	}
}

const WeaponTuning& CWeapon::GetTuning() const
{
	if (m_tuning)
		return *m_tuning;

	return GetDefaultTuning(weaponType);
}

bool CWeapon::BeginAttack(uint32 serverTick)
{
	if (!CanFire(serverTick))
		return false;

	if (m_attackPhase != EWeaponAttackPhase::None)
		return false;

	if (weaponType == Protocol::WEAPON_TYPE_BOW)
	{
		m_attackPhase = EWeaponAttackPhase::BowLoad;
		m_attackStartTick = serverTick;
		m_releaseFired = false;
		return true;
	}

	return true;
}

WeaponFireRequest CWeapon::UpdateAttack(uint32 serverTick)
{
	WeaponFireRequest req{};

	if (m_attackPhase == EWeaponAttackPhase::None)
		return req;

	if (weaponType != Protocol::WEAPON_TYPE_BOW)
	{
		CancelAttack();
		return req;
	}

	const WeaponTuning& tuning = GetTuning();
	const uint32 elapsed = serverTick - m_attackStartTick;
	const uint32 releaseStart = tuning.bowLoadTicks;
	const uint32 recoveryStart = tuning.bowLoadTicks + tuning.bowReleaseTicks;
	const uint32 attackEnd = recoveryStart + tuning.bowRecoveryTicks;

	if (elapsed >= releaseStart && m_attackPhase == EWeaponAttackPhase::BowLoad)
		m_attackPhase = EWeaponAttackPhase::BowRelease;

	if (m_attackPhase == EWeaponAttackPhase::BowRelease && !m_releaseFired)
	{
		m_releaseFired = true;
		req.fire = true;
		req.bulletType = Protocol::BULLET_TYPE_ARROW;
		req.speed = tuning.arrowSpeed;
		req.lifeTicks = tuning.arrowLifeTicks;
		return req;
	}

	if (elapsed >= recoveryStart && m_attackPhase == EWeaponAttackPhase::BowRelease)
		m_attackPhase = EWeaponAttackPhase::BowRecovery;

	if (elapsed >= attackEnd)
		CancelAttack();

	return req;
}

void CWeapon::CancelAttack()
{
	m_attackPhase = EWeaponAttackPhase::None;
	m_attackStartTick = 0;
	m_releaseFired = false;
}