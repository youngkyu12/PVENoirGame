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
        case Protocol::BUILDING_TYPE_CASTLE:
			outMin = XMFLOAT3(-8.0f, 0.0f, -8.0f);
			outMax = XMFLOAT3(8.0f, 10.0f, 8.0f);
			return;
		default:
			outMin = XMFLOAT3(-1.5f, 0.0f, -1.5f);
			outMax = XMFLOAT3(1.5f, 3.5f, 1.5f);
			return;
		}
	}

	static bool GetColliderWorldXZBounds(
		const CColliderComponent& collider,
		float& outMinX,
		float& outMaxX,
		float& outMinZ,
		float& outMaxZ)
	{
		bool hasBounds = false;

		auto IncludePoint = [&](float x, float z)
			{
				if (!hasBounds)
				{
					outMinX = outMaxX = x;
					outMinZ = outMaxZ = z;
					hasBounds = true;
					return;
				}

				outMinX = (std::min)(outMinX, x);
				outMaxX = (std::max)(outMaxX, x);
				outMinZ = (std::min)(outMinZ, z);
				outMaxZ = (std::max)(outMaxZ, z);
			};

		auto IncludeOOBB = [&](const BoundingOrientedBox& box)
			{
				XMFLOAT3 corners[8] = {};
				box.GetCorners(corners);
				for (const XMFLOAT3& corner : corners)
					IncludePoint(corner.x, corner.z);
			};

		auto IncludeCapsule = [&](const BoundingCapsule& capsule)
			{
				IncludePoint(capsule.p0.x - capsule.Radius, capsule.p0.z - capsule.Radius);
				IncludePoint(capsule.p0.x + capsule.Radius, capsule.p0.z + capsule.Radius);
				IncludePoint(capsule.p1.x - capsule.Radius, capsule.p1.z - capsule.Radius);
				IncludePoint(capsule.p1.x + capsule.Radius, capsule.p1.z + capsule.Radius);
			};

		switch (collider.GetType())
		{
		case EColliderType::AABB:
		{
			const BoundingBox& box = collider.GetAABB();
			IncludePoint(box.Center.x - box.Extents.x, box.Center.z - box.Extents.z);
			IncludePoint(box.Center.x + box.Extents.x, box.Center.z + box.Extents.z);
			break;
		}
		case EColliderType::OOBB:
		{
			IncludeOOBB(collider.GetOOBB());
			for (const BoundingOrientedBox& subBox : collider.GetSubOOBBs())
				IncludeOOBB(subBox);
			break;
		}
		case EColliderType::BSphere:
		{
			const BoundingSphere& sphere = collider.GetBSphere();
			IncludePoint(sphere.Center.x - sphere.Radius, sphere.Center.z - sphere.Radius);
			IncludePoint(sphere.Center.x + sphere.Radius, sphere.Center.z + sphere.Radius);
			break;
		}
		case EColliderType::BCapsule:
		{
			IncludeCapsule(collider.GetBCapsule());
			for (const BoundingCapsule& subCapsule : collider.GetSubBCapsules())
				IncludeCapsule(subCapsule);
			break;
		}
		default:
			break;
		}

		return hasBounds;
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

bool Room::HasCollisionWithNearbyWorldStatic(const CColliderComponent* subject) const
{
	if (!_collision || !subject)
		return false;

	if (!m_spatialGridInitialized)
		return _collision->HasCollisionWithWorldStatic(subject);

	float minX = 0.0f;
	float maxX = 0.0f;
	float minZ = 0.0f;
	float maxZ = 0.0f;
	if (!GetColliderWorldXZBounds(*subject, minX, maxX, minZ, maxZ))
		return _collision->HasCollisionWithWorldStatic(subject);

	std::vector<uint64> buildingIds;
	CollectStaticBuildingIdsForWorldBounds(minX, maxX, minZ, maxZ, buildingIds);

	for (uint64 buildingId : buildingIds)
	{
		auto it = buildings.find(buildingId);
		if (it == buildings.end()) continue;

		const BuildingRef& building = it->second;
		if (!building) continue;

		auto* candidate = building->GetComponent<CColliderComponent>();
		if (!candidate) continue;

		if (_collision->TestIntersection(subject, candidate))
			return true;
	}

	return false;
}
void Room::ResolveWorldStaticCollision(const shared_ptr<CServerObject>& obj, const GameMath::Vec3& previousPos)
{
	if (!_collision || !obj)
		return;

	auto* collider = obj->GetComponent<CColliderComponent>();
	if (!collider)
		return;

	collider->OnUpdate(0.0f);
	if (HasCollisionWithNearbyWorldStatic(collider))
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
			return HasCollisionWithNearbyWorldStatic(collider);
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
