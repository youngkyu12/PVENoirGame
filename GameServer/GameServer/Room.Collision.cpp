#include "pch.h"
#include "Room.h"
#include "Building.h"
#include "ColliderComponent.h"
#include "ReportHelper.h"

namespace
{
	enum : uint32
	{
		kCollisionLayerCharacter = 0,
		kCollisionLayerWorldStatic = 1
	};

	static constexpr uint32 CollisionBit(uint32 layer)
	{
		return (1u << layer);
	}

	static bool ShouldCreateWorldStaticCollider(Protocol::BuildingType type)
	{
		switch (type)
		{
		case Protocol::BUILDING_TYPE_GRASS:
		case Protocol::BUILDING_TYPE_GROUND:
		case Protocol::BUILDING_TYPE_DIRT_ROAD:
			return false;
		default:
			return true;
		}
	}

	static void GetStaticBuildingBounds(Protocol::BuildingType type, XMFLOAT3& outMin, XMFLOAT3& outMax)
	{
		switch (type)
		{
		case Protocol::BUILDING_TYPE_VILLAGE_WALL:
			outMin = XMFLOAT3(-2.5f, 0.0f, -0.5f);
			outMax = XMFLOAT3(2.5f, 2.5f, 0.5f);
			return;
		case Protocol::BUILDING_TYPE_BUILDING1:
		case Protocol::BUILDING_TYPE_BUILDING2:
		case Protocol::BUILDING_TYPE_BUILDING3:
		case Protocol::BUILDING_TYPE_BUILDING4:
		case Protocol::BUILDING_TYPE_BUILDING5:
		case Protocol::BUILDING_TYPE_BUILDING6:
		case Protocol::BUILDING_TYPE_BUILDING7:
		case Protocol::BUILDING_TYPE_BUILDING8:
		case Protocol::BUILDING_TYPE_BUILDING9:
			outMin = XMFLOAT3(-4.5f, -3.5f, -4.5f);
			outMax = XMFLOAT3(4.5f, 7.0f, 4.5f);
			return;
		case Protocol::BUILDING_TYPE_TOWER:
			outMin = XMFLOAT3(-1.0f, 0.0f, -1.0f);
			outMax = XMFLOAT3(1.0f, 6.0f, 1.0f);
			return;
		default:
			outMin = XMFLOAT3(-1.5f, 0.0f, -1.5f);
			outMax = XMFLOAT3(1.5f, 3.5f, 1.5f);
			return;
		}
	}

	static StaticWorldReportCache g_staticWorldReportCache;
	static bool g_staticWorldReportLoaded = false;

	static void EnsureStaticWorldReportLoaded()
	{
		if (g_staticWorldReportLoaded)
			return;

		const std::vector<std::string> candidates = {
			"MapFIle/StaticWorldLocalOOBBReport.txt",
			"GameServer/MapFIle/StaticWorldLocalOOBBReport.txt",
			"../GameServer/MapFIle/StaticWorldLocalOOBBReport.txt"
		};

		ReportHelper::LoadStaticWorldOverallLocalOOBBReport(candidates, g_staticWorldReportCache);
		g_staticWorldReportLoaded = true;
	}
}

void Room::InitializeCollisionSystem()
{
	_collision = make_unique<CCollisionSystem>();
}

void Room::RegisterDynamicCollider(const shared_ptr<CServerObject>& obj)
{
	if (!obj)
		return;

	auto* collider = obj->GetComponent<CColliderComponent>();
	if (!collider)
	{
		collider = obj->AddComponent<CColliderComponent>(EColliderType::BCapsule);
		if (collider)
		{
			collider->SetLayer(kCollisionLayerCharacter);
			collider->SetMask(CollisionBit(kCollisionLayerCharacter) | CollisionBit(kCollisionLayerWorldStatic));
			collider->SetBCapsule(XMFLOAT3(-0.4f, 0.0f, -0.4f), XMFLOAT3(0.4f, 1.8f, 0.4f));
		}
	}

	obj->CreateComponents();

	if (collider)
	{
		collider->OnUpdate(0.0f);
		if (_collision)
			_collision->RegisterCollider(collider);
	}
}

void Room::RegisterStaticCollider(BuildingRef building)
{
	if (!building)
		return;

	if (!ShouldCreateWorldStaticCollider(building->GetBuildingType()))
		return;

	auto* collider = building->GetComponent<CColliderComponent>();
	if (!collider)
	{
		collider = building->AddComponent<CColliderComponent>(EColliderType::OOBB);
		if (collider)
		{
			bool appliedFromReport = false;
			EnsureStaticWorldReportLoaded();

			const ReportObjectOOBB* report = ReportHelper::FindByBuildingType(
				g_staticWorldReportCache,
				building->GetBuildingType());

			if (report)
			{
				collider->SetOOBB(report->localOOBB);
				collider->SetSubOOBBs(report->localSubOOBBs);
				appliedFromReport = true;
			}

			if (!appliedFromReport)
			{
				XMFLOAT3 minV{};
				XMFLOAT3 maxV{};
				GetStaticBuildingBounds(building->GetBuildingType(), minV, maxV);
				collider->SetOOBB(minV, maxV);
				collider->ClearSubOOBBs();
			}

			collider->SetLayer(kCollisionLayerWorldStatic);
			collider->SetMask(CollisionBit(kCollisionLayerCharacter));
		}
	}

	building->CreateComponents();

	if (collider)
	{
		collider->OnUpdate(0.0f);
		if (_collision)
			_collision->RegisterCollider(collider);
	}
}

void Room::ResolveWorldStaticCollision(const shared_ptr<CServerObject>& obj, const GameMath::Vec3& previousPos)
{
	if (!_collision || !obj)
		return;

	auto* collider = obj->GetComponent<CColliderComponent>();
	if (!collider)
		return;

	collider->OnUpdate(0.0f);
	if (_collision->HasCollisionWithWorldStatic(collider))
	{
		obj->SetPosition(previousPos);
		collider->OnUpdate(0.0f);
	}
}

GameMath::Vec3 Room::ResolvePreBlockedShift(const shared_ptr<CServerObject>& obj, const GameMath::Vec3& desiredShift)
{
	if (!_collision || !obj)
		return desiredShift;

	if (desiredShift.LengthSq() <= 1e-8f)
		return desiredShift;

	auto* collider = obj->GetComponent<CColliderComponent>();
	if (!collider)
		return desiredShift;

	const GameMath::Vec3 originPos = obj->GetPosition();

	auto WouldCollideAt = [&](const GameMath::Vec3& testPos) -> bool
		{
			obj->SetPosition(testPos);
			collider->OnUpdate(0.0f);
			return _collision->HasCollisionWithWorldStatic(collider);
		};

	GameMath::Vec3 resolvedShift = desiredShift;
	const GameMath::Vec3 desiredPos = originPos + desiredShift;

	if (WouldCollideAt(desiredPos))
	{
		GameMath::Vec3 xOnlyPos = originPos;
		xOnlyPos.x += desiredShift.x;

		GameMath::Vec3 zOnlyPos = originPos;
		zOnlyPos.z += desiredShift.z;

		const bool canMoveX = (fabsf(desiredShift.x) > 1e-8f) && !WouldCollideAt(xOnlyPos);
		const bool canMoveZ = (fabsf(desiredShift.z) > 1e-8f) && !WouldCollideAt(zOnlyPos);

		if (canMoveX && canMoveZ)
		{
			resolvedShift = (fabsf(desiredShift.x) >= fabsf(desiredShift.z))
				? GameMath::Vec3(desiredShift.x, desiredShift.y, 0.0f)
				: GameMath::Vec3(0.0f, desiredShift.y, desiredShift.z);
		}
		else if (canMoveX)
		{
			resolvedShift = GameMath::Vec3(desiredShift.x, desiredShift.y, 0.0f);
		}
		else if (canMoveZ)
		{
			resolvedShift = GameMath::Vec3(0.0f, desiredShift.y, desiredShift.z);
		}
		else
		{
			resolvedShift = GameMath::Vec3::Zero();
		}
	}

	obj->SetPosition(originPos);
	collider->OnUpdate(0.0f);

	return resolvedShift;
}
