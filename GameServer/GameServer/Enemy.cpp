#include "pch.h"
#include "Enemy.h"



void CEnemy::Build(GameMath::Vec3 pos, GameMath::Vec3 rot)
{
	SetPosition(pos);
	Rotate(rot.x, rot.y, rot.z);
}

//void CEnemy::Update(float dt)
//{
//}
