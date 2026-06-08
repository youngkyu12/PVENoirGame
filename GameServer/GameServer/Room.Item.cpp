#include "pch.h"
#include "Room.h"
#include "Player.h"

#include <random>
#include <unordered_set>

void Room::InitializeItems()
{
	m_items.clear();
	m_framePickedUpItemIds.clear();
	uint64 nextItemId = 1;

	// Key items — fixed positions
	for (int i = 0; i < kKeyCount; ++i)
	{
		const auto& key = kKeyPositions[i];
		const int megaGrid = key.megaGridIndex + 1;
		ItemEntry entry;
		entry.id       = nextItemId++;
		entry.kind     = Protocol::ITEM_TYPE_KEY;
		entry.position = GameMath::Vec3(key.x, key.y, key.z);
		entry.active   = m_keyPickupUnlockedByMegaGrid[static_cast<size_t>(megaGrid)];
		m_items.push_back(entry);
	}

	// Potion items — random placement per megagrid
	constexpr int kTargetMegaGrids[]      = { 1, 2, 3, 4, 6, 7, 8, 9 };
	constexpr int kTargetMegaGridCount    = 8;
	constexpr int kPotionCountPerMegaGrid = 6;
	constexpr int kCenterAreaSize         = 200;

	const Protocol::ItemType potionTypes[] = {
		Protocol::ITEM_TYPE_HEAL_POTION,
		Protocol::ITEM_TYPE_ATTACK_POWER_POTION,
		Protocol::ITEM_TYPE_DEFENSE_POTION,
		Protocol::ITEM_TYPE_MOVE_SPEED_POTION,
	};

	std::mt19937 rng{ 12345u };
	std::unordered_set<int> usedCells;

	for (Protocol::ItemType kind : potionTypes)
	{
		for (int i = 0; i < kTargetMegaGridCount; ++i)
		{
			const int megaGrid     = kTargetMegaGrids[i];
			const int zeroBasedIdx = megaGrid - 1;
			const int megaX        = zeroBasedIdx % kMegaGridCols;
			const int megaZ        = zeroBasedIdx / kMegaGridCols;

			const int megaStartCellX  = megaX * kMegaGridCellWidth;
			const int megaStartCellZ  = megaZ * kMegaGridCellHeight;
			const int centerStartCellX = megaStartCellX + (kMegaGridCellWidth  - kCenterAreaSize) / 2;
			const int centerStartCellZ = megaStartCellZ + (kMegaGridCellHeight - kCenterAreaSize) / 2;
			const int centerEndCellX   = centerStartCellX + kCenterAreaSize;
			const int centerEndCellZ   = centerStartCellZ + kCenterAreaSize;

			std::vector<int> candidates;
			for (int cz = centerStartCellZ; cz < centerEndCellZ; ++cz)
			{
				for (int cx = centerStartCellX; cx < centerEndCellX; ++cx)
				{
					if (cx < 0 || cx >= kGridWidth || cz < 0 || cz >= kGridHeight)
						continue;
					const int cellIdx = GridCellIndex(cx, cz);
					if (m_gridStaticCells[static_cast<size_t>(cellIdx)].buildingCount > 0)
						continue;
					if (usedCells.count(cellIdx))
						continue;
					candidates.push_back(cellIdx);
				}
			}

			std::shuffle(candidates.begin(), candidates.end(), rng);
			const int spawnCount = std::min<int>(kPotionCountPerMegaGrid, static_cast<int>(candidates.size()));

			for (int j = 0; j < spawnCount; ++j)
			{
				const int cellIdx = candidates[static_cast<size_t>(j)];
				const int cx      = cellIdx % kGridWidth;
				const int cz      = cellIdx / kGridWidth;

				usedCells.insert(cellIdx);

				const float worldX = static_cast<float>(kGridMinX + cx) + 0.5f;
				const float worldZ = static_cast<float>(kGridMinZ + cz) + 0.5f;
				const float worldY = GetTerrainGroundHeight(worldX, worldZ);

				ItemEntry entry;
				entry.id       = nextItemId++;
				entry.kind     = kind;
				entry.position = GameMath::Vec3(worldX, worldY, worldZ);
				entry.active   = true;
				m_items.push_back(entry);
			}
		}
	}

	cout << "[Items] initialized: " << m_items.size() << " items" << endl;
}

void Room::UpdateItemPickupCollision()
{
	constexpr float kPickupRadiusSq    = 1.25f * 1.25f;
	constexpr float kPickupYTolerance  = 2.0f;

	for (auto& item : m_items)
	{
		if (!item.active) continue;
		if (item.kind == Protocol::ITEM_TYPE_KEY) continue; // key는 UpdateKeyPickupCollision에서 처리

		for (auto& [pid, player] : players)
		{
			if (!player || player->IsDead()) continue;

			const GameMath::Vec3 pos = player->GetPosition();
			if (pos.y < -100.0f) continue;

			const float dx = pos.x - item.position.x;
			const float dz = pos.z - item.position.z;
			if (dx * dx + dz * dz > kPickupRadiusSq) continue;
			if (std::abs(pos.y - item.position.y) > kPickupYTolerance) continue;

			item.active = false;
			m_framePickedUpItemIds.push_back(item.id);
			player->AddInventoryItem(item.kind);

			cout << "[Item Pickup] Player " << player->GetObjectId()
				<< " picked up item " << item.id
				<< " (kind=" << static_cast<int>(item.kind) << ")" << endl;
			break;
		}
	}
}

void Room::UseItem(uint64 playerId, int32 slot)
{
	auto it = players.find(playerId);
	if (it == players.end() || !it->second) return;
	PlayerRef& player = it->second;
	if (player->IsDead()) return;

	if (slot < 0 || slot >= Player::kInventorySlotCount) return;
	const Protocol::ItemType kind = static_cast<Protocol::ItemType>(slot + 1);
	player->UseInventoryItem(kind, m_elapsedServerMs);
}
