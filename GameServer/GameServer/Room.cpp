#include "pch.h"
#include "Room.h"
#include "Player.h"
#include "Enemy.h"
#include "Building.h"
#include "GameSession.h"
#include "GameArea.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"

#include "CommonPlayerControllerComponent.h"

#include "Protocol.pb.h"
#include "ClientPacketHandler.h"

#include <algorithm>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <unordered_map>

shared_ptr<Room> GRoom = make_shared<Room>();


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

	static Protocol::BuildingType AssetToBuildingType(const std::string& asset);

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
			// 기본 빌딩 바운드(-1.5~1.5, 0~3.5)를 중심 기준으로 약 3배 확장
			// X/Z: extents 1.5 -> 4.5, Y: center 1.75 기준 extents 1.75 -> 5.25
			outMin = XMFLOAT3(-4.5f, -3.5f, -4.5f);
			outMax = XMFLOAT3(4.5f, 7.0f, 4.5f);
			return;
		case Protocol::BUILDING_TYPE_TOWER:
			// 타워는 높이가 더 높고, 폭이 더 좁음
			outMin = XMFLOAT3(-1.0f, 0.0f, -1.0f);
			outMax = XMFLOAT3(1.0f, 6.0f, 1.0f);
			return;
		default:
			outMin = XMFLOAT3(-1.5f, 0.0f, -1.5f);
			outMax = XMFLOAT3(1.5f, 3.5f, 1.5f);
			return;
		}
	}

	// [die][hit][run][roll][attack][up][down][left][right][move]
	// LSB부터 move로 배치 (전송/해석 시 동일 규약만 지키면 됨)
	enum : uint32
	{
		kStateMove = 1u << 0,
		kStateRight = 1u << 1,
		kStateLeft = 1u << 2,
		kStateDown = 1u << 3, // backward
		kStateUp = 1u << 4, // forward
		kStateAttack = 1u << 5,
		kStateRoll = 1u << 6,
		kStateRun = 1u << 7,
		kStateHit = 1u << 8, // 현재 서버 소스에 Hit 상태 근거가 없으면 0 유지
		kStateDie = 1u << 9
	};

	static uint32 BuildStateCode(const CServerObject& obj)
	{
		uint32 code = 0;
		const auto anim = obj.GetAnimState();

		// 상위 액션 비트
		if (anim == Protocol::ANIMATION_TYPE_DIE)    code |= kStateDie;
		if (anim == Protocol::ANIMATION_TYPE_ATTACK) code |= kStateAttack;
		if (anim == Protocol::ANIMATION_TYPE_ROLL)   code |= kStateRoll;
		if (anim == Protocol::ANIMATION_TYPE_RUN)    code |= kStateRun;

		// TODO: Hit 상태 소스 추가 시 반영
		// if (anim == Protocol::ANIMATION_TYPE_HIT) code |= kStateHit;

		// 이동/방향 비트
		const GameMath::Vec3 v = obj.GetVelocity();
		constexpr float kEps = 1e-4f;

		if (v.LengthSq() > kEps)
		{
			code |= kStateMove;

			const float fwd = GameMath::Vec3::Dot(v, obj.GetLook());   // +forward, -backward
			const float str = GameMath::Vec3::Dot(v, obj.GetRight());  // +right,   -left

			if (fwd > kEps) code |= kStateUp;
			if (fwd < -kEps) code |= kStateDown;
			if (str > kEps) code |= kStateRight;
			if (str < -kEps) code |= kStateLeft;
		}

		return code;
	}

	struct PlacementEntry
	{
		std::string asset;
		std::string objectName;
		GameMath::Vec3 position = GameMath::Vec3::Zero();
		float yawDeg = 0.0f;
	};

	struct ReportObjectOOBB
	{
		int placementIndex = -1;
		std::string assetName;
		std::string objectName;
		BoundingOrientedBox localOOBB{};
      std::vector<BoundingOrientedBox> localSubOOBBs;
	};

  static std::unordered_map<int, ReportObjectOOBB> g_reportByBuildingType;
	static bool g_staticWorldReportLoaded = false;

	static float QuaternionToYawDegrees(float x, float y, float z, float w)
	{
		const float siny_cosp = 2.0f * (w * y + x * z);
		const float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
		return atan2f(siny_cosp, cosy_cosp) * GameMath::RAD_TO_DEG;
	}

	static bool ParsePlacementEntryLine(const std::string& line, PlacementEntry& out)
	{
		char asset[64] = {};
		char objectName[128] = {};
		float px = 0.0f, py = 0.0f, pz = 0.0f;
		float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f;

		const int matched = ::sscanf_s(
			line.c_str(),
			"ENTRY|asset=\"%63[^\"]\"|object=\"%127[^\"]\"|pos=(%f,%f,%f)|rot=(%f,%f,%f,%f)",
			asset, static_cast<unsigned>(_countof(asset)),
			objectName, static_cast<unsigned>(_countof(objectName)),
			&px, &py, &pz,
			&qx, &qy, &qz, &qw
		);

		if (matched != 9)
			return false;

		out.asset = asset;
		out.objectName = objectName;
		out.position = GameMath::Vec3(px, py, pz);
		out.yawDeg = QuaternionToYawDegrees(qx, qy, qz, qw);
		return true;
	}

	static bool LoadPlacementEntries(std::vector<PlacementEntry>& outEntries)
	{
		outEntries.clear();

		const std::vector<std::string> candidates = {
			"MapFIle/placement_export_full.txt",
			"GameServer/MapFIle/placement_export_full.txt",
			"../GameServer/MapFIle/placement_export_full.txt"
		};

		std::ifstream fin;
		for (const auto& path : candidates)
		{
			fin.open(path);
			if (fin.is_open())
				break;
			fin.clear();
		}

		if (!fin.is_open())
			return false;

		std::string line;
		while (std::getline(fin, line))
		{
			PlacementEntry entry;
			if (!ParsePlacementEntryLine(line, entry))
				continue;
			outEntries.push_back(std::move(entry));
		}

		return !outEntries.empty();
	}

	static std::string TrimString(const std::string& text)
	{
		size_t begin = 0;
		while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])))
			++begin;

		size_t end = text.size();
		while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
			--end;

		return text.substr(begin, end - begin);
	}

	static bool TrySplitKeyValue(const std::string& line, std::string& outKey, std::string& outValue)
	{
		const size_t sep = line.find(':');
		if (sep == std::string::npos)
			return false;

		outKey = TrimString(line.substr(0, sep));
		outValue = TrimString(line.substr(sep + 1));
		return !outKey.empty();
	}

	static bool ParseVector3Tuple(const std::string& text, XMFLOAT3& outValue)
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		if (::sscanf_s(text.c_str(), "(%f, %f, %f)", &x, &y, &z) != 3)
			return false;

		outValue = XMFLOAT3(x, y, z);
		return true;
	}

	static bool ParseVector4Tuple(const std::string& text, XMFLOAT4& outValue)
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 1.0f;
		if (::sscanf_s(text.c_str(), "(%f, %f, %f, %f)", &x, &y, &z, &w) != 4)
			return false;

		outValue = XMFLOAT4(x, y, z, w);
		return true;
	}

  static bool LoadStaticWorldOverallLocalOOBBReport(
		std::unordered_map<int, ReportObjectOOBB>& outByBuildingType)
	{
        outByBuildingType.clear();

		const std::vector<std::string> candidates = {
			"MapFIle/StaticWorldLocalOOBBReport.txt",
			"GameServer/MapFIle/StaticWorldLocalOOBBReport.txt",
			"../GameServer/MapFIle/StaticWorldLocalOOBBReport.txt"
		};

		std::ifstream fin;
		for (const auto& path : candidates)
		{
			fin.open(path);
			if (fin.is_open())
				break;
			fin.clear();
		}

		if (!fin.is_open())
			return false;

		std::string line;
		bool inObject = false;
		ReportObjectOOBB current{};
		bool hasCenter = false;
		bool hasRotation = false;
		bool hasExtents = false;
		bool inSubOOBB = false;
		BoundingOrientedBox currentSubOOBB{};
		bool hasSubCenter = false;
		bool hasSubRotation = false;
		bool hasSubExtents = false;

		while (std::getline(fin, line))
		{
			line = TrimString(line);
			if (line.empty())
				continue;

			if (line == "ObjectBegin")
			{
				inObject = true;
				current = ReportObjectOOBB{};
				hasCenter = false;
				hasRotation = false;
				hasExtents = false;
               inSubOOBB = false;
				hasSubCenter = false;
				hasSubRotation = false;
				hasSubExtents = false;
				continue;
			}

			if (!inObject)
				continue;

			if (line == "ObjectEnd")
			{
              if (inSubOOBB && hasSubCenter && hasSubRotation && hasSubExtents)
					current.localSubOOBBs.push_back(currentSubOOBB);

				if (current.placementIndex >= 0 && hasCenter && hasRotation && hasExtents)
				{
                  const Protocol::BuildingType buildingType = AssetToBuildingType(current.assetName);
					if (buildingType != Protocol::BUILDING_TYPE_NONE)
					{
						const int buildingTypeKey = static_cast<int>(buildingType);
						if (outByBuildingType.find(buildingTypeKey) == outByBuildingType.end())
							outByBuildingType[buildingTypeKey] = current;
					}
				}

				inObject = false;
               inSubOOBB = false;
				continue;
			}

			if (line == "SubOOBBBegin")
			{
				inSubOOBB = true;
				currentSubOOBB = BoundingOrientedBox{};
				hasSubCenter = false;
				hasSubRotation = false;
				hasSubExtents = false;
				continue;
			}

			if (line == "SubOOBBEnd")
			{
				if (inSubOOBB && hasSubCenter && hasSubRotation && hasSubExtents)
					current.localSubOOBBs.push_back(currentSubOOBB);

				inSubOOBB = false;
				continue;
			}

			std::string key;
			std::string value;
			if (!TrySplitKeyValue(line, key, value))
				continue;

			if (key == "PlacementIndex")
			{
				current.placementIndex = std::atoi(value.c_str());
			}
			else if (key == "AssetName")
			{
				current.assetName = value;
			}
			else if (key == "ObjectName")
			{
				current.objectName = value;
			}
			else if (key == "OverallLocalOOBB.Center")
			{
				hasCenter = ParseVector3Tuple(value, current.localOOBB.Center);
			}
			else if (key == "OverallLocalOOBB.RotationQuat")
			{
				hasRotation = ParseVector4Tuple(value, current.localOOBB.Orientation);
			}
			else if (key == "OverallLocalOOBB.Extents")
			{
				hasExtents = ParseVector3Tuple(value, current.localOOBB.Extents);
			}
           else if (key == "SubLocalOOBB.Center")
			{
				hasSubCenter = ParseVector3Tuple(value, currentSubOOBB.Center);
			}
			else if (key == "SubLocalOOBB.RotationQuat")
			{
				hasSubRotation = ParseVector4Tuple(value, currentSubOOBB.Orientation);
			}
			else if (key == "SubLocalOOBB.Extents")
			{
				hasSubExtents = ParseVector3Tuple(value, currentSubOOBB.Extents);
			}
		}

        return !outByBuildingType.empty();
	}

	static Protocol::BuildingType AssetToBuildingType(const std::string& asset)
	{
		if (asset == "Grass") return Protocol::BUILDING_TYPE_GRASS;
		if (asset == "Ground") return Protocol::BUILDING_TYPE_GROUND;
		if (asset == "Building1") return Protocol::BUILDING_TYPE_BUILDING1;
		if (asset == "Building2") return Protocol::BUILDING_TYPE_BUILDING2;
		if (asset == "Building3") return Protocol::BUILDING_TYPE_BUILDING3;
		if (asset == "Building4") return Protocol::BUILDING_TYPE_BUILDING4;
		if (asset == "Building5") return Protocol::BUILDING_TYPE_BUILDING5;
		if (asset == "Building6") return Protocol::BUILDING_TYPE_BUILDING6;
		if (asset == "Building7") return Protocol::BUILDING_TYPE_BUILDING7;
		if (asset == "Building8") return Protocol::BUILDING_TYPE_BUILDING8;
		if (asset == "Building9") return Protocol::BUILDING_TYPE_BUILDING9;
		if (asset == "VillageWall") return Protocol::BUILDING_TYPE_VILLAGE_WALL;
		if (asset == "DirtRoad") return Protocol::BUILDING_TYPE_DIRT_ROAD;
		if (asset == "Tower") return Protocol::BUILDING_TYPE_TOWER;
		return Protocol::BUILDING_TYPE_NONE;
	}

	static const char* BuildingTypeToAssetName(Protocol::BuildingType type)
	{
		switch (type)
		{
		case Protocol::BUILDING_TYPE_GRASS: return "Grass";
		case Protocol::BUILDING_TYPE_GROUND: return "Ground";
		case Protocol::BUILDING_TYPE_BUILDING1: return "Building1";
		case Protocol::BUILDING_TYPE_BUILDING2: return "Building2";
		case Protocol::BUILDING_TYPE_BUILDING3: return "Building3";
		case Protocol::BUILDING_TYPE_BUILDING4: return "Building4";
		case Protocol::BUILDING_TYPE_BUILDING5: return "Building5";
		case Protocol::BUILDING_TYPE_BUILDING6: return "Building6";
		case Protocol::BUILDING_TYPE_BUILDING7: return "Building7";
		case Protocol::BUILDING_TYPE_BUILDING8: return "Building8";
		case Protocol::BUILDING_TYPE_BUILDING9: return "Building9";
		case Protocol::BUILDING_TYPE_VILLAGE_WALL: return "VillageWall";
		case Protocol::BUILDING_TYPE_DIRT_ROAD: return "DirtRoad";
		case Protocol::BUILDING_TYPE_TOWER: return "Tower";
		default: return "";
		}
	}

	static void EnsureStaticWorldReportLoaded()
	{
		if (g_staticWorldReportLoaded)
			return;

     LoadStaticWorldOverallLocalOOBBReport(g_reportByBuildingType);
		g_staticWorldReportLoaded = true;
	}

	static GameMath::Vec3 GetInitialPlayerSpawnPosition(uint64 playerId)
	{
		// VillageWall OOBB(z: -0.5 ~ 0.5), Player 반경 z 약 0.4
		// z=1.2면 비충돌 시작 + 조금만 이동하면 충돌 테스트 가능
		constexpr float kBaseZ = 1.2f;
		constexpr float kSpacingX = 4.0f;
		return GameMath::Vec3(200 + static_cast<float>(playerId) * kSpacingX, 0.0f, kBaseZ);
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

           const int buildingTypeKey = static_cast<int>(building->GetBuildingType());
			auto reportIt = g_reportByBuildingType.find(buildingTypeKey);

			if (reportIt != g_reportByBuildingType.end())
			{
				collider->SetOOBB(reportIt->second.localOOBB);
               collider->SetSubOOBBs(reportIt->second.localSubOOBBs);
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



void Room::Enter(PlayerRef player)
{
	player->Build();
	player->SetPosition(GetInitialPlayerSpawnPosition(player->playerId)); // 초기 비충돌 스폰

	player->SetWeapon(
		static_cast<Protocol::WeaponType>(player->playerId + 2), 0); // 예시: 모든 플레이어가 검으로 시작

	players[player->playerId] = player;
	player->SetActive(false); // 초기에는 비활성화 상태로 시작 (Ready 신호 대기)

	RegisterDynamicCollider(player);
}

void Room::Leave(PlayerRef player)
{
	if (_collision && player)
	{
		if (auto* collider = player->GetComponent<CColliderComponent>())
			_collision->UnregisterCollider(collider);
	}

	players.erase(player->playerId);
}

void Room::BroadCastAll(SendBufferRef sendBuffer)
{
	//for(auto& area : gameAreas)
	//{
	//	area->BroadCast(sendBuffer);
	//}

	for (auto& p : players)
	{
		p.second->ownerSession->Send(sendBuffer);
	}
}



void Room::BuildRoom()
{
	buildings.clear();
	InitializeCollisionSystem();

	std::vector<PlacementEntry> entries;

	if (LoadPlacementEntries(entries))
	{
		uint64 buildingId = 1;
       for (size_t entryIndex = 0; entryIndex < entries.size(); ++entryIndex)
		{
          const auto& e = entries[entryIndex];
			auto building = std::make_shared<CBuilding>();
			building->SetObjectId(buildingId);
			building->SetPosition(e.position);
			building->SetYaw(GameMath::NormalizeYaw(e.yawDeg));
			building->SetBuildingType(AssetToBuildingType(e.asset));
			building->SetActive(true);

			RegisterStaticCollider(building);

			buildings[buildingId] = building;
			++buildingId;
		}
	}

	MakeFireRateMap();
	for (int i = 0; i < 10; ++i)
	{
		auto enemy = make_shared<CEnemy>(i, u8"Zombie", Protocol::ENEMY_TYPE_BASIC, nullptr);
		enemy->Build(GameMath::Vec3(i * 10.0f, 0, 0), GameMath::Vec3(0, 0, 0));
		RegisterDynamicCollider(enemy);
		//enemies[i]->AddComponent<CTransformComponent>();
		//enemies[i]->CreateComponents();
		enemies[i] = enemy;
	}

	for (int i = 0; i < 30; ++i)
	{
		auto enemy = make_shared<CEnemy>(i + 10, u8"FIghter", Protocol::ENEMY_TYPE_ARCHER, nullptr);
		enemy->Build(GameMath::Vec3((i + 10) * 10.0f, 0, 10), GameMath::Vec3(0, 0, 0));
		RegisterDynamicCollider(enemy);
		//enemies[i]->AddComponent<CTransformComponent>();
		//enemies[i]->CreateComponents();
		enemies[i] = enemy;
	}

	for (auto& playerPair : players)
		RegisterDynamicCollider(playerPair.second);
}

void Room::StartGame(bool ready, uint32 index)
{
	// 모든 플레이어가 Ready를 보냈음을 확인하면 시작
	if(players.find(index) == players.end())
		return;

	WRITE_LOCK;
	static Vector<bool> p_ready(4);
	p_ready[index] = ready;


	players[index]->SetActive(ready);
	
	static Atomic<bool> gameStarted = false;

	 //if(p_ready[0] && p_ready[1] && p_ready[2] && p_ready[3])1
	//if(p_ready[0] && p_ready[1])
	//if(p_ready[0])
	if(
		std::all_of(players.begin(), players.end(), 
		[&](const auto& player)
		{
			return player.second && player.second->IsActive();
		})
		&& 
		players.size() == MaxPlayers)
	{
		if (gameStarted.exchange(true) == false)
		{
			GRoom->DoAsync(&Room::MakeInitStruct, Protocol::S_GAME_START());
		}
	}
	else
	{
		// 아직 모든 플레이어가 Ready를 보내지 않았다면 S_ENTER_GAME 패킷을 보내서 Ready를 요청한다.
		Protocol::S_ENTER_GAME enterGamePkt;
		GRoom->DoAsync(&Room::MakeEnterGameStruct, enterGamePkt);

	}
	
}

void Room::EndGame()
{
}

void Room::TickAdvance()
{
	MakeFrameState(tick.load());

	for (auto player : players)
	{
		const GameMath::Vec3 prevPos = player.second->GetPosition();
		player.second->Update(tick); // dt는 30ms로 고정 (옵션)
		ResolveWorldStaticCollision(player.second, prevPos);
		// 위치 적용
	}

	for(auto enemy : enemies)
	{
		const GameMath::Vec3 prevPos = enemy.second->GetPosition();
		enemy.second->Update(tick);
		ResolveWorldStaticCollision(enemy.second, prevPos);
	}

	if (_collision)
		_collision->OnUpdate();


	tick++;
}


// TODO: ProcessInput을 TickAdvance에서 처리할 수 있게 바꿔야 함

void Room::ProcessInput(uint64 playerId, int32 keyCodes, float deltaX, float deltaY)
{
	// 플레이어 찾기
	auto it = players.find(playerId);
	if (it == players.end())
		return;

	//std::cout << "ProcessInput: playerId=" << playerId << 
	//	", keyCodes=" << keyCodes << 
	//	", deltaX=" << deltaX << 
	//	", deltaY=" << deltaY << 
	//	std::endl;

	PlayerRef& player = it->second;

	// ========== 1. 회전 처리 (클라이언트 Rotate 함수와 동일) ==========
	if (deltaX != 0.0f)
	{
		float currentYaw = player->GetYaw();
		player->SetYaw(GameMath::NormalizeYaw(currentYaw + deltaX));
	}

	// ========== 2. 이동 처리 (클라이언트 Move 함수와 동일) ==========
	constexpr int kDirForward	= 1 << 0;
	constexpr int kDirBackward	= 1 << 1;
	constexpr int kDirLeft		= 1 << 2;
	constexpr int kDirRight		= 1 << 3;
	constexpr int kDirRButton	= 1 << 6; // 옵션
	constexpr int kDirLButton	= 1 << 7; // 옵션
	constexpr int kDirRun		= 1 << 8; // 옵션
	constexpr int kDirRoll		= 1 << 9; // 옵션


	Protocol::AnimationType prevAnimState = player->GetAnimState();

	//const int isRolling = player->GetAnimState() == Protocol::ANIMATION_TYPE_ROLL;

	//int notPassive = kDirRoll & (keyCodes & (kDirForward | kDirBackward | kDirLeft | kDirRight))
	//	^ isRolling;

	//notPassive ^= (player->GetAnimState() == Protocol::ANIMATION_TYPE_ATTACK) &
	//	(player->GetWeaponState() % 2 == 0);




	if ((keyCodes & kDirLButton) != 0)
		player->SetAnimState(Protocol::ANIMATION_TYPE_ATTACK);
	else if (prevAnimState != Protocol::ANIMATION_TYPE_ATTACK &&
		prevAnimState != Protocol::ANIMATION_TYPE_ROLL)
	{
		player->SetAnimState(keyCodes & (kDirForward | kDirBackward | kDirLeft | kDirRight) ?
			(keyCodes & kDirRun ? Protocol::ANIMATION_TYPE_RUN : Protocol::ANIMATION_TYPE_WALK) :
			Protocol::ANIMATION_TYPE_IDLE);
	}

	player->SetAnimState(keyCodes & (kDirForward | kDirBackward | kDirLeft | kDirRight) && 
		keyCodes & kDirRoll
		&& (prevAnimState != Protocol::ANIMATION_TYPE_ROLL)
		? Protocol::ANIMATION_TYPE_ROLL : player->GetAnimState());

	if (player->GetAnimState() != prevAnimState)
		player->SetAnimTick(tick); // 애니메이션 상태가 바뀌면 현재의 server tick을 넣어줌


	//if (keyCodes & (kDirForward | kDirBackward))
	//	player->SetAnimState(Protocol::ANIMATION_TYPE_WALK);

	// Look/Right 벡터 (GameMath 사용)
	GameMath::Vec3 look  = player->GetLook();
	GameMath::Vec3 right = player->GetRight();

	// 이동 거리 계산
	const float speed = 5.0f;
	const float dt = 0.03f;
	float fDistance = speed * dt;

	// 방향별 shift 누적 (클라이언트 Move 함수와 동일한 로직)
	GameMath::Vec3 shift = GameMath::Vec3::Zero();

	if (keyCodes & kDirForward)
		shift += look * fDistance;

	if (keyCodes & kDirBackward)
		shift += look * (-fDistance);

	if (keyCodes & kDirRight)
		shift += right * fDistance * 0.5f;

	if (keyCodes & kDirLeft)
		shift += right * (-fDistance) * 0.5f;

	const float moveMul = (player->GetAnimState() == Protocol::ANIMATION_TYPE_RUN) ? 2.0f : 1.0f;
	GameMath::Vec3 desiredShift = shift * moveMul;

	if (GameMath::Vec3::Dot(desiredShift, desiredShift) > 1e-8f)
	{
		std::cout << "desiredShift before collision: " << desiredShift.x << ", " << desiredShift.y << ", " << desiredShift.z << std::endl;
		desiredShift = ResolvePreBlockedShift(player, desiredShift);
	}

	shift = desiredShift / moveMul;

	player->SetVelocity(shift);

	std::cout << "shift: " << shift.x << ", " << shift.y << ", " << shift.z << std::endl;

	// 위치 적용
	//player->Move(shift);
}



void Room::MakeFrameState(uint32 tick)
{
	// 게임 로직 업데이트 (예: 적 이동, 충돌 검사 등)
	Protocol::S_FRAME_STATE frameStatePkt;
	frameStatePkt.set_servertick(tick);

	// 프레임 상태 패킷 작성 (예: 플레이어 위치, 적 상태 등)

	for (auto playerMap : players)
	{
		PlayerRef& player = playerMap.second;

		auto p = frameStatePkt.add_players();
		p->set_id(player->playerId);
		p->set_name(player->name);
		p->set_playertype(player->type);

		Protocol::Animation* anim = p->mutable_animation();
		anim->set_statecode(BuildStateCode(*player));
		anim->set_animationtick(player->GetAnimTick());

		p->set_weapontype(player->GetWeaponState());

		
		Protocol::Transform* transform = p->mutable_transform();
		Protocol::Vec3f* position = transform->mutable_position();
		position->set_x(player->GetPosition().x);
		position->set_y(player->GetPosition().y);
		position->set_z(player->GetPosition().z);
		transform->set_yaw(player->GetYaw());
	}


	for (auto enemyMap : enemies)
	{
		EnemyRef& enemy = enemyMap.second;
		auto e = frameStatePkt.add_enemies();
		e->set_id(enemyMap.first);
		e->set_enemytype(enemy->type);
		e->set_weapontype(enemy->GetWeaponState());

		Protocol::Animation* anim = e->mutable_animation();
		anim->set_statecode(BuildStateCode(*enemy));
		anim->set_animationtick(enemy->GetAnimTick());

		Protocol::Transform* transform = e->mutable_transform();
		Protocol::Vec3f* position = transform->mutable_position();
		position->set_x(enemy->GetPosition().x);
		position->set_y(enemy->GetPosition().y);
		position->set_z(enemy->GetPosition().z);
	}

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(frameStatePkt);
	GRoom->DoAsync(&Room::BroadCastAll, sendBuffer);

	// 다음 업데이트 예약
	GRoom->DoTimer(30, &Room::TickAdvance);
}

void Room::MakeInitStruct(Protocol::S_GAME_START gameStartPkt)
{
	// 최초로 들어온 스레드가 패킷 작성 후 전송
	Protocol::InitStruct* initStruct = gameStartPkt.mutable_initstruct();
	for (auto playerMap : players)
	{
		PlayerRef& player = playerMap.second;

		auto p = initStruct->add_players();
		p->set_id(player->playerId);
		p->set_name(player->name);
		p->set_playertype(player->type);

		Protocol::Transform* transform = p->mutable_transform();
		Protocol::Vec3f* position = transform->mutable_position();
		position->set_x(player->GetPosition().x);
		position->set_y(player->GetPosition().y);
		position->set_z(player->GetPosition().z);

		transform->set_yaw(player->GetYaw());
	}


	for (auto enemyMap : enemies)
	{
		EnemyRef& enemy = enemyMap.second;
		auto e = initStruct->add_enemies();
		e->set_id(enemyMap.first);
		e->set_enemytype(enemy->type);


		Protocol::Transform* transform = e->mutable_transform();
		Protocol::Vec3f* position = transform->mutable_position();
		position->set_x(enemy->GetPosition().x);
		position->set_y(enemy->GetPosition().y);
		position->set_z(enemy->GetPosition().z);
	}

	for (auto buildingMap : buildings)
	{
		BuildingRef& building = buildingMap.second;
		auto b = initStruct->add_buildings();
		b->set_id(buildingMap.first);
		b->set_buildingtype(building->GetBuildingType());

		Protocol::Transform* transform = b->mutable_transform();
		Protocol::Vec3f* position = transform->mutable_position();
		position->set_x(building->GetPosition().x);
		position->set_y(building->GetPosition().y);
		position->set_z(building->GetPosition().z);
		transform->set_yaw(building->GetYaw());
	}

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(gameStartPkt);
	GRoom->DoAsync(&Room::BroadCastAll, sendBuffer);

	// 최초 시작 전, 모든 플레이어의 active를 비활성화
	for (auto& player : players)
	{
		player.second->SetActive(false);
	}
	CheckClientReady();
}

void Room::MakeEnterGameStruct(Protocol::S_ENTER_GAME enterGamePkt)
{
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(enterGamePkt);
	BroadCastAll(sendBuffer);
}

void Room::CheckClientReady()
{
	//TODO: 모든 플레이어가 ready를 보냈는지 확인하는 함수 정의

	bool allPlayerBuilt = !players.empty();
	for (auto& player : players)
	{
		allPlayerBuilt = allPlayerBuilt && player.second->IsActive();
	}


	if (allPlayerBuilt)
	{
		cout << "Game Started!" << endl;

		// 게임 시작 로직 (예: 타이머 시작, 적 스폰 등)
		GRoom->DoTimer(100, &Room::TickAdvance);
	}
	else
	{
		// 아직 준비가 안 끝났다면 체크를 다른 쓰레드에게 떠넘긴다
		GRoom->DoTimer(100, &Room::CheckClientReady);
	}

}

void Room::SetPlayerReady(bool ready, uint32& playerId)
{
	players[playerId]->SetActive(true);
}

GameAreaRef Room::GetArea(uint32 areaId)
{
	return GameAreaRef();
}

void Room::TransferPlayer(PlayerRef player, uint32 fromAreaId, uint32 toAreaId)
{
	//gameAreas[fromAreaId]->Leave(player);
	//gameAreas[toAreaId]->Enter(player);
}
