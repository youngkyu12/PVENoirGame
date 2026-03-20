#pragma once
#include "ServerObject.h"
#include "CWeapon.h"

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

private:
	//PlayerControllerComponentRef controller;
	CWeapon weapon;
public:
	void SetWeapon(WeaponType& type, uint32& currentBullets)
	{
		weapon.SetWeapon(type, currentBullets);
	}

	void SetBullet(BulletType& type, uint32& currentBullets)
	{
		weapon.SetBullet(type, currentBullets);
	}

};

