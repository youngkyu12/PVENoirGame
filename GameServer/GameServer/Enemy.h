#pragma once
#include "ServerObject.h"
#include "GameMath.h"
#include "CWeapon.h"

class CEnemy : public CServerObject
{
public:
	uint64					enemyId = 0;
	string					name;
	Protocol::EnemyType	type = Protocol::ENEMY_TYPE_BASIC;
	GameSessionRef			ownerSession;
public:
	CEnemy() = default;
	CEnemy(uint64 id, const string& name, Protocol::EnemyType type, GameSessionRef session)
		: enemyId(id), name(name), type(type), ownerSession(session) {
		CServerObject::SetObjectId(id);
	}
public:
	//void Update(float dt) override;
	void Build(GameMath::Vec3 pos, GameMath::Vec3 rot);

private:
	//EnemyControllerComponentRef controller;
	CWeapon weapon;

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
};

