#pragma once
#include "ServerObject.h"
#include "GameMath.h"
#include "CWeapon.h"
#include "MonsterAI.h"

class CEnemy : public CServerObject
{
public:
	uint64					enemyId = 0;
	string					name;
	Protocol::EnemyType	type = Protocol::ENEMY_TYPE_BASIC;
	GameSessionRef			ownerSession;
public:
	CEnemy() = default;
	virtual ~CEnemy();
	CEnemy(uint64 id, const string& name, Protocol::EnemyType type, GameSessionRef session)
		: enemyId(id), name(name), type(type), ownerSession(session) {
		CServerObject::SetObjectId(id);
	}
public:
	virtual void Update(uint32 serverTick) override;
	void Build(GameMath::Vec3 pos, GameMath::Vec3 rot);
	void ApplyHit(uint32 serverTick, int damage, uint32 hitDurationTicks = 20);
	void UpdateAI(float dt);
	CMonsterAI* GetMonsterAI() { EnsureAI(); return m_monsterAI.get(); }
	void SetSpawnFx(uint32 type, uint32 tick, uint32 serial)
	{
		m_spawnFxType = type;
		m_spawnFxTick = tick;
		m_spawnFxSerial = serial;
	}
	uint32 GetSpawnFxType() const { return m_spawnFxType; }
	uint32 GetSpawnFxTick() const { return m_spawnFxTick; }
	uint32 GetSpawnFxSerial() const { return m_spawnFxSerial; }

protected:
	virtual bool UsesMonsterAI() const { return true; }

private:
	void EnsureAI();

	CWeapon weapon;
	std::unique_ptr<CMonsterAI> m_monsterAI;

public:
	void SetWeapon(Protocol::WeaponType& type, uint32& currentBullets)
	{
		weapon.SetWeapon(std::move(type), std::move(currentBullets));
	}

	void SetBullet(Protocol::BulletType& type, uint32& currentBullets)
	{
		weapon.SetBullet(std::move(type), currentBullets);
	}

	Protocol::WeaponType GetWeaponState() const { return weapon.GetWeaponState(); }
	Protocol::BulletType GetBulletState() const { return weapon.GetBulletState(); }

private:
	uint32 m_hitEndTick = 0;
	uint32 m_bossHitReactionSuperArmorEndTick = 0;
	uint32 m_spawnFxType = 0;
	uint32 m_spawnFxTick = 0;
	uint32 m_spawnFxSerial = 0;
};
