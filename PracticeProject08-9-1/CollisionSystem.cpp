#include "stdafx.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"
#include "MonsterCombatComponent.h"
#include "Object.h"

#include "ActorTagComponent.h"
#include "Mesh.h"
#include "WeaponHitboxComponent.h"
#include "MonsterWeaponHitboxComponent.h"
#include "AnimatorComponent.h"
#include "AnimController.h"
#include "ArrowComponent.h"
#include "BulletComponent.h"
#include "HealthComponent.h"
#include "AttackPowerComponent.h"

#include <string>
#include <sstream>

namespace
{
	enum : uint32_t
	{
		kCollisionLayerPlayer = 0,
		kCollisionLayerMonster = 1,
		kCollisionLayerWorldStatic = 2,
		kCollisionLayerPlayerWeapon = 3,
		kCollisionLayerMonsterWeapon = 4
	};

	const char* ColliderTypeToString(EColliderType type)
	{
		switch ( type )
		{
		case EColliderType::AABB:     return "AABB";
		case EColliderType::OOBB:     return "OOBB";
		case EColliderType::BSphere:  return "BSphere";
		case EColliderType::BCapsule: return "BCapsule";
		default:                      return "None";
		}
	}

	bool IsLocalPlayerObject(const CGameObject* obj)
	{
		if ( !obj ) return false;

		auto* tag = obj->GetComponent<CActorTagComponent>();
		if ( !tag ) return false;

		return ( tag->kind == EActorKind::Player &&
				tag->control == EPlayerControl::Local );
	}

	void DebugPrintCollision(CColliderComponent* a, CColliderComponent* b)
	{
		if ( !a || !b ) return;

		CGameObject* ownerA = a->GetOwner();
		CGameObject* ownerB = b->GetOwner();

		if ( !ownerA || !ownerB ) return;

		// 로컬 플레이어가 포함된 충돌만 출력
		if ( !IsLocalPlayerObject(ownerA) && !IsLocalPlayerObject(ownerB) )
			return;
	}

	bool IsMonsterBodyPlayerBodyPair(const CColliderComponent* a, const CColliderComponent* b)
	{
		if ( !a || !b ) return false;

		const uint32_t layerA = a->GetLayer();
		const uint32_t layerB = b->GetLayer();

		return
			( layerA == kCollisionLayerMonster && layerB == kCollisionLayerPlayer ) ||
			( layerA == kCollisionLayerPlayer && layerB == kCollisionLayerMonster );
	}

	bool IsBareHandMonsterWeaponPairCandidate(const CColliderComponent* a, const CColliderComponent* b)
	{
		if ( !IsMonsterBodyPlayerBodyPair(a, b) )
			return false;

		const CColliderComponent* monsterCollider =
			( a->GetLayer() == kCollisionLayerMonster ) ? a : b;

		const CColliderComponent* playerCollider =
			( monsterCollider == a ) ? b : a;

		if ( !monsterCollider->IsCollisionEnabled() ) return false;
		if ( !playerCollider->IsCollisionEnabled() ) return false;

		if ( !monsterCollider->AreWeaponCapsulesActive() )
			return false;

		if ( !monsterCollider->HasWeaponBoneCapsules() )
			return false;

		CGameObject* monsterOwner = monsterCollider->GetOwner();
		if ( !monsterOwner )
			return false;

		auto* hitbox = monsterOwner->GetComponent<CMonsterWeaponHitboxComponent>();
		if ( !hitbox )
			return false;

		return true;
	}
}

CCollisionSystem::CCollisionSystem()
{
}

void CCollisionSystem::SetHitEffectCallback(HitEffectCallback callback)
{
	mHitEffectCallback = std::move(callback);
}

void CCollisionSystem::RegisterCollider(CColliderComponent* c)
{
    if (!c) return;
    if (std::find(mColliders.begin(), mColliders.end(), c) != mColliders.end())
        return;
    mColliders.push_back(c);
}

void CCollisionSystem::UnregisterCollider(CColliderComponent* c)
{
    if (!c) return;
    auto it = std::remove(mColliders.begin(), mColliders.end(), c);
    mColliders.erase(it, mColliders.end());
}

size_t CCollisionSystem::GetColiidersNum() const
{
	return mColliders.size();
}

const std::vector<CColliderComponent*>& CCollisionSystem::GetColliders() const
{
	return mColliders;
}

bool CCollisionSystem::PassFilter(const CColliderComponent* a, const CColliderComponent* b) const
{
	if ( !a || !b ) return false;

	if ( !a->IsCollisionEnabled() ) return false;
	if ( !b->IsCollisionEnabled() ) return false;

	const uint32_t bitA = ( 1u << a->GetLayer() );
	const uint32_t bitB = ( 1u << b->GetLayer() );

	if ( ( a->GetMask() & bitB ) == 0 ) return false;
	if ( ( b->GetMask() & bitA ) == 0 ) return false;
	return true;
}

bool CCollisionSystem::IsPairIntersecting(const CColliderComponent* a, const CColliderComponent* b) const
{
	if ( !a || !b )
		return false;

	if ( !PassFilter(a, b) )
		return false;

	const uint32_t layerA = a->GetLayer();
	const uint32_t layerB = b->GetLayer();

	// BCapsule vs BCapsule
	if ( a->GetType() == EColliderType::BCapsule &&
		b->GetType() == EColliderType::BCapsule )
	{
		return a->GetBCapsule().Intersects(b->GetBCapsule());
	}

	// BCapsule vs OOBB
	if ( a->GetType() == EColliderType::BCapsule &&
		b->GetType() == EColliderType::OOBB )
	{
		// body vs world static
		if ( layerB == kCollisionLayerWorldStatic )
			return b->IntersectsCapsuleHierarchical(a->GetBCapsule());

		// body vs weapon OOBB
		return a->IntersectsBoneCapsulesHierarchical(b->GetOOBB());
	}

	// OOBB vs BCapsule
	if ( a->GetType() == EColliderType::OOBB &&
		b->GetType() == EColliderType::BCapsule )
	{
		// world static vs body
		if ( layerA == kCollisionLayerWorldStatic )
			return a->IntersectsCapsuleHierarchical(b->GetBCapsule());

		// weapon OOBB vs body
		return b->IntersectsBoneCapsulesHierarchical(a->GetOOBB());
	}

	// BCapsule vs BSphere
	if ( a->GetType() == EColliderType::BCapsule &&
		b->GetType() == EColliderType::BSphere )
	{
		// body vs projectile sphere
		return a->IntersectsBoneCapsulesHierarchical(b->GetBSphere());
	}

	// BSphere vs BCapsule
	if ( a->GetType() == EColliderType::BSphere &&
		b->GetType() == EColliderType::BCapsule )
	{
		// projectile sphere vs body
		return b->IntersectsBoneCapsulesHierarchical(a->GetBSphere());
	}

	return false;
}

bool CCollisionSystem::HasCollisionWithWorldStatic(const CColliderComponent* subject) const
{
	if ( !subject )
		return false;

	for ( CColliderComponent* other : mColliders )
	{
		if ( !other ) continue;
		if ( other == subject ) continue;

		if ( other->GetLayer() != kCollisionLayerWorldStatic )
			continue;

		if ( IsPairIntersecting(subject, other) )
			return true;
	}

	return false;
}

void CCollisionSystem::HandlePair(CColliderComponent* a, CColliderComponent* b)
{
	if ( !a || !b )
		return;

	CGameObject* ownerA = a->GetOwner();
	CGameObject* ownerB = b->GetOwner();
	if ( !ownerA || !ownerB )
		return;

	const uint32_t layerA = a->GetLayer();
	const uint32_t layerB = b->GetLayer();

	// true 로 바꾸면 몬스터는 맞는 즉시 Death 테스트 가능
	constexpr bool kTestForceDeathOnHit = false;

	auto RequestPlayerHitAnimation = [ & ] (CGameObject* playerObject)
		{
			if ( !playerObject )
				return;

			if ( auto* animComp = playerObject->GetComponent<CAnimatorComponent>() )
			{
				if ( auto* ctrl = animComp->EnsureController() )
				{
					ctrl->RequestHit();
					return;
				}
			}

			if ( auto* ctrl = playerObject->GetAnimController() )
			{
				ctrl->RequestHit();
			}
		};

	auto GetAttackPower = [ ] (CGameObject* weaponObject) -> int
		{
			if ( !weaponObject )
				return 0;

			auto* attack = weaponObject->GetComponent<CAttackPowerComponent>();
			if ( !attack )
				return 0;

			return attack->GetAttackPower();
		};

	auto IsDeadByHealth = [ ] (CGameObject* obj) -> bool
		{
			if ( !obj )
				return true;

			auto* hp = obj->GetComponent<CHealthComponent>();
			if ( !hp )
				return false;

			return hp->IsDead();
		};

	auto ApplyDamage = [ & ] (CGameObject* weaponObject, CGameObject* targetObject) -> bool
		{
			if ( !weaponObject || !targetObject )
				return false;

			if ( IsDeadByHealth(targetObject) )
				return false;

			const int damage = GetAttackPower(weaponObject);
			if ( damage <= 0 )
				return false;

			auto* hp = targetObject->GetComponent<CHealthComponent>();
			if ( !hp )
				return false;

			return hp->TakeDamage(damage);
		};
	auto EmitHitEffect = [ this ](
		CGameObject* weaponObject,
		CGameObject* targetObject)
		{
			if ( mHitEffectCallback )
				mHitEffectCallback(weaponObject, targetObject);
		};

	auto NotifyMonsterHit = [ & ] (CGameObject* weaponObject, CGameObject* monsterObject)
		{
			if ( !weaponObject || !monsterObject )
				return;

			if ( IsDeadByHealth(monsterObject) )
				return;

			auto DeactivateProjectileIfNeeded = [ ] (CGameObject* weaponObj)
				{
					if ( !weaponObj ) return;

					if ( auto* arrow = weaponObj->GetComponent<CArrowComponent>() )
					{
						arrow->Deactivate();
						return;
					}

					if ( auto* bullet = weaponObj->GetComponent<CBulletComponent>() )
					{
						bullet->Deactivate();
						return;
					}
				};

			if ( auto* hitbox = weaponObject->GetComponent<CWeaponHitboxComponent>() )
			{
				if ( !hitbox->CanHitTarget(monsterObject) )
					return;

				auto* combat = monsterObject->GetComponent<CMonsterCombatComponent>();
				auto* hp = monsterObject->GetComponent<CHealthComponent>();

				const bool damaged = ApplyDamage(weaponObject, monsterObject);

				const bool deadByHp = ( hp && hp->IsDead() );

				if ( damaged )
					EmitHitEffect(weaponObject, monsterObject);

				if ( combat )
					combat->OnHitByPlayerWeapon(weaponObject, kTestForceDeathOnHit || deadByHp);

				hitbox->MarkHitTarget(monsterObject);
				DeactivateProjectileIfNeeded(weaponObject);
				return;
			}

			const bool isProjectile =
				weaponObject->GetComponent<CArrowComponent>() ||
				weaponObject->GetComponent<CBulletComponent>();

			// 검/도끼 같은 melee weapon은 반드시 CWeaponHitboxComponent를 통해서만 데미지를 넣는다.
			// 이 컴포넌트가 없는 비투사체 weapon은 fallback 경로로 매 프레임 데미지를 줄 수 있으므로 차단한다.
			if ( !isProjectile )
				return;

			auto* combat = monsterObject->GetComponent<CMonsterCombatComponent>();
			auto* hp = monsterObject->GetComponent<CHealthComponent>();

			const bool damaged = ApplyDamage(weaponObject, monsterObject);

			const bool deadByHp = ( hp && hp->IsDead() );

			if ( damaged )
				EmitHitEffect(weaponObject, monsterObject);

			if ( combat )
				combat->OnHitByPlayerWeapon(weaponObject, kTestForceDeathOnHit || deadByHp);

			DeactivateProjectileIfNeeded(weaponObject);
		};

	auto NotifyPlayerHit = [ & ] (CGameObject* weaponObject, CGameObject* playerObject)
		{
			if ( IsDeadByHealth(playerObject) )
				return;

			if ( !weaponObject || !playerObject )
				return;

			auto DeactivateProjectileIfNeeded = [ ] (CGameObject* weaponObj)
				{
					if ( !weaponObj ) return;

					if ( auto* arrow = weaponObj->GetComponent<CArrowComponent>() )
					{
						arrow->Deactivate();
						return;
					}

					if ( auto* bullet = weaponObj->GetComponent<CBulletComponent>() )
					{
						bullet->Deactivate();
						return;
					}
				};

			if ( auto* hitbox = weaponObject->GetComponent<CMonsterWeaponHitboxComponent>() )
			{
				if ( !hitbox->CanHitTarget(playerObject) )
					return;

				const bool damaged = ApplyDamage(weaponObject, playerObject);

				const bool deadAfterHit = IsDeadByHealth(playerObject);

				if ( damaged )
					EmitHitEffect(weaponObject, playerObject);

#ifndef USING_NETWORK
				if ( !deadAfterHit )
					RequestPlayerHitAnimation(playerObject);
#endif

				hitbox->MarkHitTarget(playerObject);
				DeactivateProjectileIfNeeded(weaponObject);
				return;
			}

			const bool damaged = ApplyDamage(weaponObject, playerObject);

			const bool deadAfterHit = IsDeadByHealth(playerObject);

			if ( damaged )
				EmitHitEffect(weaponObject, playerObject);

#ifndef USING_NETWORK
			if ( !deadAfterHit )
				RequestPlayerHitAnimation(playerObject);
#endif

			DeactivateProjectileIfNeeded(weaponObject);
		};

	// ------------------------------------------------------------
	// Bare-hand Monster body weapon capsules -> Player
	// ------------------------------------------------------------
	if ( IsMonsterBodyPlayerBodyPair(a, b) )
	{
		CColliderComponent* monsterCollider =
			( layerA == kCollisionLayerMonster ) ? a : b;

		CColliderComponent* playerCollider =
			( monsterCollider == a ) ? b : a;

		if ( monsterCollider->AreWeaponCapsulesActive() &&
			 monsterCollider->HasWeaponBoneCapsules() &&
			 monsterCollider->IntersectsActiveWeaponBoneCapsulesAgainstBody(*playerCollider) )
		{
			CGameObject* monsterObject = monsterCollider->GetOwner();
			CGameObject* playerObject = playerCollider->GetOwner();

			if ( !monsterObject || !playerObject )
				return;

			DebugPrintCollision(monsterCollider, playerCollider);

			NotifyPlayerHit(monsterObject, playerObject);
			return;
		}
	}

	const bool isHit = IsPairIntersecting(a, b);
	if ( !isHit )
		return;

	if ( a->IsTrigger() || b->IsTrigger() )
		return;

	DebugPrintCollision(a, b);

	// ------------------------------------------------------------
	// PlayerWeapon -> Monster
	// ------------------------------------------------------------
	if ( layerA == kCollisionLayerPlayerWeapon &&
		 layerB == kCollisionLayerMonster )
	{
		NotifyMonsterHit(ownerA, ownerB);
		return;
	}

	if ( layerA == kCollisionLayerMonster &&
		 layerB == kCollisionLayerPlayerWeapon )
	{
		NotifyMonsterHit(ownerB, ownerA);
		return;
	}

	// ------------------------------------------------------------
	// MonsterWeapon -> Player
	// ------------------------------------------------------------
	if ( layerA == kCollisionLayerMonsterWeapon &&
		 layerB == kCollisionLayerPlayer )
	{
		NotifyPlayerHit(ownerA, ownerB);
		return;
	}

	if ( layerA == kCollisionLayerPlayer &&
		 layerB == kCollisionLayerMonsterWeapon )
	{
		NotifyPlayerHit(ownerB, ownerA);
		return;
	}
}

void CCollisionSystem::OnUpdate()
{
    // 전수검사: i<j
    
	const size_t n = mColliders.size();

    for (size_t i = 0; i < n; ++i)
    {
        auto* a = mColliders[i];
		if ( !a )continue;

        for (size_t j = i + 1; j < n; ++j)
        {
            auto* b = mColliders[j];

            if (!b) 
				continue;

			const bool normalFilteredPair = PassFilter(a, b);
			const bool bareHandPair = IsBareHandMonsterWeaponPairCandidate(a, b);

            HandlePair(a, b);
        }
    }
}