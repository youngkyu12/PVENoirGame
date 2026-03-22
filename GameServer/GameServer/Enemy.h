#pragma once
#include "ServerObject.h"
#include "GameMath.h"

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
};

