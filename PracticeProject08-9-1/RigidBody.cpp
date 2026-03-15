#include "stdafx.h"
#include "RigidBody.h"
#include "Object.h" // CGameObject

CRigidBody::CRigidBody(CGameObject* owner)
	: CComponentT<CRigidBody>(owner)
{
}
