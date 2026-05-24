#include "pch.h"
#include "Room.h"
#include "Building.h"
#include "ColliderComponent.h"
#include "Player.h"
#include "Enemy.h"
#include "ReportHelper.h"

#include <array>
#include <cfloat>

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

	constexpr int kCastleCenterMegaGridX = 1;
	constexpr int kCastleCenterMegaGridZ = 1;
	constexpr int kTowerDoorPortalCooldownTicks = 30;
	constexpr int kCastleDoorPortalCooldownTicks = 30;
	constexpr float kTowerDoorPortalExitOffset = 2.0f;
	constexpr float kCastleDoorPortalExitOffset = 2.0f;
	constexpr float kTowerDoorPortalLowerExitYOffset = 0.0f;
	constexpr float kTowerDoorPortalUpperExitYOffset = 3.5f;
	constexpr float kTowerDoorPortalUpperHeightThreshold = 10.0f;
	constexpr int kRequiredClearedMegaGridCountForCastlePortal = 4;

	// Client: "Double Door Frame"
	constexpr std::array<size_t, 3> kTowerDoorPortalLowerDoorSubIndices = { 5, 6, 7 };

	// Client: "Double Door Frame2"; sub-index 11 is the upper spot/back wall and is not a trigger.
	constexpr std::array<size_t, 3> kTowerDoorPortalUpperDoorSubIndices = { 8, 9, 10 };

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

	static bool ComputeDoorHorizontalNormal(const BoundingOrientedBox& box, XMVECTOR& outNormal)
	{
		XMVECTOR localAxis = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		if (box.Extents.x < box.Extents.z)
			localAxis = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

		XMVECTOR q = XMLoadFloat4(&box.Orientation);
		XMVECTOR normal = XMVector3Rotate(localAxis, q);
		normal = XMVectorSetY(normal, 0.0f);

		const float lenSq = XMVectorGetX(XMVector3LengthSq(normal));
		if (lenSq <= 1.0e-6f)
			return false;

		outNormal = XMVector3Normalize(normal);
		return true;
	}

	static float YawFromHorizontalDirection(XMVECTOR dir)
	{
		dir = XMVectorSetY(dir, 0.0f);
		const float lenSq = XMVectorGetX(XMVector3LengthSq(dir));
		if (lenSq <= 1.0e-6f)
			return 0.0f;

		dir = XMVector3Normalize(dir);
		return GameMath::NormalizeYaw(
			atan2f(XMVectorGetX(dir), XMVectorGetZ(dir)) * GameMath::RAD_TO_DEG);
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
		collider->SetCollisionMegaGridMask(ComputeObjectCurrentMegaGridMask(obj.get()));
		collider->SetCollisionMegaGridMaskFixed(false);
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
		collider->SetCollisionMegaGridMask(ComputeStaticBuildingMegaGridMask(building));
		collider->SetCollisionMegaGridMaskFixed(true);
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

void Room::RegisterDoorPortal(BuildingRef building)
{
	if (!building)
		return;

	auto* collider = building->GetComponent<CColliderComponent>();
	if (!collider)
		return;

	collider->OnUpdate(0.0f);
	const auto& subBoxes = collider->GetSubOOBBs();

	if (building->GetBuildingType() == Protocol::BUILDING_TYPE_TOWER)
	{
		if (subBoxes.size() <= kTowerDoorPortalUpperDoorSubIndices.back())
			return;

		TowerDoorPortalEntry entry{};
		entry.tower = building;
		entry.collider = collider;
		for (size_t subIndex : kTowerDoorPortalLowerDoorSubIndices)
			entry.doorARefs.push_back(DoorPortalSubBoxRef{ subIndex });

		for (size_t subIndex : kTowerDoorPortalUpperDoorSubIndices)
			entry.doorBRefs.push_back(DoorPortalSubBoxRef{ subIndex });
		m_towerDoorPortals.push_back(std::move(entry));
		return;
	}

	if (building->GetBuildingType() != Protocol::BUILDING_TYPE_CASTLE)
		return;

	if (subBoxes.size() <= 163)
		return;

	CastleDoorPortalEntry entry{};
	entry.castle = building;
	entry.collider = collider;

	const std::array<std::array<size_t, 3>, 8> doorSubIndices = {
		std::array<size_t, 3>{ 140, 141, 142 },
		std::array<size_t, 3>{ 143, 144, 145 },
		std::array<size_t, 3>{ 146, 147, 148 },
		std::array<size_t, 3>{ 149, 150, 151 },
		std::array<size_t, 3>{ 152, 153, 154 },
		std::array<size_t, 3>{ 155, 156, 157 },
		std::array<size_t, 3>{ 158, 159, 160 },
		std::array<size_t, 3>{ 161, 162, 163 }
	};

	for (size_t doorIndex = 0; doorIndex < doorSubIndices.size(); ++doorIndex)
	{
		for (size_t subIndex : doorSubIndices[doorIndex])
			entry.doorRefsByIndex[doorIndex].push_back(DoorPortalSubBoxRef{ subIndex });
	}

	auto AddPair = [&](int source, int target)
		{
			CastleDoorPortalPair pair{};
			pair.sourceDoorIndex = source;
			pair.targetDoorIndex = target;
			pair.sourceRefs = entry.doorRefsByIndex[static_cast<size_t>(source)];
			pair.targetRefs = entry.doorRefsByIndex[static_cast<size_t>(target)];
			entry.pairs.push_back(std::move(pair));
		};

	AddPair(1, 0);
	AddPair(2, 4);
	AddPair(3, 5);
	AddPair(7, 6);

	m_castleDoorPortals.push_back(std::move(entry));
}

void Room::TickDoorPortalCooldowns()
{
	for (TowerDoorPortalEntry& portal : m_towerDoorPortals)
	{
		if (portal.cooldownTicks > 0)
			--portal.cooldownTicks;
	}

	for (CastleDoorPortalEntry& portal : m_castleDoorPortals)
	{
		if (portal.cooldownTicks > 0)
			--portal.cooldownTicks;
	}
}

int Room::CountClearedMegaGrids() const
{
	int clearedCount = 0;

	for (int z = 0; z < kMegaGridRows; ++z)
	{
		for (int x = 0; x < kMegaGridCols; ++x)
		{
			const MegaGridCell& cell = m_megaGridCells[static_cast<size_t>(MegaGridIndex(x, z))];
			if (cell.isCleared)
			{
				++clearedCount;
				continue;
			}

			if (cell.enemyIds.empty())
				continue;

			bool allDead = true;
			for (uint64 enemyId : cell.enemyIds)
			{
				auto it = enemies.find(enemyId);
				if (it == enemies.end() || !it->second || it->second->IsDead())
					continue;

				allDead = false;
				break;
			}

			if (allDead)
				++clearedCount;
		}
	}

	return clearedCount;
}

bool Room::CanUseCastleDoorPortal() const
{
	return CountClearedMegaGrids() >= kRequiredClearedMegaGridCountForCastlePortal;
}

void Room::MarkPlayerEnteredCastleCenterMegaGrid(uint64 playerId)
{
	if (!m_spatialGridInitialized)
		return;

	m_castleCenterPlayerIds.insert(playerId);
	m_megaGridCells[static_cast<size_t>(MegaGridIndex(kCastleCenterMegaGridX, kCastleCenterMegaGridZ))].hasPlayerApproached = true;
}

bool Room::IsPlayerInsideCastleCenterMegaGridFullArea(uint64 playerId) const
{
	if (!m_spatialGridInitialized)
		return false;

	const GridDynamicTracker* tracker = nullptr;
	for (const GridDynamicTracker& candidate : m_playerGridTrackers)
	{
		if (!candidate.occupied || !candidate.object)
			continue;

		if (candidate.object->GetObjectId() != playerId)
			continue;

		tracker = &candidate;
		break;
	}

	if (!tracker)
		return false;

	int megaX = -1;
	int megaZ = -1;
	if (!FineCellToMegaGridCell(tracker->prevCellX, tracker->prevCellZ, megaX, megaZ))
		return false;

	return megaX == kCastleCenterMegaGridX && megaZ == kCastleCenterMegaGridZ;
}

void Room::UpdateCastleCenterMegaGridState()
{
	if (!m_spatialGridInitialized)
		return;

	for (auto it = m_castleCenterPlayerIds.begin(); it != m_castleCenterPlayerIds.end();)
	{
		if (IsPlayerInsideCastleCenterMegaGridFullArea(*it))
		{
			m_megaGridCells[static_cast<size_t>(MegaGridIndex(kCastleCenterMegaGridX, kCastleCenterMegaGridZ))].hasPlayerApproached = true;
			++it;
			continue;
		}

		it = m_castleCenterPlayerIds.erase(it);
	}

	if (m_castleCenterPlayerIds.empty())
		m_megaGridCells[static_cast<size_t>(MegaGridIndex(kCastleCenterMegaGridX, kCastleCenterMegaGridZ))].hasPlayerApproached = false;
}

void Room::ProcessDoorPortals()
{
	TickDoorPortalCooldowns();

	for (auto& [pid, player] : players)
	{
		if (!player || player->IsDead())
			continue;

		if (TryTeleportPlayerByTowerDoorPortal(player))
			continue;

		TryTeleportPlayerByCastleDoorPortal(player);
	}
}

bool Room::TryQueuePortalTeleportFromBlockedMove(const PlayerRef& player, const GameMath::Vec3& desiredShift)
{
	if (!player || player->IsDead() || player->HasPendingPortalTeleport())
		return false;

	auto* playerCollider = player->GetComponent<CColliderComponent>();
	if (!playerCollider || playerCollider->GetType() != EColliderType::BCapsule)
		return false;

	const GameMath::Vec3 originPos = player->GetPosition();
	const GameMath::Vec3 testPos = originPos + desiredShift;

	player->SetPosition(testPos);
	playerCollider->OnUpdate(0.0f);

	const bool blocked = HasCollisionWithNearbyWorldStatic(playerCollider);
	const bool queued =
		blocked &&
		(TryQueueTowerDoorPortalTeleport(player) ||
		 TryQueueCastleDoorPortalTeleport(player));

	player->SetPosition(originPos);
	playerCollider->OnUpdate(0.0f);

	return queued;
}

bool Room::TryQueueTowerDoorPortalTeleport(const PlayerRef& player)
{
	if (!player || player->IsDead())
		return false;

	auto* playerCollider = player->GetComponent<CColliderComponent>();
	if (!playerCollider || playerCollider->GetType() != EColliderType::BCapsule)
		return false;

	playerCollider->OnUpdate(0.0f);
	const BoundingCapsule playerCapsule = playerCollider->GetBCapsule();
	const GameMath::Vec3 playerPos = player->GetPosition();

	auto GetWorldBox = [](const TowerDoorPortalEntry& portal, const DoorPortalSubBoxRef& ref) -> const BoundingOrientedBox*
		{
			if (!portal.collider)
				return nullptr;

			const auto& boxes = portal.collider->GetSubOOBBs();
			if (ref.subIndex >= boxes.size())
				return nullptr;

			return &boxes[ref.subIndex];
		};

	auto DoesDoorGroupIntersect = [&](const TowerDoorPortalEntry& portal, const std::vector<DoorPortalSubBoxRef>& refs) -> bool
		{
			for (const DoorPortalSubBoxRef& ref : refs)
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if (box && playerCapsule.Intersects(*box))
					return true;
			}

			return false;
		};

	auto ComputeDoorGroupCenter = [&](const TowerDoorPortalEntry& portal, const std::vector<DoorPortalSubBoxRef>& refs, XMFLOAT3& outCenter) -> bool
		{
			XMVECTOR sum = XMVectorZero();
			int count = 0;

			for (const DoorPortalSubBoxRef& ref : refs)
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if (!box)
					continue;

				sum += XMLoadFloat3(&box->Center);
				++count;
			}

			if (count <= 0)
				return false;

			XMStoreFloat3(&outCenter, XMVectorScale(sum, 1.0f / static_cast<float>(count)));
			return true;
		};

	auto ComputeDoorGroupBottomY = [&](const TowerDoorPortalEntry& portal, const std::vector<DoorPortalSubBoxRef>& refs, float& outBottomY) -> bool
		{
			bool found = false;
			float bottomY = FLT_MAX;

			for (const DoorPortalSubBoxRef& ref : refs)
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if (!box)
					continue;

				XMFLOAT3 corners[BoundingOrientedBox::CORNER_COUNT] = {};
				box->GetCorners(corners);
				for (const XMFLOAT3& corner : corners)
				{
					if (!found || corner.y < bottomY)
					{
						bottomY = corner.y;
						found = true;
					}
				}
			}

			if (!found)
				return false;

			outBottomY = bottomY;
			return true;
		};

	auto QueueBetweenDoorGroups = [&](
		TowerDoorPortalEntry& portal,
		const std::vector<DoorPortalSubBoxRef>& sourceRefs,
		const std::vector<DoorPortalSubBoxRef>& targetRefs) -> bool
		{
			XMFLOAT3 sourceCenter{};
			XMFLOAT3 targetCenter{};
			if (!ComputeDoorGroupCenter(portal, sourceRefs, sourceCenter))
				return false;
			if (!ComputeDoorGroupCenter(portal, targetRefs, targetCenter))
				return false;

			const BoundingOrientedBox* targetBox = targetRefs.empty() ? nullptr : GetWorldBox(portal, targetRefs.front());

			XMVECTOR sourceV = XMLoadFloat3(&sourceCenter);
			XMVECTOR targetV = XMLoadFloat3(&targetCenter);
			XMVECTOR targetNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			const bool hasTargetNormal = targetBox && ComputeDoorHorizontalNormal(*targetBox, targetNormal);

			XMVECTOR exitDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			if (hasTargetNormal)
			{
				exitDir = XMVectorNegate(targetNormal);
			}
			else
			{
				exitDir = XMVectorSetY(targetV - sourceV, 0.0f);
				const float lenSq = XMVectorGetX(XMVector3LengthSq(exitDir));
				exitDir = (lenSq > 1.0e-6f) ? XMVector3Normalize(exitDir) : XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			}

			XMFLOAT3 dst{};
			XMStoreFloat3(&dst, targetV + XMVectorScale(exitDir, kTowerDoorPortalExitOffset));

			float targetBottomY = 0.0f;
			if (ComputeDoorGroupBottomY(portal, targetRefs, targetBottomY))
			{
				dst.y = targetBottomY +
					((targetBottomY > kTowerDoorPortalUpperHeightThreshold)
						? kTowerDoorPortalUpperExitYOffset
						: kTowerDoorPortalLowerExitYOffset);
			}
			else
			{
				dst.y = playerPos.y;
			}

			player->SetPendingPortalTeleport(
				GameMath::Vec3(dst.x, dst.y, dst.z),
				GameMath::NormalizeYaw(player->GetYaw() + 180.0f));
			portal.cooldownTicks = kTowerDoorPortalCooldownTicks;
			return true;
		};

	for (TowerDoorPortalEntry& portal : m_towerDoorPortals)
	{
		if (portal.cooldownTicks > 0 || !portal.collider)
			continue;

		const bool hitDoorA = DoesDoorGroupIntersect(portal, portal.doorARefs);
		const bool hitDoorB = DoesDoorGroupIntersect(portal, portal.doorBRefs);

		if (hitDoorA && hitDoorB)
			continue;

		if (hitDoorA)
			return QueueBetweenDoorGroups(portal, portal.doorARefs, portal.doorBRefs);

		if (hitDoorB)
			return QueueBetweenDoorGroups(portal, portal.doorBRefs, portal.doorARefs);
	}

	return false;
}

bool Room::TryQueueCastleDoorPortalTeleport(const PlayerRef& player)
{
	if (!CanUseCastleDoorPortal())
		return false;

	if (!player || player->IsDead())
		return false;

	auto* playerCollider = player->GetComponent<CColliderComponent>();
	if (!playerCollider || playerCollider->GetType() != EColliderType::BCapsule)
		return false;

	playerCollider->OnUpdate(0.0f);
	const BoundingCapsule playerCapsule = playerCollider->GetBCapsule();
	const GameMath::Vec3 playerPos = player->GetPosition();

	auto GetWorldBox = [](const CastleDoorPortalEntry& portal, const DoorPortalSubBoxRef& ref) -> const BoundingOrientedBox*
		{
			if (!portal.collider)
				return nullptr;

			const auto& boxes = portal.collider->GetSubOOBBs();
			if (ref.subIndex >= boxes.size())
				return nullptr;

			return &boxes[ref.subIndex];
		};

	auto DoesDoorGroupIntersect = [&](const CastleDoorPortalEntry& portal, const std::vector<DoorPortalSubBoxRef>& refs) -> bool
		{
			for (const DoorPortalSubBoxRef& ref : refs)
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if (box && playerCapsule.Intersects(*box))
					return true;
			}

			return false;
		};

	auto ComputeDoorGroupCenter = [&](const CastleDoorPortalEntry& portal, const std::vector<DoorPortalSubBoxRef>& refs, XMFLOAT3& outCenter) -> bool
		{
			XMVECTOR sum = XMVectorZero();
			int count = 0;

			for (const DoorPortalSubBoxRef& ref : refs)
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if (!box)
					continue;

				sum += XMLoadFloat3(&box->Center);
				++count;
			}

			if (count <= 0)
				return false;

			XMStoreFloat3(&outCenter, XMVectorScale(sum, 1.0f / static_cast<float>(count)));
			return true;
		};

	auto QueueCastleDoorPair = [&](CastleDoorPortalEntry& portal, const CastleDoorPortalPair& pair) -> bool
		{
			XMFLOAT3 sourceCenter{};
			XMFLOAT3 targetCenter{};
			if (!ComputeDoorGroupCenter(portal, pair.sourceRefs, sourceCenter))
				return false;
			if (!ComputeDoorGroupCenter(portal, pair.targetRefs, targetCenter))
				return false;

			const BoundingOrientedBox* sourceBox = pair.sourceRefs.empty() ? nullptr : GetWorldBox(portal, pair.sourceRefs.front());
			const BoundingOrientedBox* targetBox = pair.targetRefs.empty() ? nullptr : GetWorldBox(portal, pair.targetRefs.front());

			XMVECTOR sourceV = XMLoadFloat3(&sourceCenter);
			XMVECTOR targetV = XMLoadFloat3(&targetCenter);
			XMVECTOR sourceNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			XMVECTOR targetNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			const bool hasSourceNormal = sourceBox && ComputeDoorHorizontalNormal(*sourceBox, sourceNormal);
			const bool hasTargetNormal = targetBox && ComputeDoorHorizontalNormal(*targetBox, targetNormal);

			float sideSign = 1.0f;
			if (hasSourceNormal)
			{
				XMVECTOR playerV = XMVectorSet(playerPos.x, playerPos.y, playerPos.z, 0.0f);
				XMVECTOR sourceToPlayer = XMVectorSetY(playerV - sourceV, 0.0f);
				const float sideDot = XMVectorGetX(XMVector3Dot(sourceToPlayer, sourceNormal));
				sideSign = (sideDot >= 0.0f) ? 1.0f : -1.0f;
			}

			XMVECTOR exitDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			if (hasTargetNormal)
			{
				exitDir = XMVectorScale(targetNormal, sideSign);
			}
			else
			{
				exitDir = XMVectorSetY(targetV - sourceV, 0.0f);
				const float lenSq = XMVectorGetX(XMVector3LengthSq(exitDir));
				exitDir = (lenSq > 1.0e-6f) ? XMVector3Normalize(exitDir) : XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			}

			XMFLOAT3 dst{};
			XMStoreFloat3(&dst, targetV + XMVectorScale(exitDir, kCastleDoorPortalExitOffset));
			dst.y = playerPos.y;

			player->SetPendingPortalTeleport(
				GameMath::Vec3(dst.x, dst.y, dst.z),
				YawFromHorizontalDirection(exitDir));
			portal.cooldownTicks = kCastleDoorPortalCooldownTicks;
			MarkPlayerEnteredCastleCenterMegaGrid(player->playerId);
			return true;
		};

	for (CastleDoorPortalEntry& portal : m_castleDoorPortals)
	{
		if (portal.cooldownTicks > 0 || !portal.collider)
			continue;

		for (const CastleDoorPortalPair& pair : portal.pairs)
		{
			if (!DoesDoorGroupIntersect(portal, pair.sourceRefs))
				continue;

			if (QueueCastleDoorPair(portal, pair))
				return true;
		}
	}

	return false;
}

bool Room::TryTeleportPlayerByTowerDoorPortal(const PlayerRef& player)
{
	if (!player || player->IsDead())
		return false;

	auto* playerCollider = player->GetComponent<CColliderComponent>();
	if (!playerCollider || playerCollider->GetType() != EColliderType::BCapsule)
		return false;

	playerCollider->OnUpdate(0.0f);
	const BoundingCapsule playerCapsule = playerCollider->GetBCapsule();
	const GameMath::Vec3 playerPos = player->GetPosition();

	auto GetWorldBox = [](const TowerDoorPortalEntry& portal, const DoorPortalSubBoxRef& ref) -> const BoundingOrientedBox*
		{
			if (!portal.collider)
				return nullptr;

			const auto& boxes = portal.collider->GetSubOOBBs();
			if (ref.subIndex >= boxes.size())
				return nullptr;

			return &boxes[ref.subIndex];
		};

	auto DoesDoorGroupIntersect = [&](const TowerDoorPortalEntry& portal, const std::vector<DoorPortalSubBoxRef>& refs) -> bool
		{
			for (const DoorPortalSubBoxRef& ref : refs)
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if (box && playerCapsule.Intersects(*box))
					return true;
			}

			return false;
		};

	auto ComputeDoorGroupCenter = [&](const TowerDoorPortalEntry& portal, const std::vector<DoorPortalSubBoxRef>& refs, XMFLOAT3& outCenter) -> bool
		{
			XMVECTOR sum = XMVectorZero();
			int count = 0;

			for (const DoorPortalSubBoxRef& ref : refs)
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if (!box)
					continue;

				sum += XMLoadFloat3(&box->Center);
				++count;
			}

			if (count <= 0)
				return false;

			XMStoreFloat3(&outCenter, XMVectorScale(sum, 1.0f / static_cast<float>(count)));
			return true;
		};

	auto ComputeDoorGroupBottomY = [&](const TowerDoorPortalEntry& portal, const std::vector<DoorPortalSubBoxRef>& refs, float& outBottomY) -> bool
		{
			bool found = false;
			float bottomY = FLT_MAX;

			for (const DoorPortalSubBoxRef& ref : refs)
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if (!box)
					continue;

				XMFLOAT3 corners[BoundingOrientedBox::CORNER_COUNT] = {};
				box->GetCorners(corners);
				for (const XMFLOAT3& corner : corners)
				{
					if (!found || corner.y < bottomY)
					{
						bottomY = corner.y;
						found = true;
					}
				}
			}

			if (!found)
				return false;

			outBottomY = bottomY;
			return true;
		};

	auto TeleportBetweenDoorGroups = [&](
		TowerDoorPortalEntry& portal,
		const std::vector<DoorPortalSubBoxRef>& sourceRefs,
		const std::vector<DoorPortalSubBoxRef>& targetRefs) -> bool
		{
			XMFLOAT3 sourceCenter{};
			XMFLOAT3 targetCenter{};
			if (!ComputeDoorGroupCenter(portal, sourceRefs, sourceCenter))
				return false;
			if (!ComputeDoorGroupCenter(portal, targetRefs, targetCenter))
				return false;

			const BoundingOrientedBox* targetBox = targetRefs.empty() ? nullptr : GetWorldBox(portal, targetRefs.front());

			XMVECTOR sourceV = XMLoadFloat3(&sourceCenter);
			XMVECTOR targetV = XMLoadFloat3(&targetCenter);
			XMVECTOR targetNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			const bool hasTargetNormal = targetBox && ComputeDoorHorizontalNormal(*targetBox, targetNormal);

			XMVECTOR exitDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			if (hasTargetNormal)
			{
				// Tower doors should exit toward the playable tower side. The report data uses
				// the opposite horizontal normal for both lower and upper door groups.
				exitDir = XMVectorNegate(targetNormal);
			}
			else
			{
				exitDir = XMVectorSetY(targetV - sourceV, 0.0f);
				const float lenSq = XMVectorGetX(XMVector3LengthSq(exitDir));
				exitDir = (lenSq > 1.0e-6f) ? XMVector3Normalize(exitDir) : XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			}

			XMFLOAT3 dst{};
			XMStoreFloat3(&dst, targetV + XMVectorScale(exitDir, kTowerDoorPortalExitOffset));

			float targetBottomY = 0.0f;
			if (ComputeDoorGroupBottomY(portal, targetRefs, targetBottomY))
			{
				dst.y = targetBottomY +
					((targetBottomY > kTowerDoorPortalUpperHeightThreshold)
						? kTowerDoorPortalUpperExitYOffset
						: kTowerDoorPortalLowerExitYOffset);
			}
			else
			{
				dst.y = playerPos.y;
			}

			player->SetPosition(GameMath::Vec3(dst.x, dst.y, dst.z));
			player->SetYaw(GameMath::NormalizeYaw(player->GetYaw() + 180.0f));
			player->SetVelocity(GameMath::Vec3::Zero());
			player->ClearMoveKeyCodes();
			playerCollider->OnUpdate(0.0f);
			portal.cooldownTicks = kTowerDoorPortalCooldownTicks;
			UpdateDynamicGridState();
			return true;
		};

	for (TowerDoorPortalEntry& portal : m_towerDoorPortals)
	{
		if (portal.cooldownTicks > 0 || !portal.collider)
			continue;

		const bool hitDoorA = DoesDoorGroupIntersect(portal, portal.doorARefs);
		const bool hitDoorB = DoesDoorGroupIntersect(portal, portal.doorBRefs);

		if (hitDoorA && hitDoorB)
			continue;

		if (hitDoorA)
			return TeleportBetweenDoorGroups(portal, portal.doorARefs, portal.doorBRefs);

		if (hitDoorB)
			return TeleportBetweenDoorGroups(portal, portal.doorBRefs, portal.doorARefs);
	}

	return false;
}

bool Room::TryTeleportPlayerByCastleDoorPortal(const PlayerRef& player)
{
	if (!CanUseCastleDoorPortal())
		return false;

	if (!player || player->IsDead())
		return false;

	auto* playerCollider = player->GetComponent<CColliderComponent>();
	if (!playerCollider || playerCollider->GetType() != EColliderType::BCapsule)
		return false;

	playerCollider->OnUpdate(0.0f);
	const BoundingCapsule playerCapsule = playerCollider->GetBCapsule();
	const GameMath::Vec3 playerPos = player->GetPosition();

	auto GetWorldBox = [](const CastleDoorPortalEntry& portal, const DoorPortalSubBoxRef& ref) -> const BoundingOrientedBox*
		{
			if (!portal.collider)
				return nullptr;

			const auto& boxes = portal.collider->GetSubOOBBs();
			if (ref.subIndex >= boxes.size())
				return nullptr;

			return &boxes[ref.subIndex];
		};

	auto DoesDoorGroupIntersect = [&](const CastleDoorPortalEntry& portal, const std::vector<DoorPortalSubBoxRef>& refs) -> bool
		{
			for (const DoorPortalSubBoxRef& ref : refs)
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if (box && playerCapsule.Intersects(*box))
					return true;
			}

			return false;
		};

	auto ComputeDoorGroupCenter = [&](const CastleDoorPortalEntry& portal, const std::vector<DoorPortalSubBoxRef>& refs, XMFLOAT3& outCenter) -> bool
		{
			XMVECTOR sum = XMVectorZero();
			int count = 0;

			for (const DoorPortalSubBoxRef& ref : refs)
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if (!box)
					continue;

				sum += XMLoadFloat3(&box->Center);
				++count;
			}

			if (count <= 0)
				return false;

			XMStoreFloat3(&outCenter, XMVectorScale(sum, 1.0f / static_cast<float>(count)));
			return true;
		};

	auto TeleportCastleDoorPair = [&](CastleDoorPortalEntry& portal, const CastleDoorPortalPair& pair) -> bool
		{
			XMFLOAT3 sourceCenter{};
			XMFLOAT3 targetCenter{};
			if (!ComputeDoorGroupCenter(portal, pair.sourceRefs, sourceCenter))
				return false;
			if (!ComputeDoorGroupCenter(portal, pair.targetRefs, targetCenter))
				return false;

			const BoundingOrientedBox* sourceBox = pair.sourceRefs.empty() ? nullptr : GetWorldBox(portal, pair.sourceRefs.front());
			const BoundingOrientedBox* targetBox = pair.targetRefs.empty() ? nullptr : GetWorldBox(portal, pair.targetRefs.front());

			XMVECTOR sourceV = XMLoadFloat3(&sourceCenter);
			XMVECTOR targetV = XMLoadFloat3(&targetCenter);
			XMVECTOR sourceNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			XMVECTOR targetNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			const bool hasSourceNormal = sourceBox && ComputeDoorHorizontalNormal(*sourceBox, sourceNormal);
			const bool hasTargetNormal = targetBox && ComputeDoorHorizontalNormal(*targetBox, targetNormal);

			float sideSign = 1.0f;
			if (hasSourceNormal)
			{
				XMVECTOR playerV = XMVectorSet(playerPos.x, playerPos.y, playerPos.z, 0.0f);
				XMVECTOR sourceToPlayer = XMVectorSetY(playerV - sourceV, 0.0f);
				const float sideDot = XMVectorGetX(XMVector3Dot(sourceToPlayer, sourceNormal));
				sideSign = (sideDot >= 0.0f) ? 1.0f : -1.0f;
			}

			XMVECTOR exitDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			if (hasTargetNormal)
			{
				exitDir = XMVectorScale(targetNormal, sideSign);
			}
			else
			{
				exitDir = XMVectorSetY(targetV - sourceV, 0.0f);
				const float lenSq = XMVectorGetX(XMVector3LengthSq(exitDir));
				exitDir = (lenSq > 1.0e-6f) ? XMVector3Normalize(exitDir) : XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			}

			XMFLOAT3 dst{};
			XMStoreFloat3(&dst, targetV + XMVectorScale(exitDir, kCastleDoorPortalExitOffset));
			dst.y = playerPos.y;

			player->SetPosition(GameMath::Vec3(dst.x, dst.y, dst.z));
			player->SetYaw(YawFromHorizontalDirection(exitDir));
			player->SetVelocity(GameMath::Vec3::Zero());
			player->ClearMoveKeyCodes();
			playerCollider->OnUpdate(0.0f);
			portal.cooldownTicks = kCastleDoorPortalCooldownTicks;
			UpdateDynamicGridState();
			MarkPlayerEnteredCastleCenterMegaGrid(player->playerId);
			return true;
		};

	for (CastleDoorPortalEntry& portal : m_castleDoorPortals)
	{
		if (portal.cooldownTicks > 0 || !portal.collider)
			continue;

		for (const CastleDoorPortalPair& pair : portal.pairs)
		{
			if (!DoesDoorGroupIntersect(portal, pair.sourceRefs))
				continue;

			if (TeleportCastleDoorPair(portal, pair))
				return true;
		}
	}

	return false;
}
