//-----------------------------------------------------------------------------
// File: GameScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScene.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <cstdio>
#include <cctype>
#include <unordered_map>
#include <algorithm>
#include <random>

#include "AnimatorComponent.h"
#include "AnimatorData.h"
#include "Animator.h"
#include "AnimController.h"
#include "MonsterAnimController.h"
#include "MonsterAnimTypes.h"
#include "Material.h"
#include "AssetManager.h"
#include "Texture.h"
#include "LightComponent.h"
#include "PlayerControllerComponent.h"
#include "Object.h"
#include "Mesh.h"
#include "SkinningComponent.h"
#include "ActorTagComponent.h"
#include "ArrowComponent.h"
#include "BulletComponent.h"
#include "Camera.h"
#include "FollowBoneComponent.h"
#include "PlayerEquipmentComponent.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"
#include "WeaponHitboxComponent.h"
#include "MonsterWeaponHitboxComponent.h"
#include "MonsterCombatComponent.h"
#include "NavMesh.h"
#include "MonsterAIComponent.h"
#include "GhoulAIComponent.h"
#include "HealthComponent.h"
#include "AttackPowerComponent.h"
#include "AudioManager.h"
#include "MusicDirector.h"

#include "ThreadManager.h"
#include "Service.h"
#include "Session.h"
#include "BufferReader.h"
#include "ServerPacketHandler.h"

#include "GlobalValues.h"
#include "GameSceneContentCatalog.h"
#include "GameSceneObjectFactory.h"
#include "GameSceneAttachmentBinder.h"

namespace
{
    // [die][hit][run][roll][attack][up][down][left][right][move]
    // move를 bit0(LSB)로 가정
    enum : uint32_t
    {
        kStateMoveBit = 1u << 0,
        kStateRightBit = 1u << 1,
        kStateLeftBit = 1u << 2,
        kStateDownBit = 1u << 3,
        kStateUpBit = 1u << 4,
        kStateAttackBit = 1u << 5,
        kStateRollBit = 1u << 6,
        kStateRunBit = 1u << 7,
        kStateHitBit = 1u << 8,
        kStateDieBit = 1u << 9,
    };

	enum : uint32_t
	{
		kCollisionLayerPlayer = 0, // 플레이어 본체
		kCollisionLayerMonster = 1, // 몬스터 본체
		kCollisionLayerWorldStatic = 2, // 월드 정적 오브젝트
		kCollisionLayerPlayerWeapon = 3, // 플레이어 무기
		kCollisionLayerMonsterWeapon = 4  // 몬스터 무기
	};

	static constexpr uint32_t CollisionBit(uint32_t layer)
	{
		return ( 1u << layer );
	}

	enum : uint32_t
	{
		kNetworkEnemyTypeNone = 0,
		kNetworkEnemyTypeBasic = 1,
		kNetworkEnemyTypeArcher = 2,
		kNetworkEnemyTypeWarrior = 3,
		kNetworkEnemyTypeBoss = 4,
		kNetworkEnemyTypeMutant = 5
	};

	static void ConfigureProjectileCollider(CColliderComponent* collider, bool firedByPlayer)
	{
		if ( !collider ) return;

		if ( firedByPlayer )
		{
			collider->SetLayer(kCollisionLayerPlayerWeapon);
			collider->SetMask(CollisionBit(kCollisionLayerMonster));
		}
		else
		{
			collider->SetLayer(kCollisionLayerMonsterWeapon);
			collider->SetMask(CollisionBit(kCollisionLayerPlayer));
		}
	}

    struct DecodedAnimStateCode
    {
        bool hasMove = false;
        bool run = false;
        uint32_t moveDirBits = 0;

        bool die = false;
        bool hit = false;
        bool roll = false;
        bool attack = false;
    };

    static DecodedAnimStateCode DecodeStateCode(uint32_t stateCode)
    {
        DecodedAnimStateCode out{};

        const bool moveBit = (stateCode & kStateMoveBit) != 0;

        if (moveBit)
        {
            if (stateCode & kStateUpBit) out.moveDirBits |= DIR_FORWARD;
            if (stateCode & kStateDownBit) out.moveDirBits |= DIR_BACKWARD;
            if (stateCode & kStateLeftBit) out.moveDirBits |= DIR_LEFT;
            if (stateCode & kStateRightBit) out.moveDirBits |= DIR_RIGHT;

            // 규칙:
            // isMove=1이어도 방향 4비트가 모두 0이면 Move/Run 아님
            out.hasMove = (out.moveDirBits != 0);
            out.run = out.hasMove && ((stateCode & kStateRunBit) != 0);
        }

        // 우선순위: die > hit > roll > attack
        out.die = (stateCode & kStateDieBit) != 0;
        out.hit = !out.die && ((stateCode & kStateHitBit) != 0);
        out.roll = !out.die && !out.hit && ((stateCode & kStateRollBit) != 0);
        out.attack = !out.die && !out.hit && !out.roll && ((stateCode & kStateAttackBit) != 0);

        return out;
    }

	static constexpr UINT kDebugSubmeshOOBBCapacity = 8192;
	static constexpr bool kEnableStaticWorldLocalOOBBReportExport = false;
	static constexpr const char* kStaticWorldLocalOOBBReportPath = "MapData/StaticWorldLocalOOBBReport.txt";
	static constexpr bool kEnableCastleVillageWallColliderBuildLog = false;

	static XMFLOAT4X4 BuildWorldMatrixFromOOBB(const BoundingOrientedBox& box)
	{
		XMFLOAT4X4 out{};

		const XMMATRIX S = XMMatrixScaling(
			box.Extents.x * 2.0f,
			box.Extents.y * 2.0f,
			box.Extents.z * 2.0f
		);

		const XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&box.Orientation));

		const XMMATRIX T = XMMatrixTranslation(
			box.Center.x,
			box.Center.y,
			box.Center.z
		);

		XMStoreFloat4x4(&out, S * R * T);
		return out;
	}

	static XMFLOAT4X4 BuildIdentityMatrix4x4()
	{
		XMFLOAT4X4 out{};
		XMStoreFloat4x4(&out, XMMatrixIdentity());
		return out;
	}

	static bool IsTreeCullBlockerAssetName(const std::string& assetName)
	{
		return
			( assetName == "VillageWall" ) ||
			( assetName == "Castle" ) ||
			( assetName == "Tower" ) ||
			( assetName == "Building1" ) ||
			( assetName == "Building2" ) ||
			( assetName == "Building3" ) ||
			( assetName == "Building4" ) ||
			( assetName == "Building5" ) ||
			( assetName == "Building6" ) ||
			( assetName == "Building7" ) ||
			( assetName == "Building8" ) ||
			( assetName == "Building9" );
	}

	static bool ShouldStaticPlacementCastShadow(const std::string& assetName)
	{
		if ( assetName == "Ground" )
			return false;

		if ( assetName == "DirtRoad" )
			return false;

		if ( assetName == "Grass" )
			return false;

		return true;
	}

	static void SetObjectAttackPower(CGameObject* obj, int attackPower)
	{
		if ( !obj )
			return;

		if ( auto* attack = obj->GetComponent<CAttackPowerComponent>() )
			attack->SetAttackPower(attackPower);
	}

	static float DistanceSqXZ(const XMFLOAT3& a, const XMFLOAT3& b)
	{
		const float dx = a.x - b.x;
		const float dz = a.z - b.z;

		return dx * dx + dz * dz;
	}

	static void StoreStaticWorldRows(
		XMFLOAT4& out0,
		XMFLOAT4& out1,
		XMFLOAT4& out2,
		XMFLOAT4& out3,
		const XMFLOAT4X4& W)
	{
		out0 = XMFLOAT4(W._11, W._12, W._13, W._14);
		out1 = XMFLOAT4(W._21, W._22, W._23, W._24);
		out2 = XMFLOAT4(W._31, W._32, W._33, W._34);
		out3 = XMFLOAT4(W._41, W._42, W._43, W._44);
	}

	static bool ContainsGameObjectPtr(
		const std::vector<CGameObject*>& refs,
		const CGameObject* obj)
	{
		if ( !obj )
			return false;

		return std::find(refs.begin(), refs.end(), obj) != refs.end();
	}

	static XMFLOAT3 GetSafeObjectForward(const CGameObject* obj)
	{
		if ( !obj )
			return XMFLOAT3(0.0f, 0.0f, 1.0f);

		const XMFLOAT4X4& W = obj->GetWorldMatrix();

		XMFLOAT3 dir = XMFLOAT3(W._31, W._32, W._33);
		XMVECTOR dirV = XMLoadFloat3(&dir);

		if ( XMVectorGetX(XMVector3LengthSq(dirV)) <= 1.0e-8f )
			return XMFLOAT3(0.0f, 0.0f, 1.0f);

		dirV = XMVector3Normalize(dirV);

		XMFLOAT3 out{};
		XMStoreFloat3(&out, dirV);
		return out;
	}

	static constexpr XMFLOAT3 kLocalPlayerRespawnPosition =
		XMFLOAT3(0.0f, 0.0f, -200.0f);

	static constexpr float kLocalPlayerRespawnDelay = 5.0f;

	static constexpr ELocalStagePreset kLocalStagePreset = ELocalStagePreset::FullStage;

	static constexpr float kFootstepSfxVolume = 0.04f;

	static bool IsWalkClipName(const std::string& clipName)
	{
		return clipName.rfind("Walk_", 0) == 0;
	}

	static bool IsRunClipName(const std::string& clipName)
	{
		return clipName.rfind("Run_", 0) == 0;
	}

	static int GetFootstepModeFromClipName(const std::string& clipName)
	{
		if ( IsWalkClipName(clipName) )
			return 1;

		if ( IsRunClipName(clipName) )
			return 2;

		return 0;
	}

	static bool CrossedNormalizedEvent(
		float prevNormalized,
		float curNormalized,
		float eventNormalized)
	{
		if ( prevNormalized < 0.0f ) prevNormalized = 0.0f;
		if ( prevNormalized > 1.0f ) prevNormalized = 1.0f;

		if ( curNormalized < 0.0f ) curNormalized = 0.0f;
		if ( curNormalized > 1.0f ) curNormalized = 1.0f;

		// 일반 진행
		if ( curNormalized >= prevNormalized )
			return prevNormalized < eventNormalized && eventNormalized <= curNormalized;

		// 루프 wrap: 0.95 -> 0.05 같은 경우
		return eventNormalized > prevNormalized || eventNormalized <= curNormalized;
	}

	static const char* SelectRandomFootstepGrassSfxPath()
	{
		static std::mt19937 rng{ std::random_device{}( ) };
		static std::uniform_int_distribution<int> dist(1, 3);

		switch ( dist(rng) )
		{
		case 1: return "Assets/Audio/Walk_Grass1.wav";
		case 2: return "Assets/Audio/Walk_Grass2.wav";
		case 3: return "Assets/Audio/Walk_Grass3.wav";
		default: break;
		}

		return "Assets/Audio/Walk_Grass1.wav";
	}

	static const char* SelectRandomFootstepBlockSfxPath()
	{
		static std::mt19937 rng{ std::random_device{}( ) };
		static std::uniform_int_distribution<int> dist(1, 3);

		switch ( dist(rng) )
		{
		case 1: return "Assets/Audio/Walk_Block1.wav";
		case 2: return "Assets/Audio/Walk_Block2.wav";
		case 3: return "Assets/Audio/Walk_Block3.wav";
		default: break;
		}

		return "Assets/Audio/Walk_Block1.wav";
	}

	// -----------------------------------------------------------------------------
	// HP
	// -----------------------------------------------------------------------------
	static constexpr int kHpGhoul = 30;
	static constexpr int kHpBowMan = 120;
	static constexpr int kHpSwordMan = 120;
	static constexpr int kHpMutant = 240;
	static constexpr int kHpBoss = 4800;
	static constexpr int kHpPlayer = 100;

	// -----------------------------------------------------------------------------
	// Attack power
	// -----------------------------------------------------------------------------
	static constexpr int kAttackPowerPlayerSword = 10;
	static constexpr int kAttackPowerPlayerAxe = 15;
	static constexpr int kAttackPowerPlayerArrow = 15;
	static constexpr int kAttackPowerPlayerBullet = 8;

	static constexpr int kAttackPowerGhoul = 5;
	static constexpr int kAttackPowerEnemySword = 10;
	static constexpr int kAttackPowerEnemyArrow = 10;
	static constexpr int kAttackPowerMutant = 20;
	static constexpr int kAttackPowerBoss = 50;

	static constexpr UINT kOfflineGhoulAICount = 200;

	static constexpr float kDisableVillageTreeCullPlayerHeight = 3.0f;

	static constexpr int kCastleCenterMegaGridX = 1;
	static constexpr int kCastleCenterMegaGridZ = 1;

	// Castle 텔레포트는 총 4개 이상의 메가그리드가 클리어된 뒤부터 허용한다.
	static constexpr int kRequiredClearedMegaGridCountForCastlePortal = 4;

#ifndef USING_NETWORK
	static constexpr int kTowerDoorPortalCooldownFrames = 30;
	static constexpr float kTowerDoorPortalExitOffset = 2.0f;

	static constexpr int kCastleDoorPortalCooldownFrames = 30;
	static constexpr float kCastleDoorPortalExitOffset = 2.0f;

	static constexpr float kTowerDoorPortalLowerExitYOffset = 0.0f;
	static constexpr float kTowerDoorPortalUpperExitYOffset = 3.5f;
	static constexpr float kTowerDoorPortalUpperHeightThreshold = 10.0f;
	static constexpr float kTowerDoorPortalPlayerYawOffsetFromCamera = 0.0f;

	static constexpr bool kEnableTowerDoorPortalCollisionLog = false;
	static constexpr bool kEnableTowerDoorPortalVerboseLog = false;
#endif
}

namespace
{
    bool ParsePlacementEntryLine(const std::string& line, StaticPlacementEntry& outEntry)
    {
        char asset[64] = {};
        char objectName[128] = {};

        float px = 0.0f, py = 0.0f, pz = 0.0f;
        float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f;

        const int matched = sscanf_s(
            line.c_str(),
            "ENTRY|asset=\"%63[^\"]\"|object=\"%127[^\"]\"|pos=(%f,%f,%f)|rot=(%f,%f,%f,%f)",
            asset, (unsigned)_countof(asset),
            objectName, (unsigned)_countof(objectName),
            &px, &py, &pz,
            &qx, &qy, &qz, &qw
        );

        if (matched != 9) 
			return false;

        outEntry.assetName = asset;
        outEntry.objectName = objectName;
        outEntry.pos = XMFLOAT3(px, py, pz);
        outEntry.rot = XMFLOAT4(qx, qy, qz, qw);
        return true;
    }

	std::string TrimString(const std::string& text)
	{
		size_t begin = 0;
		while ( begin < text.size() && std::isspace(static_cast< unsigned char >(text[begin])) )
			++begin;

		size_t end = text.size();
		while ( end > begin && std::isspace(static_cast< unsigned char >( text[end - 1] )) )
			--end;

		return text.substr(begin, end - begin);
	}

	std::string StripTopRootFromAPath(const std::string& fullAPath)
	{
		const size_t firstSlash = fullAPath.find('/');
		if ( firstSlash == std::string::npos )
			return fullAPath;

		return fullAPath.substr(firstSlash + 1);
	}

	bool ParseVector3Tuple(const std::string& text, XMFLOAT3& outValue)
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;

		if ( sscanf_s(text.c_str(), "(%f, %f, %f)", &x, &y, &z) != 3 )
			return false;

		outValue = XMFLOAT3(x, y, z);
		return true;
	}

	bool ParseVector4Tuple(const std::string& text, XMFLOAT4& outValue)
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 1.0f;

		if ( sscanf_s(text.c_str(), "(%f, %f, %f, %f)", &x, &y, &z, &w) != 4 )
			return false;

		outValue = XMFLOAT4(x, y, z, w);
		return true;
	}

    static std::string ToLowerAscii(const std::string& text)
    {
        std::string out = text;
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    }

#ifndef USING_NETWORK
	static std::string NormalizeTowerDoorNameForMatch(const std::string& text)
	{
		std::string out;
		out.reserve(text.size());

		for ( unsigned char c : text )
		{
			if ( c == ' ' || c == '_' || c == '-' || c == '[' || c == ']' || c == '/' || c == '|' )
				continue;

			out.push_back(static_cast< char >( std::tolower(c) ));
		}

		return out;
	}

	static float NormalizeYawDegrees180(float yaw)
	{
		while ( yaw > 180.0f )
			yaw -= 360.0f;

		while ( yaw <= -180.0f )
			yaw += 360.0f;

		return yaw;
	}

	bool IsTowerDoorFrame2Name(const std::string& meshName, const std::string& authoringPath)
	{
		UNREFERENCED_PARAMETER(authoringPath);

		const std::string name = NormalizeTowerDoorNameForMatch(meshName);
		return name == "doubledoorframe2";
	}

	static int GetCastleDoorFrameIndexFromMeshName(const std::string& meshName)
	{
		const std::string name = NormalizeTowerDoorNameForMatch(meshName);

		static constexpr const char* kPrefix = "doubledoorframe";
		const std::string prefix = kPrefix;

		if ( name == prefix )
			return 0; // Unity: Double Door Frame

		if ( name.rfind(prefix, 0) != 0 )
			return -1;

		const std::string suffix = name.substr(prefix.size());

		if ( suffix.empty() )
			return 0;

		int value = 0;

		for ( unsigned char c : suffix )
		{
			if ( !std::isdigit(c) )
				return -1;

			value = value * 10 + static_cast< int >( c - '0' );
		}

		if ( value < 0 || value > 7 )
			return -1;

		return value;
	}

	static const char* GetCastleDoorFrameDebugName(int index)
	{
		switch ( index )
		{
		case 0: return "Double Door Frame";
		case 1: return "Double Door Frame (1)";
		case 2: return "Double Door Frame (2)";
		case 3: return "Double Door Frame (3)";
		case 4: return "Double Door Frame (4)";
		case 5: return "Double Door Frame (5)";
		case 6: return "Double Door Frame (6)";
		case 7: return "Double Door Frame (7)";
		default: return "Double Door Frame (?)";
		}
	}

	bool IsTowerDoorFrame1Name(const std::string& meshName, const std::string& authoringPath)
	{
		UNREFERENCED_PARAMETER(authoringPath);

		const std::string name = NormalizeTowerDoorNameForMatch(meshName);
		return name == "doubledoorframe";
	}

	static void DebugPrintTowerDoorPortalLine(const char* text)
	{
		if ( !kEnableTowerDoorPortalCollisionLog )
			return;

		OutputDebugStringA(text);
		OutputDebugStringA("\n");
	}

	static void DebugPrintTowerDoorPortalFloat3(
		const char* tag,
		const XMFLOAT3& v)
	{
		if ( !kEnableTowerDoorPortalCollisionLog )
			return;

		char buf[256];
		sprintf_s(
			buf,
			"[TowerDoorPortal][%s] (%.3f, %.3f, %.3f)\n",
			tag,
			v.x,
			v.y,
			v.z
		);
		OutputDebugStringA(buf);
	}

	static void DebugPrintTowerDoorPortalOOBB(
		const char* tag,
		const BoundingOrientedBox& box)
	{
		if ( !kEnableTowerDoorPortalCollisionLog )
			return;

		char buf[512];
		sprintf_s(
			buf,
			"[TowerDoorPortal][%s] center=(%.3f, %.3f, %.3f) extents=(%.3f, %.3f, %.3f)\n",
			tag,
			box.Center.x,
			box.Center.y,
			box.Center.z,
			box.Extents.x,
			box.Extents.y,
			box.Extents.z
		);
		OutputDebugStringA(buf);
	}
#endif

    static bool ResolvePlacementFilePathFromMapId(const std::string& mapId, std::string& outPlacementFilePath)
    {
        std::string normalized = ToLowerAscii(TrimString(mapId));

        if (normalized.size() >= 2 && normalized.front() == '"' && normalized.back() == '"')
            normalized = normalized.substr(1, normalized.size() - 2);

        if (normalized.empty() ||
            normalized == "full" ||
            normalized == "fullstage" ||
            normalized == "map_fullstage" ||
            normalized == "mapdata_fullstage")
        {
            outPlacementFilePath = "MapData/MapData_fullstage(NoTree).txt";
            return true;
        }

        if (normalized == "fullstage_tree" ||
            normalized == "map_fullstage_tree" ||
            normalized == "mapdata_fullstage_tree")
        {
            outPlacementFilePath = "MapData/MapData_fullstage.txt";
            return true;
        }

		if (normalized == "fullstage_withboss" ||
			normalized == "fullstagewithboss" ||
			normalized == "map_fullstage_withboss" ||
			normalized == "map_fullstage_with_boss" ||
			normalized == "mapdata_fullstage_withboss" ||
			normalized == "mapdata_fullstage_with_boss")
		{
			outPlacementFilePath = "MapData/MapData_fullstage(withBoss).txt";
			return true;
		}

        if (normalized == "stage1" ||
            normalized == "map_stage1" ||
            normalized == "mapdata_stage1")
        {
            outPlacementFilePath = "MapData/MapData_stage1_with_Tree.txt";
            return true;
        }

        if (normalized == "test" ||
            normalized == "tst" ||
            normalized == "map_tst" ||
            normalized == "mapdata_tst")
        {
            outPlacementFilePath = "MapData/MapData_tst.txt";
            return true;
        }

        return false;
    }

	void TriggerMonsterTestCommand(CGameObject* obj, EMonsterAnimCommand cmd, EMonsterAnimState locomotion = EMonsterAnimState::Idle)
	{
		if ( !obj ) return;

		auto* animComp = obj->GetComponent<CAnimatorComponent>();
		if ( !animComp ) return;

		auto* ctrl = animComp->EnsureMonsterController();
		if ( !ctrl ) return;

		ctrl->SetLocomotionState(locomotion);
		ctrl->RequestCommand(cmd);
	}
}

CGameScene::CGameScene()
{
    m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };
    m_localPlayerSlot = 0;

	m_grassCount = 1;
    m_groundCount = 1;
    m_villagewallCount = 1;
	m_castleCount = 1;
	m_dirtRoadCount = 1;

    m_building1Count = 1;
    m_building2Count = 1;
    m_building3Count = 1;
    m_building4Count = 1;
    m_building5Count = 1;
    m_building6Count = 1;
    m_building7Count = 1;
    m_building8Count = 1;
    m_building9Count = 1;
    m_towerCount = 1;

    m_ghoulCount = 4;
    m_swordManCount = 3;
    m_bowManCount = 3;
    m_MutantCount = 2;
    m_bossCount = 1;

    m_PlayerCount = 4;

    m_staticBatch.capacity = 0;
    m_staticBatch.count = 0;

    m_skinnedBatch.capacity = 0;
    m_skinnedBatch.count = 0;

    m_arrowRefs.clear();
    m_arrowRefs.shrink_to_fit();

	m_bulletRefs.clear();
	m_bulletRefs.shrink_to_fit();

	m_navMesh.reset();

	m_bLocalPlayerDead = false;
	m_bLocalPlayerRespawnUsed = false;
	m_localPlayerRespawnTimer = 0.0f;
#ifdef USING_NETWORK
	m_prevPlayerNetworkStateCode.clear();
#endif
	m_deadMonsters.clear();

	m_bLocalPlayerInsideCastleCenterMegaGrid = false;
}

void CGameScene::InitializeSpatialGrid()
{
	m_sceneGrid.Initialize();

	for ( auto& tracker : m_playerGridTrackers )
		tracker = GridDynamicTracker{};

	m_monsterGridTrackers.clear();
	m_arrowGridTrackers.clear();
	m_bulletGridTrackers.clear();
}

void CGameScene::ShutdownSpatialGrid()
{
	m_sceneGrid.Shutdown();

	for ( auto& tracker : m_playerGridTrackers )
		tracker = GridDynamicTracker{};

	m_monsterGridTrackers.clear();
	m_arrowGridTrackers.clear();
	m_bulletGridTrackers.clear();
}

bool CGameScene::TryGetTreeCullReferenceGridCell(
	CCamera* camera,
	int& outCellX,
	int& outCellZ,
	int& outMegaX,
	int& outMegaZ) const
{
	outCellX = -1;
	outCellZ = -1;
	outMegaX = -1;
	outMegaZ = -1;

	if ( !m_sceneGrid.IsInitialized() )
		return false;

	if ( camera )
	{
		const XMFLOAT3 cameraPosition = camera->GetPosition();

		if ( m_sceneGrid.WorldToCell(cameraPosition.x, cameraPosition.z, outCellX, outCellZ) )
		{
			if ( m_sceneGrid.FineCellToMegaGridCell(outCellX, outCellZ, outMegaX, outMegaZ) )
				return true;
		}
	}

	CGameObject* localPlayer = GetPlayer();

	if ( !localPlayer )
		localPlayer = GetPlayerBySlot(0);

	if ( !localPlayer )
		return false;

	const XMFLOAT3 playerPosition = localPlayer->GetPosition();

	if ( !m_sceneGrid.WorldToCell(playerPosition.x, playerPosition.z, outCellX, outCellZ) )
		return false;

	return m_sceneGrid.FineCellToMegaGridCell(outCellX, outCellZ, outMegaX, outMegaZ);
}

bool CGameScene::ShouldCullTreesByVillageGrid(CCamera* camera) const
{
	if ( !m_sceneGrid.IsInitialized() )
		return false;

	// Castle 포탈로 중앙 성 내부에 들어간 경우에는
	// 기존 문/성벽 시야 판정과 무관하게 모든 나무를 컬링한다.
	if ( m_bLocalPlayerInsideCastleCenterMegaGrid )
		return true;

	CGameObject* localPlayer = GetPlayer();

	if ( !localPlayer )
		localPlayer = GetPlayerBySlot(0);

	if ( localPlayer )
	{
		const XMFLOAT3 playerPosition = localPlayer->GetPosition();

		// Tower 포탈 등으로 위로 올라간 경우에는
		// 성벽/문 기준 나무 컬링을 하지 않는다.
		if ( playerPosition.y >= kDisableVillageTreeCullPlayerHeight )
			return false;
	}

	int cellX = -1;
	int cellZ = -1;
	int megaX = -1;
	int megaZ = -1;

	if ( !TryGetTreeCullReferenceGridCell(camera, cellX, cellZ, megaX, megaZ) )
		return false;

	return m_sceneGrid.ShouldCullTreesByVillageGridCell(megaX, megaZ, cellX, cellZ);
}

void CGameScene::AddDynamicCount(int cellX, int cellZ, EGridDynamicKind kind, int delta)
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	m_sceneGrid.AddDynamicCount(cellX, cellZ, kind, delta);
}

void CGameScene::RegisterStaticPlacementToGrid(const StaticPlacementEntry& placement, CGameObject* obj)
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	if ( !obj )
		return;

	const bool isBuilding =
		( placement.assetName == "VillageWall" ) ||
		( placement.assetName == "Castle" ) ||
		( placement.assetName == "Tower" ) ||
		( placement.assetName == "Building1" ) ||
		( placement.assetName == "Building2" ) ||
		( placement.assetName == "Building3" ) ||
		( placement.assetName == "Building4" ) ||
		( placement.assetName == "Building5" ) ||
		( placement.assetName == "Building6" ) ||
		( placement.assetName == "Building7" ) ||
		( placement.assetName == "Building8" ) ||
		( placement.assetName == "Building9" ) ||
		( placement.assetName == "Tree1" ) ||
		( placement.assetName == "Tree2" ) ||
		( placement.assetName == "Tree3" ) ||
		( placement.assetName == "Tree4" ) ||
		( placement.assetName == "Tree5" ) ||
		( placement.assetName == "Tree6" );

	if ( !isBuilding )
		return;

	std::unordered_set<int> touchedCells;

	if ( auto* collider = obj->GetComponent<CColliderComponent>() )
	{
		const std::vector<MeshOOBBSet>& meshSets = collider->GetMeshOOBBSets();

		for ( const MeshOOBBSet& set : meshSets )
		{
			for ( const BoundingOrientedBox& subOOBB : set.WorldSubOOBBs )
				m_sceneGrid.StampBuildingCellsFromOOBB(subOOBB, touchedCells);
		}
	}

	if ( touchedCells.empty() )
	{
		int cellX = -1;
		int cellZ = -1;
		const XMFLOAT3 pos = obj->GetPosition();

		if ( m_sceneGrid.WorldToCell(pos.x, pos.z, cellX, cellZ) )
			touchedCells.insert(m_sceneGrid.GridCellIndex(cellX, cellZ));
	}

	m_sceneGrid.AddStaticTouchedCells(touchedCells);

	if ( IsTreeCullBlockerAssetName(placement.assetName) )
		m_sceneGrid.MarkTreeCullBlockerCells(touchedCells);
}

#ifndef USING_NETWORK
void CGameScene::RegisterTowerDoorPortal(CGameObject* tower)
{
	if ( !tower )
		return;

	auto* collider = tower->GetComponent<CColliderComponent>();
	if ( !collider )
		return;

	if ( collider->GetType() != EColliderType::OOBB )
		return;

	TowerDoorPortalEntry entry{};
	entry.tower = tower;
	entry.collider = collider;

	const auto& meshSets = collider->GetMeshOOBBSets();

	for ( size_t meshSetIndex = 0; meshSetIndex < meshSets.size(); ++meshSetIndex )
	{
		const MeshOOBBSet& set = meshSets[meshSetIndex];

		size_t ob = set.SubOOBBMetas.size();
		size_t obmin = set.LocalSubOOBBs.size();
		if ( ob < obmin ) obmin = ob;
		const size_t count = obmin;

		for ( size_t subIndex = 0; subIndex < count; ++subIndex )
		{
			const SubOOBBMeta& meta = set.SubOOBBMetas[subIndex];

			if ( kEnableTowerDoorPortalCollisionLog )
			{
				const bool isDoorA = IsTowerDoorFrame1Name(meta.meshName, meta.authoringPath);
				const bool isDoorB = IsTowerDoorFrame2Name(meta.meshName, meta.authoringPath);

				if ( isDoorA || isDoorB || kEnableTowerDoorPortalVerboseLog )
				{
					char buf[1024];
					sprintf_s(
						buf,
						"[TowerDoorPortal][REGISTER_SCAN] tower=%p meshSet=%zu sub=%zu mesh=\"%s\" path=\"%s\" isDoorA=%d isDoorB=%d\n",
						static_cast< void* >( tower ),
						meshSetIndex,
						subIndex,
						meta.meshName.c_str(),
						meta.authoringPath.c_str(),
						isDoorA ? 1 : 0,
						isDoorB ? 1 : 0
					);
					OutputDebugStringA(buf);
				}
			}

			TowerDoorSubBoxRef ref{};
			ref.meshSetIndex = meshSetIndex;
			ref.subIndex = subIndex;

			if ( IsTowerDoorFrame2Name(meta.meshName, meta.authoringPath) )
			{
				entry.doorBRefs.push_back(ref);
			}
			else if ( IsTowerDoorFrame1Name(meta.meshName, meta.authoringPath) )
			{
				entry.doorARefs.push_back(ref);
			}
		}
	}

	if ( entry.doorARefs.empty() || entry.doorBRefs.empty() )
	{
		if ( kEnableTowerDoorPortalCollisionLog )
		{
			char buf[512];
			sprintf_s(
				buf,
				"[TowerDoorPortal][REGISTER_FAILED] tower=%p meshSetCount=%zu doorABoxes=%zu doorBBoxes=%zu\n",
				static_cast< void* >( tower ),
				meshSets.size(),
				entry.doorARefs.size(),
				entry.doorBRefs.size()
			);
			OutputDebugStringA(buf);
		}
		return;
	}

	char buf[256];
	sprintf_s(
		buf,
		"[TowerDoorPortal][REGISTER] tower=%p doorABoxes=%zu doorBBoxes=%zu\n",
		static_cast< void* >( tower ),
		entry.doorARefs.size(),
		entry.doorBRefs.size()
	);
	OutputDebugStringA(buf);

	m_towerDoorPortals.push_back(std::move(entry));
}

void CGameScene::RegisterCastleDoorPortal(CGameObject* castle)
{
	if ( !castle )
		return;

	auto* collider = castle->GetComponent<CColliderComponent>();
	if ( !collider )
		return;

	if ( collider->GetType() != EColliderType::OOBB )
		return;

	CastleDoorPortalEntry entry{};
	entry.castle = castle;
	entry.collider = collider;

	const auto& meshSets = collider->GetMeshOOBBSets();

	for ( size_t meshSetIndex = 0; meshSetIndex < meshSets.size(); ++meshSetIndex )
	{
		const MeshOOBBSet& set = meshSets[meshSetIndex];

		size_t count = set.SubOOBBMetas.size();

		if ( set.LocalSubOOBBs.size() < count )
			count = set.LocalSubOOBBs.size();

		for ( size_t subIndex = 0; subIndex < count; ++subIndex )
		{
			const SubOOBBMeta& meta = set.SubOOBBMetas[subIndex];

			const int doorIndex = GetCastleDoorFrameIndexFromMeshName(meta.meshName);

			if ( doorIndex < 0 || doorIndex >= 8 )
				continue;

			DoorPortalSubBoxRef ref{};
			ref.meshSetIndex = meshSetIndex;
			ref.subIndex = subIndex;

			entry.doorRefsByIndex[( size_t ) doorIndex].push_back(ref);

			if ( kEnableTowerDoorPortalCollisionLog )
			{
				char buf[1024];
				sprintf_s(
					buf,
					"[CastleDoorPortal][REGISTER_SCAN] castle=%p meshSet=%zu sub=%zu mesh=\"%s\" path=\"%s\" doorIndex=%d doorName=\"%s\"\n",
					static_cast< void* >( castle ),
					meshSetIndex,
					subIndex,
					meta.meshName.c_str(),
					meta.authoringPath.c_str(),
					doorIndex,
					GetCastleDoorFrameDebugName(doorIndex)
				);
				OutputDebugStringA(buf);
			}
		}
	}

	auto AddPair =
		[ &entry ] (int sourceIndex, int targetIndex)
		{
			if ( sourceIndex < 0 || sourceIndex >= 8 )
				return;

			if ( targetIndex < 0 || targetIndex >= 8 )
				return;

			if ( entry.doorRefsByIndex[( size_t ) sourceIndex].empty() )
				return;

			if ( entry.doorRefsByIndex[( size_t ) targetIndex].empty() )
				return;

			CastleDoorPortalPair pair{};
			pair.sourceDoorIndex = sourceIndex;
			pair.targetDoorIndex = targetIndex;
			pair.sourceRefs = entry.doorRefsByIndex[( size_t ) sourceIndex];
			pair.targetRefs = entry.doorRefsByIndex[( size_t ) targetIndex];

			entry.pairs.push_back(std::move(pair));
		};

	// Unity 기준:
	// Double Door Frame (1) -> Double Door Frame 
	// Double Door Frame (2) -> Double Door Frame (4)
	// Double Door Frame (3) -> Double Door Frame (5)
	// Double Door Frame (7) -> Double Door Frame (6)
	AddPair(1, 0);
	AddPair(2, 4);
	AddPair(3, 5);
	AddPair(7, 6);

	if ( entry.pairs.empty() )
	{
		if ( kEnableTowerDoorPortalCollisionLog )
		{
			char buf[512];
			sprintf_s(
				buf,
				"[CastleDoorPortal][REGISTER_FAILED] castle=%p meshSetCount=%zu doorCounts=[%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu]\n",
				static_cast< void* >( castle ),
				meshSets.size(),
				entry.doorRefsByIndex[0].size(),
				entry.doorRefsByIndex[1].size(),
				entry.doorRefsByIndex[2].size(),
				entry.doorRefsByIndex[3].size(),
				entry.doorRefsByIndex[4].size(),
				entry.doorRefsByIndex[5].size(),
				entry.doorRefsByIndex[6].size(),
				entry.doorRefsByIndex[7].size()
			);
			OutputDebugStringA(buf);
		}
		return;
	}

	if ( kEnableTowerDoorPortalCollisionLog )
	{
		char buf[512];
		sprintf_s(
			buf,
			"[CastleDoorPortal][REGISTER] castle=%p pairCount=%zu doorCounts=[%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu]\n",
			static_cast< void* >( castle ),
			entry.pairs.size(),
			entry.doorRefsByIndex[0].size(),
			entry.doorRefsByIndex[1].size(),
			entry.doorRefsByIndex[2].size(),
			entry.doorRefsByIndex[3].size(),
			entry.doorRefsByIndex[4].size(),
			entry.doorRefsByIndex[5].size(),
			entry.doorRefsByIndex[6].size(),
			entry.doorRefsByIndex[7].size()
		);
		OutputDebugStringA(buf);
	}

	m_castleDoorPortals.push_back(std::move(entry));
}

void CGameScene::TickTowerDoorPortalCooldowns()
{
	for ( TowerDoorPortalEntry& portal : m_towerDoorPortals )
	{
		if ( portal.cooldownFrames > 0 )
			--portal.cooldownFrames;
	}

	for ( CastleDoorPortalEntry& portal : m_castleDoorPortals )
	{
		if ( portal.cooldownFrames > 0 )
			--portal.cooldownFrames;
	}
}

bool CGameScene::IsTowerDoorPortalOnCooldown() const
{
	for ( const TowerDoorPortalEntry& portal : m_towerDoorPortals )
	{
		if ( portal.cooldownFrames > 0 )
			return true;
	}

	for ( const CastleDoorPortalEntry& portal : m_castleDoorPortals )
	{
		if ( portal.cooldownFrames > 0 )
			return true;
	}

	return false;
}

int CGameScene::CountClearedMegaGrids() const
{
	int clearedCount = 0;

	for ( int megaNumber = 1; megaNumber <= CSceneGrid::kMegaGridCount; ++megaNumber )
	{
		const int zeroBased = megaNumber - 1;
		const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
		const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

		if ( m_sceneGrid.IsMegaGridCleared(megaX, megaZ) )
			++clearedCount;
	}

	return clearedCount;
}

bool CGameScene::CanUseCastleDoorPortal() const
{
	return CountClearedMegaGrids() >= kRequiredClearedMegaGridCountForCastlePortal;
}

bool CGameScene::TryTeleportLocalPlayerByTowerDoorPortal(bool forceLog)
{
	const bool shouldLog = kEnableTowerDoorPortalCollisionLog && forceLog;

	if ( shouldLog )
	{
		char buf[256];
		sprintf_s(
			buf,
			"[TowerDoorPortal][PROBE_BEGIN] portalCount=%zu localDead=%d\n",
			m_towerDoorPortals.size(),
			m_bLocalPlayerDead ? 1 : 0
		);
		OutputDebugStringA(buf);
	}

	if ( m_bLocalPlayerDead )
	{
		if ( shouldLog )
			DebugPrintTowerDoorPortalLine("[TowerDoorPortal][PROBE_ABORT] local player is dead");
		return false;
	}

	CGameObject* player = GetPlayer();
	if ( !player )
	{
		if ( shouldLog )
			DebugPrintTowerDoorPortalLine("[TowerDoorPortal][PROBE_ABORT] GetPlayer returned null");
		return false;
	}

	auto* playerCollider = player->GetComponent<CColliderComponent>();
	if ( !playerCollider )
	{
		if ( shouldLog )
			DebugPrintTowerDoorPortalLine("[TowerDoorPortal][PROBE_ABORT] player collider missing");
		return false;
	}

	if ( playerCollider->GetType() != EColliderType::BCapsule )
	{
		if ( shouldLog )
			DebugPrintTowerDoorPortalLine("[TowerDoorPortal][PROBE_ABORT] player collider is not BCapsule");
		return false;
	}

	if ( !playerCollider->IsCollisionEnabled() )
	{
		if ( shouldLog )
			DebugPrintTowerDoorPortalLine("[TowerDoorPortal][PROBE_ABORT] player collider disabled");
		return false;
	}

	playerCollider->UpdateWorldBounds();

	const BoundingCapsule playerCapsule = playerCollider->GetBCapsule();
	const XMFLOAT3 playerPos = player->GetPosition();

	auto GetWorldBox =
		[ ](
			const TowerDoorPortalEntry& portal,
			const TowerDoorSubBoxRef& ref
		) -> const BoundingOrientedBox*
		{
			if ( !portal.collider )
				return nullptr;

			const auto& sets = portal.collider->GetMeshOOBBSets();

			if ( ref.meshSetIndex >= sets.size() )
				return nullptr;

			const MeshOOBBSet& set = sets[ref.meshSetIndex];

			if ( ref.subIndex >= set.WorldSubOOBBs.size() )
				return nullptr;

			return &set.WorldSubOOBBs[ref.subIndex];
		};

	auto DoesDoorGroupIntersect =
		[ & ](
			const TowerDoorPortalEntry& portal,
			const std::vector<TowerDoorSubBoxRef>& refs,
			const char* doorName
		) -> bool
		{
			bool anyHit = false;

			for ( const TowerDoorSubBoxRef& ref : refs )
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if ( !box )
				{
					if ( shouldLog )
					{
						char buf[256];
						sprintf_s(
							buf,
							"[TowerDoorPortal][BOX_MISSING] door=%s meshSet=%zu sub=%zu\n",
							doorName,
							ref.meshSetIndex,
							ref.subIndex
						);
						OutputDebugStringA(buf);
					}
					continue;
				}

				const bool hit = playerCapsule.Intersects(*box);

				if ( shouldLog && ( hit || kEnableTowerDoorPortalVerboseLog ) )
				{
					char buf[512];
					sprintf_s(
						buf,
						"[TowerDoorPortal][DOOR_TEST] door=%s hit=%d meshSet=%zu sub=%zu playerPos=(%.3f, %.3f, %.3f) boxCenter=(%.3f, %.3f, %.3f) boxExtents=(%.3f, %.3f, %.3f)\n",
						doorName,
						hit ? 1 : 0,
						ref.meshSetIndex,
						ref.subIndex,
						playerPos.x,
						playerPos.y,
						playerPos.z,
						box->Center.x,
						box->Center.y,
						box->Center.z,
						box->Extents.x,
						box->Extents.y,
						box->Extents.z
					);
					OutputDebugStringA(buf);
				}

				if ( hit )
					anyHit = true;
			}

			return anyHit;
		};

	auto ComputeDoorGroupCenter =
		[ & ](
			const TowerDoorPortalEntry& portal,
			const std::vector<TowerDoorSubBoxRef>& refs,
			XMFLOAT3& outCenter
		) -> bool
		{
			XMVECTOR sum = XMVectorZero();
			int count = 0;

			for ( const TowerDoorSubBoxRef& ref : refs )
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if ( !box )
					continue;

				sum += XMLoadFloat3(&box->Center);
				++count;
			}

			if ( count <= 0 )
				return false;

			sum = XMVectorScale(sum, 1.0f / static_cast< float >( count ));
			XMStoreFloat3(&outCenter, sum);
			return true;
		};

	auto ComputeDoorGroupBottomY =
		[ & ](
			const TowerDoorPortalEntry& portal,
			const std::vector<TowerDoorSubBoxRef>& refs,
			float& outBottomY
		) -> bool
		{
			bool found = false;
			float bottomY = FLT_MAX;

			for ( const TowerDoorSubBoxRef& ref : refs )
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);
				if ( !box )
					continue;

				XMFLOAT3 corners[BoundingOrientedBox::CORNER_COUNT];
				box->GetCorners(corners);

				for ( const XMFLOAT3& corner : corners )
				{
					if ( !found || corner.y < bottomY )
					{
						bottomY = corner.y;
						found = true;
					}
				}
			}

			if ( !found )
				return false;

			outBottomY = bottomY;
			return true;
		};

	auto GetFirstDoorBox =
		[ & ](
			const TowerDoorPortalEntry& portal,
			const std::vector<TowerDoorSubBoxRef>& refs
		) -> const BoundingOrientedBox*
		{
			if ( refs.empty() )
				return nullptr;

			return GetWorldBox(portal, refs.front());
		};

	auto ComputeDoorHorizontalNormal =
		[ & ](
			const BoundingOrientedBox& box,
			XMVECTOR& outNormal
		) -> bool
		{
			// Door frame은 일반적으로 local Z가 두께 방향이다.
			// 그래도 혹시 local X가 더 얇은 경우까지 대비해서,
			// X/Z 중 더 얇은 축을 문의 법선 후보로 쓴다.
			XMVECTOR localAxis = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			if ( box.Extents.x < box.Extents.z )
				localAxis = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

			XMVECTOR q = XMLoadFloat4(&box.Orientation);
			XMVECTOR n = XMVector3Rotate(localAxis, q);

			// 출구 방향은 수평 방향만 사용한다.
			n = XMVectorSetY(n, 0.0f);

			const float lenSq = XMVectorGetX(XMVector3LengthSq(n));
			if ( lenSq <= 1.0e-6f )
				return false;

			outNormal = XMVector3Normalize(n);
			return true;
		};

	auto RotateLocalPlayerAndCameraForPortal =
		[&](
			float yawDeltaDeg,
			float& outOldCameraYaw,
			float& outNewCameraYaw,
			float& outOldPlayerYaw,
			float& outNewPlayerYaw
		) -> bool
		{
			outOldCameraYaw = 0.0f;
			outNewCameraYaw = 0.0f;
			outOldPlayerYaw = 0.0f;
			outNewPlayerYaw = 0.0f;

			bool rotatedAny = false;

			CCamera* camera = GetMainCamera();

			// 1) 카메라 yaw를 먼저 돌린다.
			if ( camera )
			{
				outOldCameraYaw = camera->GetYaw();
				outNewCameraYaw = NormalizeYawDegrees180(outOldCameraYaw + yawDeltaDeg);

				camera->GetYaw() = outNewCameraYaw;

				XMFLOAT3 cameraTarget = player->GetPosition();
				cameraTarget.y += 1.7f;

				camera->Update(cameraTarget, 0.0f);
				camera->SetLookAt(cameraTarget);
				camera->RegenerateViewMatrix();

				rotatedAny = true;
			}

			// 2) 로컬 플레이어의 실제 회전 기준은 CPlayerControllerComponent 쪽이다.
			//    프레임워크 입력 처리도 pc->SetYawDegrees(cameraYawDeg),
			//    pc->RotateTowardYawDegrees(cameraYawDeg, ...)를 사용하므로
			//    포탈 직후에도 controller yaw를 카메라 yaw와 같은 값으로 맞춘다.
			const float desiredPlayerYaw =
				camera
				? outNewCameraYaw
				: NormalizeYawDegrees180(outOldPlayerYaw + yawDeltaDeg);

			if ( auto* tr = player->GetComponent<CTransformComponent>() )
			{
				outOldPlayerYaw = QuaternionToYawDegrees(tr->rotation);
			}

			outNewPlayerYaw = desiredPlayerYaw;

			if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
			{
				controller->SetYawDegrees(outNewPlayerYaw);
				controller->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
				controller->SetInputDirection(static_cast< DWORD >( 0 ));
				controller->SetRunRequested(false);
				rotatedAny = true;
			}
			else if ( auto* tr = player->GetComponent<CTransformComponent>() )
			{
				tr->SetYawDegrees(outNewPlayerYaw);
				rotatedAny = true;
			}
			else
			{
				player->Rotate(0.0f, yawDeltaDeg, 0.0f);
				rotatedAny = true;
			}

			return rotatedAny;
		};

	auto TeleportBetweenDoorGroups =
		[&](
			TowerDoorPortalEntry& portal,
			const std::vector<TowerDoorSubBoxRef>& sourceRefs,
			const std::vector<TowerDoorSubBoxRef>& targetRefs,
			const char* debugFrom,
			const char* debugTo
		) -> bool
		{
			XMFLOAT3 sourceCenter{};
			XMFLOAT3 targetCenter{};

			if ( !ComputeDoorGroupCenter(portal, sourceRefs, sourceCenter) )
				return false;

			if ( !ComputeDoorGroupCenter(portal, targetRefs, targetCenter) )
				return false;

			XMVECTOR sourceV = XMLoadFloat3(&sourceCenter);
			XMVECTOR targetV = XMLoadFloat3(&targetCenter);

			const BoundingOrientedBox* sourceBox = GetFirstDoorBox(portal, sourceRefs);
			const BoundingOrientedBox* targetBox = GetFirstDoorBox(portal, targetRefs);

			XMVECTOR sourceNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			XMVECTOR targetNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			bool hasSourceNormal = false;
			bool hasTargetNormal = false;

			if ( sourceBox )
				hasSourceNormal = ComputeDoorHorizontalNormal(*sourceBox, sourceNormal);

			if ( targetBox )
				hasTargetNormal = ComputeDoorHorizontalNormal(*targetBox, targetNormal);

			// 플레이어가 source 문 기준 어느 면에서 들어왔는지 구한다.
			// 양쪽 문이 같은 방향을 보고 있으면, target에서도 같은 면으로 내보내야 한다.
			float sideSign = 1.0f;

			if ( hasSourceNormal )
			{
				XMVECTOR playerV = XMLoadFloat3(&playerPos);
				XMVECTOR sourceToPlayer = playerV - sourceV;
				sourceToPlayer = XMVectorSetY(sourceToPlayer, 0.0f);

				const float sideDot = XMVectorGetX(XMVector3Dot(sourceToPlayer, sourceNormal));
				sideSign = ( sideDot >= 0.0f ) ? 1.0f : -1.0f;
			}

			XMVECTOR exitDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			if ( hasTargetNormal )
			{
				exitDir = XMVectorScale(targetNormal, sideSign);
			}
			else
			{
				// fallback: 기존 방식
				exitDir = targetV - sourceV;
				exitDir = XMVectorSetY(exitDir, 0.0f);

				const float lenSq = XMVectorGetX(XMVector3LengthSq(exitDir));
				if ( lenSq > 1.0e-6f )
					exitDir = XMVector3Normalize(exitDir);
				else
					exitDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			}

			XMVECTOR dstV = targetV + XMVectorScale(exitDir, kTowerDoorPortalExitOffset);

			XMFLOAT3 dst{};
			XMStoreFloat3(&dst, dstV);

			float targetBottomY = 0.0f;
			float appliedYOffset = 0.0f;

			if ( ComputeDoorGroupBottomY(portal, targetRefs, targetBottomY) )
			{
				appliedYOffset =
					( targetBottomY > kTowerDoorPortalUpperHeightThreshold )
					? kTowerDoorPortalUpperExitYOffset
					: kTowerDoorPortalLowerExitYOffset;

				dst.y = targetBottomY + appliedYOffset;
			}
			else
			{
				dst.y = playerPos.y;
			}

			player->SetPosition(dst);

			float oldCameraYaw = 0.0f;
			float newCameraYaw = 0.0f;
			float oldPlayerYaw = 0.0f;
			float newPlayerYaw = 0.0f;

			const bool rotatedPortalView =
				RotateLocalPlayerAndCameraForPortal(
					180.0f,
					oldCameraYaw,
					newCameraYaw,
					oldPlayerYaw,
					newPlayerYaw
				);

			playerCollider->UpdateWorldBounds();

			if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
			{
				controller->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
				controller->SetInputDirection(static_cast< DWORD >( 0 ));
				controller->SetRunRequested(false);
			}

			portal.cooldownFrames = kTowerDoorPortalCooldownFrames;

			UpdateDynamicGridState();

			return true;
		};

	for ( TowerDoorPortalEntry& portal : m_towerDoorPortals )
	{
		if ( portal.cooldownFrames > 0 )
		{
			if ( shouldLog )
			{
				char buf[256];
				sprintf_s(
					buf,
					"[TowerDoorPortal][SKIP_COOLDOWN] tower=%p cooldownFrames=%d\n",
					static_cast< void* >( portal.tower ),
					portal.cooldownFrames
				);
				OutputDebugStringA(buf);
			}
			continue;
		}

		if ( !portal.collider )
		{
			if ( shouldLog )
				DebugPrintTowerDoorPortalLine("[TowerDoorPortal][SKIP] portal collider is null");
			continue;
		}

		const bool hitDoorA = DoesDoorGroupIntersect(
			portal,
			portal.doorARefs,
			"Double Door Frame"
		);

		const bool hitDoorB = DoesDoorGroupIntersect(
			portal,
			portal.doorBRefs,
			"Double Door Frame2"
		);

		if ( shouldLog )
		{
			char buf[512];
			sprintf_s(
				buf,
				"[TowerDoorPortal][PROBE_RESULT] tower=%p doorARefs=%zu doorBRefs=%zu hitA=%d hitB=%d\n",
				static_cast< void* >( portal.tower ),
				portal.doorARefs.size(),
				portal.doorBRefs.size(),
				hitDoorA ? 1 : 0,
				hitDoorB ? 1 : 0
			);
			OutputDebugStringA(buf);
		}

		// 양쪽이 동시에 맞으면 문 중앙/겹침 상태일 수 있으므로 이번 프레임은 무시.
		if ( hitDoorA && hitDoorB )
		{
			if ( shouldLog )
				DebugPrintTowerDoorPortalLine("[TowerDoorPortal][SKIP_BOTH_HIT] both door groups intersected");
			continue;
		}

		if ( hitDoorA )
		{
			if ( TeleportBetweenDoorGroups(
				portal,
				portal.doorARefs,
				portal.doorBRefs,
				"Double Door Frame",
				"Double Door Frame2") )
			{
				return true;
			}
		}
		else if ( hitDoorB )
		{
			if ( TeleportBetweenDoorGroups(
				portal,
				portal.doorBRefs,
				portal.doorARefs,
				"Double Door Frame2",
				"Double Door Frame") )
			{
				return true;
			}
		}
	}

	if ( shouldLog )
		DebugPrintTowerDoorPortalLine("[TowerDoorPortal][NO_DOOR_HIT] no registered door OOBB intersected player capsule");

	return false;
}

bool CGameScene::TryTeleportLocalPlayerByCastleDoorPortal(bool forceLog)
{
	const bool shouldLog = kEnableTowerDoorPortalCollisionLog && forceLog;

	if ( !CanUseCastleDoorPortal() )
	{
		if ( shouldLog )
		{
			char buf[256];
			sprintf_s(
				buf,
				"[CastleDoorPortal][LOCKED] clearedMegaGrids=%d required=%d\n",
				CountClearedMegaGrids(),
				kRequiredClearedMegaGridCountForCastlePortal
			);
			OutputDebugStringA(buf);
		}

		return false;
	}

	if ( m_bLocalPlayerDead )
		return false;

	CGameObject* player = GetPlayer();
	if ( !player )
		return false;

	auto* playerCollider = player->GetComponent<CColliderComponent>();
	if ( !playerCollider )
		return false;

	if ( playerCollider->GetType() != EColliderType::BCapsule )
		return false;

	if ( !playerCollider->IsCollisionEnabled() )
		return false;

	playerCollider->UpdateWorldBounds();

	const BoundingCapsule playerCapsule = playerCollider->GetBCapsule();
	const XMFLOAT3 playerPos = player->GetPosition();

	auto GetWorldBox =
		[ ](
			const CastleDoorPortalEntry& portal,
			const DoorPortalSubBoxRef& ref
		) -> const BoundingOrientedBox*
		{
			if ( !portal.collider )
				return nullptr;

			const auto& sets = portal.collider->GetMeshOOBBSets();

			if ( ref.meshSetIndex >= sets.size() )
				return nullptr;

			const MeshOOBBSet& set = sets[ref.meshSetIndex];

			if ( ref.subIndex >= set.WorldSubOOBBs.size() )
				return nullptr;

			return &set.WorldSubOOBBs[ref.subIndex];
		};

	auto DoesDoorGroupIntersect =
		[ & ](
			const CastleDoorPortalEntry& portal,
			const std::vector<DoorPortalSubBoxRef>& refs,
			int doorIndex
		) -> bool
		{
			bool anyHit = false;

			for ( const DoorPortalSubBoxRef& ref : refs )
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);

				if ( !box )
					continue;

				const bool hit = playerCapsule.Intersects(*box);

				if ( shouldLog && ( hit || kEnableTowerDoorPortalVerboseLog ) )
				{
					char buf[512];
					sprintf_s(
						buf,
						"[CastleDoorPortal][DOOR_TEST] door=\"%s\" index=%d hit=%d meshSet=%zu sub=%zu playerPos=(%.3f, %.3f, %.3f) boxCenter=(%.3f, %.3f, %.3f) boxExtents=(%.3f, %.3f, %.3f)\n",
						GetCastleDoorFrameDebugName(doorIndex),
						doorIndex,
						hit ? 1 : 0,
						ref.meshSetIndex,
						ref.subIndex,
						playerPos.x,
						playerPos.y,
						playerPos.z,
						box->Center.x,
						box->Center.y,
						box->Center.z,
						box->Extents.x,
						box->Extents.y,
						box->Extents.z
					);
					OutputDebugStringA(buf);
				}

				if ( hit )
					anyHit = true;
			}

			return anyHit;
		};

	auto ComputeDoorGroupCenter =
		[ & ](
			const CastleDoorPortalEntry& portal,
			const std::vector<DoorPortalSubBoxRef>& refs,
			XMFLOAT3& outCenter
		) -> bool
		{
			XMVECTOR sum = XMVectorZero();
			int count = 0;

			for ( const DoorPortalSubBoxRef& ref : refs )
			{
				const BoundingOrientedBox* box = GetWorldBox(portal, ref);

				if ( !box )
					continue;

				sum += XMLoadFloat3(&box->Center);
				++count;
			}

			if ( count <= 0 )
				return false;

			sum = XMVectorScale(sum, 1.0f / static_cast< float >( count ));
			XMStoreFloat3(&outCenter, sum);
			return true;
		};

	auto GetFirstDoorBox =
		[ & ](
			const CastleDoorPortalEntry& portal,
			const std::vector<DoorPortalSubBoxRef>& refs
		) -> const BoundingOrientedBox*
		{
			if ( refs.empty() )
				return nullptr;

			return GetWorldBox(portal, refs.front());
		};

	auto ComputeDoorHorizontalNormal =
		[ ](
			const BoundingOrientedBox& box,
			XMVECTOR& outNormal
		) -> bool
		{
			XMVECTOR localAxis = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			if ( box.Extents.x < box.Extents.z )
				localAxis = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

			XMVECTOR q = XMLoadFloat4(&box.Orientation);
			XMVECTOR n = XMVector3Rotate(localAxis, q);
			n = XMVectorSetY(n, 0.0f);

			const float lenSq = XMVectorGetX(XMVector3LengthSq(n));

			if ( lenSq <= 1.0e-6f )
				return false;

			outNormal = XMVector3Normalize(n);
			return true;
		};

	auto TeleportCastleDoorPair =
		[ & ](
			CastleDoorPortalEntry& portal,
			const CastleDoorPortalPair& pair
		) -> bool
		{
			XMFLOAT3 sourceCenter{};
			XMFLOAT3 targetCenter{};

			if ( !ComputeDoorGroupCenter(portal, pair.sourceRefs, sourceCenter) )
				return false;

			if ( !ComputeDoorGroupCenter(portal, pair.targetRefs, targetCenter) )
				return false;

			const BoundingOrientedBox* sourceBox =
				GetFirstDoorBox(portal, pair.sourceRefs);

			const BoundingOrientedBox* targetBox =
				GetFirstDoorBox(portal, pair.targetRefs);

			XMVECTOR sourceV = XMLoadFloat3(&sourceCenter);
			XMVECTOR targetV = XMLoadFloat3(&targetCenter);

			XMVECTOR sourceNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			XMVECTOR targetNormal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			const bool hasSourceNormal =
				sourceBox && ComputeDoorHorizontalNormal(*sourceBox, sourceNormal);

			const bool hasTargetNormal =
				targetBox && ComputeDoorHorizontalNormal(*targetBox, targetNormal);

			float sideSign = 1.0f;

			if ( hasSourceNormal )
			{
				XMVECTOR playerV = XMLoadFloat3(&playerPos);
				XMVECTOR sourceToPlayer = playerV - sourceV;
				sourceToPlayer = XMVectorSetY(sourceToPlayer, 0.0f);

				const float sideDot =
					XMVectorGetX(XMVector3Dot(sourceToPlayer, sourceNormal));

				sideSign = ( sideDot >= 0.0f ) ? 1.0f : -1.0f;
			}

			XMVECTOR exitDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

			if ( hasTargetNormal )
			{
				// Castle 문은 방향이 서로 반대이므로,
				// Tower처럼 카메라/플레이어를 돌리지 않고 같은 world 진행 방향으로 나오게 둔다.
				exitDir = XMVectorScale(targetNormal, sideSign);
			}
			else
			{
				exitDir = targetV - sourceV;
				exitDir = XMVectorSetY(exitDir, 0.0f);

				const float lenSq = XMVectorGetX(XMVector3LengthSq(exitDir));

				if ( lenSq > 1.0e-6f )
					exitDir = XMVector3Normalize(exitDir);
				else
					exitDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			}

			XMVECTOR dstV =
				targetV + XMVectorScale(exitDir, kCastleDoorPortalExitOffset);

			XMFLOAT3 dst{};
			XMStoreFloat3(&dst, dstV);

			// Castle 포탈은 같은 높이끼리 이동한다.
			// 문 OOBB center/bottom y를 쓰지 않고 현재 플레이어 높이를 유지한다.
			dst.y = playerPos.y;

			player->SetPosition(dst);
			playerCollider->UpdateWorldBounds();

			if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
			{
				controller->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
				controller->SetInputDirection(static_cast< DWORD >( 0 ));
				controller->SetRunRequested(false);
			}

			portal.cooldownFrames = kCastleDoorPortalCooldownFrames;

			UpdateDynamicGridState();
			MarkLocalPlayerEnteredCastleCenterMegaGrid();

			if ( shouldLog )
			{
				XMFLOAT3 exitDirF{};
				XMStoreFloat3(&exitDirF, exitDir);

				char buf[768];
				sprintf_s(
					buf,
					"[CastleDoorPortal][TELEPORT] %s -> %s dst=(%.3f, %.3f, %.3f) targetCenter=(%.3f, %.3f, %.3f) exitDir=(%.3f, %.3f, %.3f) sideSign=%.1f\n",
					GetCastleDoorFrameDebugName(pair.sourceDoorIndex),
					GetCastleDoorFrameDebugName(pair.targetDoorIndex),
					dst.x,
					dst.y,
					dst.z,
					targetCenter.x,
					targetCenter.y,
					targetCenter.z,
					exitDirF.x,
					exitDirF.y,
					exitDirF.z,
					sideSign
				);
				OutputDebugStringA(buf);
			}

			return true;
		};

	if ( shouldLog )
	{
		char buf[256];
		sprintf_s(
			buf,
			"[CastleDoorPortal][PROBE_BEGIN] portalCount=%zu localDead=%d\n",
			m_castleDoorPortals.size(),
			m_bLocalPlayerDead ? 1 : 0
		);
		OutputDebugStringA(buf);
	}

	for ( CastleDoorPortalEntry& portal : m_castleDoorPortals )
	{
		if ( portal.cooldownFrames > 0 )
		{
			if ( shouldLog )
			{
				char buf[256];
				sprintf_s(
					buf,
					"[CastleDoorPortal][SKIP_COOLDOWN] castle=%p cooldownFrames=%d\n",
					static_cast< void* >( portal.castle ),
					portal.cooldownFrames
				);
				OutputDebugStringA(buf);
			}
			continue;
		}

		if ( !portal.collider )
			continue;

		for ( const CastleDoorPortalPair& pair : portal.pairs )
		{
			const bool hitSource =
				DoesDoorGroupIntersect(
					portal,
					pair.sourceRefs,
					pair.sourceDoorIndex
				);

			if ( shouldLog && ( hitSource || kEnableTowerDoorPortalVerboseLog ) )
			{
				char buf[512];
				sprintf_s(
					buf,
					"[CastleDoorPortal][PROBE_RESULT] castle=%p %s -> %s sourceRefs=%zu targetRefs=%zu hitSource=%d\n",
					static_cast< void* >( portal.castle ),
					GetCastleDoorFrameDebugName(pair.sourceDoorIndex),
					GetCastleDoorFrameDebugName(pair.targetDoorIndex),
					pair.sourceRefs.size(),
					pair.targetRefs.size(),
					hitSource ? 1 : 0
				);
				OutputDebugStringA(buf);
			}

			if ( !hitSource )
				continue;

			if ( TeleportCastleDoorPair(portal, pair) )
				return true;
		}
	}

	if ( shouldLog )
		DebugPrintTowerDoorPortalLine("[CastleDoorPortal][NO_DOOR_HIT] no source castle door OOBB intersected player capsule");

	return false;
}
#endif

void CGameScene::ResetDynamicGridCounts()
{
	m_sceneGrid.ResetDynamicCounts();
}

bool CGameScene::TryGetTrackedCell(const CGameObject* obj, int& outCellX, int& outCellZ) const
{
	if ( !obj )
		return false;

	const XMFLOAT3 pos = obj->GetPosition();

	if ( pos.y < -100.0f )
		return false;

	return m_sceneGrid.WorldToCell(pos.x, pos.z, outCellX, outCellZ);
}

void CGameScene::RefreshDynamicTracker(GridDynamicTracker& tracker, EGridDynamicKind kind)
{
	int currentCellX = -1;
	int currentCellZ = -1;
	const bool hasCurrentCell = TryGetTrackedCell(tracker.object, currentCellX, currentCellZ);

	if ( tracker.occupied )
	{
		if ( !hasCurrentCell ||
			tracker.prevCellX != currentCellX ||
			tracker.prevCellZ != currentCellZ )
		{
			AddDynamicCount(tracker.prevCellX, tracker.prevCellZ, kind, -1);
			tracker.occupied = false;
		}
	}

	if ( hasCurrentCell )
	{
		if ( !tracker.occupied ||
			tracker.prevCellX != currentCellX ||
			tracker.prevCellZ != currentCellZ )
		{
			AddDynamicCount(currentCellX, currentCellZ, kind, +1);
			tracker.prevCellX = currentCellX;
			tracker.prevCellZ = currentCellZ;
			tracker.occupied = true;
		}
	}
}

void CGameScene::BuildDynamicGridTrackers()
{
	for ( size_t i = 0; i < m_playerGridTrackers.size(); ++i )
	{
		m_playerGridTrackers[i] = GridDynamicTracker{};
		m_playerGridTrackers[i].object = m_playersBySlot[i];
	}

	m_monsterGridTrackers.clear();
	m_arrowGridTrackers.clear();
	m_bulletGridTrackers.clear();
}

void CGameScene::RebuildDynamicGridState()
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	ResetDynamicGridCounts();
	BuildDynamicGridTrackers();

	for ( auto& tracker : m_playerGridTrackers )
		RefreshDynamicTracker(tracker, EGridDynamicKind::Player);

	UpdateMegaGridState();
	UpdateCastleCenterMegaGridState();
}

void CGameScene::UpdateDynamicGridState()
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	for ( auto& tracker : m_playerGridTrackers )
		RefreshDynamicTracker(tracker, EGridDynamicKind::Player);

	UpdateMegaGridState();
	UpdateCastleCenterMegaGridState();
}

void CGameScene::UpdateMegaGridState()
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	for ( const GridDynamicTracker& tracker : m_playerGridTrackers )
	{
		if ( !tracker.occupied )
			continue;

		int megaX = -1;
		int megaZ = -1;

		if ( !m_sceneGrid.FineCellToMegaGridCell(tracker.prevCellX, tracker.prevCellZ, megaX, megaZ) )
			continue;

		// 중앙 메가그리드 Castle은 기존 200x200 approach zone으로 진입 처리하지 않는다.
		// Castle 포탈 성공 여부로만 중앙 메가그리드 접근을 처리한다.
		if ( megaX == kCastleCenterMegaGridX &&
			 megaZ == kCastleCenterMegaGridZ )
		{
			if ( m_bLocalPlayerInsideCastleCenterMegaGrid )
			{
				m_sceneGrid.SetMegaGridPlayerApproached(
					kCastleCenterMegaGridX,
					kCastleCenterMegaGridZ,
					true
				);
			}

			continue;
		}

		if ( !m_sceneGrid.IsFineCellInsideMegaGridApproachZone(
			megaX,
			megaZ,
			tracker.prevCellX,
			tracker.prevCellZ) )
		{
			continue;
		}

		if ( !m_sceneGrid.HasMegaGridPlayerApproached(megaX, megaZ) )
		{
			m_sceneGrid.SetMegaGridPlayerApproached(megaX, megaZ, true);
		}
	}
}

void CGameScene::RegisterMonsterToMegaGrid(
	CGameObject* monster,
	const XMFLOAT3& spawnPosition,
	UINT skinnedBatchObjectIndex)
{
	if ( !monster )
		return;

	if ( skinnedBatchObjectIndex >= m_skinnedMonsterMegaGridNumbers.size() )
		m_skinnedMonsterMegaGridNumbers.resize(( size_t ) skinnedBatchObjectIndex + 1, -1);

	const int megaNumber =
		m_sceneGrid.MegaGridNumberFromWorldPosition(spawnPosition.x, spawnPosition.z);

	m_skinnedMonsterMegaGridNumbers[( size_t ) skinnedBatchObjectIndex] = megaNumber;

	if ( megaNumber <= 0 )
		return;

	m_collisionMegaGridMaskByObject[monster] =
		static_cast< uint16_t >( 1u << ( megaNumber - 1 ) );

	const int zeroBased = megaNumber - 1;
	const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
	const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

	m_sceneGrid.AddMonsterToMegaGrid(megaX, megaZ, monster);
}

int CGameScene::GetLocalPlayerMegaGridNumberForMonsterTick() const
{
	CGameObject* player = GetPlayer();

	if ( !player )
		player = GetPlayerBySlot(0);

	if ( !player )
		return -1;

	const XMFLOAT3 pos = player->GetPosition();
	return m_sceneGrid.MegaGridNumberFromWorldPosition(pos.x, pos.z);
}

bool CGameScene::ShouldSkipMonsterByMegaGrid(
	const CGameObject* monster,
	UINT skinnedBatchObjectIndex,
	int activeMegaGridNumber) const
{
	if ( !monster )
		return false;

	if ( activeMegaGridNumber <= 0 )
		return false;

	auto* tag = monster->GetComponent<CActorTagComponent>();
	if ( !tag || tag->kind != EActorKind::NPC )
		return false;

	if ( skinnedBatchObjectIndex >= static_cast< UINT >( m_skinnedMonsterMegaGridNumbers.size() ) )
		return false;

	const int monsterMegaGridNumber =
		m_skinnedMonsterMegaGridNumbers[( size_t ) skinnedBatchObjectIndex];

	if ( monsterMegaGridNumber <= 0 )
		return false;

	return monsterMegaGridNumber != activeMegaGridNumber;
}

uint16_t CGameScene::ComputeStaticObjectMegaGridMask(CGameObject* obj) const
{
	if ( !obj )
		return 0;

	auto* collider = obj->GetComponent<CColliderComponent>();
	if ( !collider )
		return 0;

	uint16_t mask = 0;

	if ( collider->GetType() == EColliderType::OOBB )
	{
		const std::vector<MeshOOBBSet>& meshSets = collider->GetMeshOOBBSets();

		for ( const MeshOOBBSet& set : meshSets )
		{
			for ( const BoundingOrientedBox& box : set.WorldSubOOBBs )
			{
				XMFLOAT3 corners[BoundingOrientedBox::CORNER_COUNT] = {};
				box.GetCorners(corners);

				float minX = corners[0].x;
				float maxX = corners[0].x;
				float minZ = corners[0].z;
				float maxZ = corners[0].z;

				for ( int i = 1; i < BoundingOrientedBox::CORNER_COUNT; ++i )
				{
					minX = min(minX, corners[i].x);
					maxX = max(maxX, corners[i].x);
					minZ = min(minZ, corners[i].z);
					maxZ = max(maxZ, corners[i].z);
				}

				int beginCellX = static_cast< int >(floor(minX)) - CSceneGrid::kGridMinX;
				int endCellX = static_cast< int >(ceil(maxX)) - CSceneGrid::kGridMinX - 1;
				int beginCellZ = static_cast< int >(floor(minZ)) - CSceneGrid::kGridMinZ;
				int endCellZ = static_cast< int >( ceil(maxZ) ) - CSceneGrid::kGridMinZ - 1;

				beginCellX = std::clamp(beginCellX, 0, CSceneGrid::kGridWidth - 1);
				endCellX = std::clamp(endCellX, 0, CSceneGrid::kGridWidth - 1);
				beginCellZ = std::clamp(beginCellZ, 0, CSceneGrid::kGridHeight - 1);
				endCellZ = std::clamp(endCellZ, 0, CSceneGrid::kGridHeight - 1);

				if ( beginCellX > endCellX || beginCellZ > endCellZ )
					continue;

				const int beginMegaX = beginCellX / CSceneGrid::kMegaGridCellWidth;
				const int endMegaX = endCellX / CSceneGrid::kMegaGridCellWidth;
				const int beginMegaZ = beginCellZ / CSceneGrid::kMegaGridCellHeight;
				const int endMegaZ = endCellZ / CSceneGrid::kMegaGridCellHeight;

				for ( int mz = beginMegaZ; mz <= endMegaZ; ++mz )
				{
					for ( int mx = beginMegaX; mx <= endMegaX; ++mx )
					{
						if ( mx < 0 || mx >= CSceneGrid::kMegaGridCols )
							continue;

						if ( mz < 0 || mz >= CSceneGrid::kMegaGridRows )
							continue;

						const int bit = mz * CSceneGrid::kMegaGridCols + mx;
						mask |= static_cast< uint16_t >(1u << bit);
					}
				}
			}
		}
	}

	if ( mask != 0 )
		return mask;

	return ComputeObjectCurrentMegaGridMask(obj);
}

uint16_t CGameScene::ComputeObjectCurrentMegaGridMask(const CGameObject* obj) const
{
	if ( !obj )
		return 0;

	const XMFLOAT3 pos = obj->GetPosition();

	int megaX = -1;
	int megaZ = -1;

	if ( !m_sceneGrid.TryGetMegaGridFromWorldPosition(pos.x, pos.z, megaX, megaZ) )
		return 0;

	const int bit = megaZ * CSceneGrid::kMegaGridCols + megaX;
	return static_cast< uint16_t >( 1u << bit );
}

uint16_t CGameScene::GetCollisionMegaGridMaskForObject(const CGameObject* obj) const
{
	if ( !obj )
		return 0;

	const auto it = m_collisionMegaGridMaskByObject.find(obj);
	if ( it != m_collisionMegaGridMaskByObject.end() )
		return it->second;

	return ComputeObjectCurrentMegaGridMask(obj);
}

bool CGameScene::ShouldKeepCollisionPairByMegaGrid(
	const CColliderComponent* a,
	const CColliderComponent* b) const
{
	if ( !a || !b )
		return false;

	// 월드 지형/건물과 플레이어, 몬스터, 투사체, 무기 충돌은 grid mask로 필터링한다.
	// 정적 건물은 위치 1점이 아니라 m_staticCollisionMegaGridMasks를 우선 사용한다.
	const CGameObject* objA = a->GetOwner();
	const CGameObject* objB = b->GetOwner();

	const uint16_t maskA = GetCollisionMegaGridMaskForObject(objA);
	const uint16_t maskB = GetCollisionMegaGridMaskForObject(objB);

	// grid 밖으로 나간 투사체/오브젝트는 충돌 후보에서 제거.
	if ( maskA == 0 || maskB == 0 )
		return false;

	return ( maskA & maskB ) != 0;
}

void CGameScene::MarkLocalPlayerEnteredCastleCenterMegaGrid()
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	m_bLocalPlayerInsideCastleCenterMegaGrid = true;

	// Castle 포탈을 탄 순간 중앙 메가그리드 중앙에 접근한 것으로 처리한다.
	// 기존 200x200 approach zone 판정은 중앙 메가그리드에서는 쓰지 않는다.
	m_sceneGrid.SetMegaGridPlayerApproached(
		kCastleCenterMegaGridX,
		kCastleCenterMegaGridZ,
		true
	);
}

bool CGameScene::IsLocalPlayerInsideCastleCenterMegaGridFullArea() const
{
	if ( !m_sceneGrid.IsInitialized() )
		return false;

	if ( m_localPlayerSlot < 0 ||
		 m_localPlayerSlot >= static_cast< int >(m_playerGridTrackers.size()) )
	{
		return false;
	}

	const GridDynamicTracker& tracker =
		m_playerGridTrackers[static_cast< size_t >(m_localPlayerSlot)];

	if ( !tracker.occupied )
		return false;

	int megaX = -1;
	int megaZ = -1;

	if ( !m_sceneGrid.FineCellToMegaGridCell(
		tracker.prevCellX,
		tracker.prevCellZ,
		megaX,
		megaZ) )
	{
		return false;
	}

	return
		megaX == kCastleCenterMegaGridX &&
		megaZ == kCastleCenterMegaGridZ;
}

void CGameScene::UpdateCastleCenterMegaGridState()
{
	if ( !m_sceneGrid.IsInitialized() )
		return;

	if ( !m_bLocalPlayerInsideCastleCenterMegaGrid )
		return;

	// Castle 포탈 후에는 중앙 메가그리드 400x400 안에 있는 동안
	// 계속 중앙 메가그리드 내부로 취급한다.
	if ( IsLocalPlayerInsideCastleCenterMegaGridFullArea() )
	{
		m_sceneGrid.SetMegaGridPlayerApproached(
			kCastleCenterMegaGridX,
			kCastleCenterMegaGridZ,
			true
		);
		return;
	}

	// 죽고 부활해서 중앙 메가그리드 400x400 밖으로 나간 경우.
	m_bLocalPlayerInsideCastleCenterMegaGrid = false;

	m_sceneGrid.SetMegaGridPlayerApproached(
		kCastleCenterMegaGridX,
		kCastleCenterMegaGridZ,
		false
	);
}

void CGameScene::DumpStaticGridOccupancyLog() const
{
	m_sceneGrid.DumpStaticGridOccupancyLog();
}

void CGameScene::SetMegaGridApproachZoneSize(int megaX, int megaZ, int widthCells, int heightCells)
{
	m_sceneGrid.SetMegaGridApproachZoneSize(megaX, megaZ, widthCells, heightCells);
}

void CGameScene::SetMegaGridCleared(int megaX, int megaZ, bool cleared)
{
	m_sceneGrid.SetMegaGridCleared(megaX, megaZ, cleared);
}

void CGameScene::SetMegaGridEventOccurred(int megaX, int megaZ, bool occurred)
{
	m_sceneGrid.SetMegaGridEventOccurred(megaX, megaZ, occurred);
}

bool CGameScene::HasMegaGridPlayerApproached(int megaX, int megaZ) const
{
	return m_sceneGrid.HasMegaGridPlayerApproached(megaX, megaZ);
}

bool CGameScene::IsMegaGridCleared(int megaX, int megaZ) const
{
	return m_sceneGrid.IsMegaGridCleared(megaX, megaZ);
}

bool CGameScene::HasMegaGridEventOccurred(int megaX, int megaZ) const
{
	return m_sceneGrid.HasMegaGridEventOccurred(megaX, megaZ);
}


CGameScene::~CGameScene()
{
}

void CGameScene::ReleaseObjects()
{
    m_staticBatch.shader.reset();
    m_skinnedBatch.shader.reset();

	m_treeStaticShader.reset();
	m_treeAlphaClipObjects.clear();
	m_skinnedAlphaClipObjects.clear();

	m_staticTreeGridCullFlags.clear();
	m_staticShadowCasterFlags.clear();
	m_staticTreeObjectIndices.clear();
	m_staticShadowOcclusionEntryIndices.clear();
	m_staticCollisionMegaGridMasks.clear();
	m_collisionMegaGridMaskByObject.clear();
	m_skinnedShadowOcclusionEntryIndices.clear();

#ifndef USING_NETWORK
	m_towerDoorPortals.clear();
	m_castleDoorPortals.clear();
#endif

	m_staticObjects.clear();
	m_skinnedObjects.clear();

    m_lightObjects.clear();
    m_pPlayerSpotFollower = nullptr;

    m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };

    m_staticBatch.objectRefs.clear();
    m_skinnedBatch.objectRefs.clear();

	m_colliderBatch.shader.reset();
	m_colliderBatch.objectRefs.clear();
	m_colliderObjects.clear();
	m_ColliderCount = 0;

	m_swordManRefs.clear();
	m_bowManRefs.clear();
	m_MutantRefs.clear();
	m_bossRefs.clear();

	m_mutantKeyTriggerMegaByObject.clear();
	m_mutantKeyTriggerRegisteredByMega.fill(false);

	m_helmetRefs.clear();
	m_arrowRefs.clear();
	m_bulletRefs.clear();

	m_attachmentBinds.clear();
	m_staticInstanceGroups.clear();
	ResetStaticWorldLodEntries();
	ResetStaticOcclusionEntries();
	m_staticOcclusionUnitBoxMesh.reset();
	m_skinnedInstanceGroups.clear();
	ResetSkinnedWorldLodEntries();
	ResetSkinnedOcclusionEntries();

    m_PlayerSwordRefs.clear();
    m_PlayerBowRefs.clear();
    m_PlayerAxeRefs.clear();
    m_PlayerGunRefs.clear();

    m_EnemySwordRefs.clear();
    m_EnemyBowRefs.clear();

	ResetPlayerFootstepSfxState();
	m_preparedPlayerArrows = { nullptr, nullptr, nullptr, nullptr };
	m_prevBowLoadPhase = { false, false, false, false };
	m_prevBowReleasePhase = { false, false, false, false };
	m_preparedBowmanArrows.clear();
	m_prevEnemyBowReleasePhase.clear();

	m_hud.ReleaseResources();
	// mShadowMap.reset();
	// mShadowShader.reset();
	// if ( m_pd3dShadowDsvDescriptorHeap )
	//     m_pd3dShadowDsvDescriptorHeap.Reset();
	m_depthFog.ReleaseResources();

	m_occlusionStaticShader.reset();
	m_shadowStaticShader.reset();
	m_shadowAlphaClipStaticShader.reset();
	m_shadowSkinnedShader.reset();
	m_shadowAlphaClipSkinnedShader.reset();

	m_sceneRenderTargetCount = 0;
	m_bSceneRenderTargetsReady = false;
	m_bInactiveOverlayVisible = false;
	m_bStartedGameplayMusic = false;
	m_bWasLocalPlayerInsideMegaGridCenter = false;
	m_bLocalPlayerInsideCastleCenterMegaGrid = false;

	m_navMesh.reset();

	m_bLocalPlayerDead = false;
	m_bLocalPlayerRespawnUsed = false;
	m_localPlayerRespawnTimer = 0.0f;
#ifdef USING_NETWORK
	m_prevPlayerNetworkStateCode.clear();
#endif
	m_deadMonsters.clear();
	m_skinnedMonsterMegaGridNumbers.clear();

	m_itemBillboardShader.reset();
	m_transparentItemBillboardShader.reset();

	m_itemBillboardQuadMesh.reset();
	m_itemBillboards.clear();
	m_keyItemTexture.reset();

	ReleaseItemBillboardGpuResources();
	m_muzzleFlashShader.reset();
	m_muzzleFlashes.clear();
	m_swordTrailShader.reset();
	m_swordTrails.clear();

	m_staticRenderObjectCache.clear();

#ifndef USING_NETWORK
	m_monsterSpawnEntries.clear();
#endif

	ShutdownSpatialGrid();

	ReleaseShaderVariables();

	CScene::ReleaseObjects();
}

//void CGameScene::InitShadowMap(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
//{
//	if ( !dev || !cmd || !m_pd3dGraphicsRootSignature )
//		return;
//
//	mShadowShader = std::make_shared<CShadowShader>();
//	mShadowShader->CreateShader(
//		dev,
//		m_pd3dGraphicsRootSignature.Get(),
//		0,
//		nullptr,
//		DXGI_FORMAT_D24_UNORM_S8_UINT);
//
//	constexpr UINT kShadowMapWidth = 2048;
//	constexpr UINT kShadowMapHeight = 2048;
//	mShadowMap = std::make_unique<ShadowMap>(
//		dev,
//		mShadowShader.get(),
//		kShadowMapWidth,
//		kShadowMapHeight);
//
//	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
//	dsvHeapDesc.NumDescriptors = 1;
//	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
//	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
//	dsvHeapDesc.NodeMask = 0;
//	HRESULT hr = dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_pd3dShadowDsvDescriptorHeap));
//	assert(SUCCEEDED(hr));
//
//	const UINT shadowSrvIndex = CScene::m_pDescriptorHeap->AllocateSrvRangeBack(1);
//	assert(shadowSrvIndex != UINT_MAX);
//	D3D12_CPU_DESCRIPTOR_HANDLE shadowSrvCpu = CScene::m_pDescriptorHeap->GetCPUSrvHandle(shadowSrvIndex);
//	D3D12_GPU_DESCRIPTOR_HANDLE shadowSrvGpu = CScene::m_pDescriptorHeap->GetGPUSrvHandle(shadowSrvIndex);
//	D3D12_CPU_DESCRIPTOR_HANDLE shadowDsvCpu = m_pd3dShadowDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
//
//	mShadowMap->OnCreate(shadowDsvCpu);
//	mShadowMap->BuildDescriptors(shadowSrvCpu, shadowSrvGpu, shadowDsvCpu);
//
//	CGameObject* targetPlayer = GetPlayer();
//	if ( !targetPlayer )
//		targetPlayer = GetPlayerBySlot(0);
//	mShadowMap->SetTargetObject(targetPlayer);
//
//	for ( auto& lo : m_lightObjects )
//	{
//		if ( !lo ) continue;
//		auto* lc = lo->GetComponent<CLightComponent>();
//		if ( !lc ) continue;
//		if ( lc->type == ELightType::Directional )
//		{
//			mShadowMap->SetLightComponent(lc);
//			break;
//		}
//	}
//
//	mShadowMap->OnUpdate();
//	mShadowMap->UpdateShadowPassCB();
//}

//void CGameScene::RenderShadowMap(ID3D12GraphicsCommandList* cmd, const CGameTimer& gt)
//{
//	if ( !cmd || !mShadowMap )
//		return;
//
//	CGameObject* targetPlayer = GetPlayer();
//	if ( !targetPlayer )
//		targetPlayer = GetPlayerBySlot(0);
//	mShadowMap->SetTargetObject(targetPlayer);
//
//	std::vector<CGameObject*> casters;
//	casters.reserve(m_staticObjects.size());
//
//	for ( auto& obj : m_staticObjects )
//	{
//		if ( obj )
//			casters.push_back(obj.get());
//	}
//
//	// NOTE:
//	// 현재 shadow 전용 VS(Shadows.hlsl)는 스키닝(본 인덱스/가중치)을 처리하지 않는다.
//	// 스키닝 섀도우 패스를 별도로 구현하기 전까지는 정적 오브젝트만 그림자 캐스터로 사용한다.
//
//	mShadowMap->Render(gt, cmd, casters);
//}

void CGameScene::ReleaseUploadBuffers()
{
    for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
    {
        if (!m_staticObjects[j]) continue;
        m_staticObjects[j]->ReleaseUploadBuffers();
    }
    for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
    {
        if (!m_skinnedObjects[j]) continue;
        m_skinnedObjects[j]->ReleaseUploadBuffers();
    }

#ifdef _WITH_BATCH_MATERIAL
    if (m_staticBatch.material)  m_staticBatch.material->ReleaseUploadBuffers();
#endif

	if ( m_itemBillboardQuadMesh )
		m_itemBillboardQuadMesh->ReleaseUploadBuffers();

	if ( m_keyItemTexture )
		m_keyItemTexture->ReleaseUploadBuffers();
}

void CGameScene::ReleaseShaderVariables()
{
	ReleaseItemBillboardGpuResources();
	ReleaseStaticOcclusionGpuResources();
	ReleaseSkinnedOcclusionGpuResources();

	if ( m_pd3dStaticInstanceBuffer )
	{
		if ( m_pMappedStaticInstanceBuffer )
		{
			m_pd3dStaticInstanceBuffer->Unmap(0, NULL);
			m_pMappedStaticInstanceBuffer = nullptr;
		}
		m_pd3dStaticInstanceBuffer.Reset();
	}
	m_staticInstanceBufferCapacity = 0;

	if ( m_pd3dSkinnedInstanceBuffer )
	{
		if ( m_pMappedSkinnedInstanceBuffer )
		{
			m_pd3dSkinnedInstanceBuffer->Unmap(0, NULL);
			m_pMappedSkinnedInstanceBuffer = nullptr;
		}
		m_pd3dSkinnedInstanceBuffer.Reset();
	}
	m_skinnedInstanceBufferCapacity = 0;

	if ( m_pd3dSkinnedBonePaletteBuffer )
	{
		if ( m_pMappedSkinnedBonePaletteBuffer )
		{
			m_pd3dSkinnedBonePaletteBuffer->Unmap(0, NULL);
			m_pMappedSkinnedBonePaletteBuffer = nullptr;
		}
		m_pd3dSkinnedBonePaletteBuffer.Reset();
	}
	m_skinnedBonePaletteStride = 0;
	m_skinnedBonePaletteCapacity = 0;

	// ---- Static batch CB ----
	if ( m_staticBatch.cbGameObjects )
	{
		if ( m_staticBatch.mappedGameObjects )
		{
			m_staticBatch.cbGameObjects->Unmap(0, NULL);
			m_staticBatch.mappedGameObjects = nullptr;
		}
		m_staticBatch.cbGameObjects.Reset();
	}


    // ---- Skinned batch CB ----
    if (m_skinnedBatch.cbGameObjects)
    {
        if (m_skinnedBatch.mappedGameObjects)
        {
            m_skinnedBatch.cbGameObjects->Unmap(0, NULL);
            m_skinnedBatch.mappedGameObjects = nullptr;
        }
        m_skinnedBatch.cbGameObjects.Reset();
    }

    if (m_pd3dcbLights)
    {
        m_pd3dcbLights->Unmap(0, NULL);
        m_pd3dcbLights.Reset();
    }
    m_pcbMappedLights = nullptr;

    if (m_pd3dcbMaterials)
    {
        m_pd3dcbMaterials->Unmap(0, NULL);
        m_pd3dcbMaterials.Reset();
    }
    m_pcbMappedMaterials = nullptr;

	m_depthFog.ReleaseConstantBuffer();

	m_shadowMap.ReleaseResources();

	if ( m_colliderBatch.cbGameObjects )
	{
		if ( m_colliderBatch.mappedGameObjects )
		{
			m_colliderBatch.cbGameObjects->Unmap(0, NULL);
			m_colliderBatch.mappedGameObjects = nullptr;
		}
		m_colliderBatch.cbGameObjects.Reset();
	}

	m_hud.ReleaseResources();
	m_depthFog.ReleaseShaderVariables();
}

void CGameScene::BuildObjects(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
	ResetPlayerFootstepSfxState();
	m_deadMonsters.clear();
	m_bLocalPlayerDead = false;
	m_bLocalPlayerRespawnUsed = false;
	m_localPlayerRespawnTimer = 0.0f;

#ifdef USING_NETWORK
	m_prevPlayerNetworkStateCode.clear();

	while ( false == g_GameStarted )
	{
		OutputDebugStringA("Waiting for game start message...\n");
	}

	DequeueNetworkMessage(NetworkMessageType::GameStart);
	m_localPlayerSlot = g_myPlayerId;

	if ( !std::holds_alternative<GameStartData>(m_pendingNetworkMessage.data) )
	{
		assert(false && "Missing GameStartData in network mode");
		return;
	}

	const GameStartData& gameStartData = std::get<GameStartData>(m_pendingNetworkMessage.data);

	std::string placementFilePath;
	if ( !ResolvePlacementFilePathFromMapId(gameStartData.mapId, placementFilePath) )
	{
		assert(false && "Unknown mapId received from server");
		return;
	}

	if ( !LoadStaticPlacementFile(placementFilePath) )
	{
		assert(false && "Failed to load placement data for mapId");
		return;
	}

	ApplyStaticPlacementCounts();

	m_ghoulCount = 0;
	m_swordManCount = 0;
	m_bowManCount = 0;
	m_MutantCount = 0;
	m_bossCount = 0;

	for ( const EnemyState& enemyState : gameStartData.enemies )
	{
		switch ( enemyState.enemyType )
		{
		case kNetworkEnemyTypeArcher:
			++m_bowManCount;
			break;
		case kNetworkEnemyTypeWarrior:
			++m_swordManCount;
			break;
		case kNetworkEnemyTypeBoss:
			++m_bossCount;
			break;
		case kNetworkEnemyTypeMutant:
			++m_MutantCount;
			break;
		case kNetworkEnemyTypeNone:
		case kNetworkEnemyTypeBasic:
		default:
			++m_ghoulCount;
			break;
		}
	}
#else
	m_localPlayerSlot = 0;

	const GameSceneStageFileSet& stageFiles = GetLocalStageFileSet(kLocalStagePreset);

	if ( !LoadStaticPlacementFile(stageFiles.placementFilePath) )
	{
		assert(false && "Failed to load placement_export");
		return;
	}

	if ( !LoadSceneCubeBoxColliderReport(stageFiles.cubeColliderReportFilePath) )
	{
		assert(false && "Failed to load cube box collider report");
		return;
	}

	m_navMesh = std::make_unique<CNavMesh>();
	if ( !m_navMesh->LoadFromFile(stageFiles.navMeshFilePath) )
	{
		assert(false && "Failed to load navmesh file");
		m_navMesh.reset();
		return;
	}

	if ( !LoadMonsterSpawnFile(stageFiles.monsterSpawnFilePath) )
	{
		assert(false && "Failed to load monster spawn file");
		return;
	}
#endif

//#ifndef USING_NETWORK
//
//#else
//	m_staticPlacementEntries.clear();
//	ResetStaticPlacementCounts();
//#endif

#ifdef USING_NETWORK
	if ( std::holds_alternative<GameStartData>(m_pendingNetworkMessage.data) )
		m_PlayerCount = static_cast< UINT >( std::get<GameStartData>(m_pendingNetworkMessage.data).players.size() );
	else
		m_PlayerCount = 4;
#else
	m_PlayerCount = 4;
#endif

	m_PlayerSwordCount = m_PlayerCount;
	m_PlayerBowCount = m_PlayerCount;
	m_PlayerAxeCount = m_PlayerCount;
	m_PlayerGunCount = m_PlayerCount;

	m_helmetCount = m_MutantCount;

#ifdef USING_NETWORK
	const UINT worldStaticCount = static_cast< UINT >( m_staticPlacementEntries.size() );
#else
	const UINT worldStaticCount = static_cast< UINT >( m_staticPlacementEntries.size() );
#endif

	m_staticBatch.capacity =
		worldStaticCount +
		kArrowPoolSize +
		kBulletPoolSize +
		m_helmetCount +
		m_PlayerSwordCount +
		m_PlayerAxeCount +
		m_PlayerGunCount +
		m_swordManCount;

	m_skinnedBatch.capacity =
		m_ghoulCount +
		m_swordManCount +
		m_bowManCount +
		m_MutantCount +
		m_bossCount +
		m_PlayerCount +
		m_PlayerBowCount +
		m_bowManCount;

	m_colliderBatch.capacity = m_staticBatch.capacity + m_skinnedBatch.capacity;

	m_staticBatch.count = 0;
	m_skinnedBatch.count = 0;
	m_colliderBatch.count = 0;

	CreateGraphicsRootSignature(dev);
	InitializeSpatialGrid();

	auto pStaticShader = std::make_shared<CStaticObjectsShader>();
	auto pTreeStaticShader = std::make_shared<CTreeStaticObjectsShader>();
	auto pSkinnedShader = std::make_shared<CSkinnedObjectsShader>();
	auto pColliderShader = std::make_shared<CDiffusedShader>();
	auto pOcclusionStaticShader = std::make_shared<COcclusionStaticShader>();
	auto pShadowStaticShader = std::make_shared<CShadowMapStaticShader>();
	auto pShadowAlphaClipStaticShader = std::make_shared<CShadowMapAlphaClipStaticShader>();
	auto pShadowSkinnedShader = std::make_shared<CShadowMapSkinnedShader>();
	auto pShadowAlphaClipSkinnedShader = std::make_shared<CShadowMapAlphaClipSkinnedShader>();

	m_staticBatch.shader = pStaticShader;
	m_treeStaticShader = pTreeStaticShader;
	m_skinnedBatch.shader = pSkinnedShader;
	m_colliderBatch.shader = pColliderShader;
	m_occlusionStaticShader = pOcclusionStaticShader;
	m_shadowStaticShader = pShadowStaticShader;
	m_shadowAlphaClipStaticShader = pShadowAlphaClipStaticShader;
	m_shadowSkinnedShader = pShadowSkinnedShader;
	m_shadowAlphaClipSkinnedShader = pShadowAlphaClipSkinnedShader;

	DXGI_FORMAT rtvFormats[5] =
	{
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R32_FLOAT
	};

	BuildLightsAndMaterials();
	m_hud.BuildResources(dev, cmd, GetGraphicsRootSignature());
	m_hud.SetInactiveOverlayVisible(m_bInactiveOverlayVisible);
	BuildDepthFogResources(dev, cmd);
	m_shadowMap.BuildResources(dev, cmd, m_pDescriptorHeap.get());

	for ( auto& lo : m_lightObjects )
	{
		if ( lo ) 
			lo->CreateComponents(dev, cmd);
	}

	constexpr UINT kRTCount = 5;
	const DXGI_FORMAT kDsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

#ifndef USING_NETWORK
	//BuildColliderBatch(dev, cmd, pColliderShader, kRTCount, rtvFormats, kDsvFormat);
#endif

	pTreeStaticShader->CreateShader(dev, m_pd3dGraphicsRootSignature.Get(), kRTCount, rtvFormats, kDsvFormat);
	pOcclusionStaticShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		0,
		nullptr,
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);

	pShadowStaticShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		0,
		nullptr,
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);

	pShadowAlphaClipStaticShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		0,
		nullptr,
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);

	pShadowSkinnedShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		0,
		nullptr,
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);

	pShadowAlphaClipSkinnedShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		0,
		nullptr,
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);

	BuildStaticBatch(dev, cmd, pStaticShader, kRTCount, rtvFormats, kDsvFormat);
	BuildItemBillboardBatch(dev, cmd, kRTCount, rtvFormats, kDsvFormat);
	
#ifndef USING_NETWORK
	//DumpStaticGridOccupancyLog();
	//BuildStaticWorldSubmeshOOBBDebugObjects(dev, cmd);
#endif
	BuildSkinnedBatch(dev, cmd, pSkinnedShader, kRTCount, rtvFormats, kDsvFormat);

	for ( CGameObject* obj : m_skinnedBatch.objectRefs )
	{
		if ( !obj )
			continue;

		if ( auto* hp = obj->GetComponent<CHealthComponent>() )
			hp->SetAudioManager(m_pAudioManager);
	}

	LinkSceneObjects();

	CreateShaderVariables(dev, cmd);

	CGameObject* local = GetPlayer();
	if ( !local ) 
		local = GetPlayerBySlot(0);

	CreateMainCamera(dev, cmd, local);
	//InitShadowMap(dev, cmd);
	BuildObjectsCollider();

	RebuildDynamicGridState();
#ifdef USING_NETWORK
	Protocol::C_CLIENT_READY iamReady;

	iamReady.set_ready(true);
	iamReady.set_playerid(g_myPlayerId);
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(iamReady);
	g_clientService->BroadCast(sendBuffer);
#endif
}

void CGameScene::BuildStaticWorldSubmeshOOBBDebugObjects(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd)
{
	if ( !dev || !cmd ) return;
	if ( !m_colliderBatch.mappedGameObjects ) return;

	for ( auto& ownerObj : m_staticObjects )
	{
		if ( !ownerObj ) continue;

		auto* ownerCollider = ownerObj->GetComponent<CColliderComponent>();
		if ( !ownerCollider ) continue;
		if ( ownerCollider->GetType() != EColliderType::OOBB ) continue;
		if ( ownerCollider->GetLayer() != kCollisionLayerWorldStatic ) continue;

		const std::vector<MeshOOBBSet>& meshSets = ownerCollider->GetMeshOOBBSets();
		if ( meshSets.empty() ) continue;

		for ( const MeshOOBBSet& set : meshSets )
		{
			for ( const BoundingOrientedBox& subOOBB : set.WorldSubOOBBs )
			{
				if ( m_ColliderCount >= m_colliderBatch.capacity )
				{
					OutputDebugStringA("[DebugOOBB] capacity reached\n");
					return;
				}

				const UINT i = m_ColliderCount;

				auto debugObj = std::make_unique<CGameObject>(1);

				auto* cb = reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >( m_colliderBatch.mappedGameObjects ) +
					i * m_colliderBatch.cbElementBytes
				);

				debugObj->SetMappedGameObjectCB(cb);
				debugObj->SetCbvGPUDescriptorHandlePtr(
					m_colliderBatch.baseCbvGpu.ptr + ( UINT64 ) i * m_colliderBatch.cbvInc
				);

				debugObj->AddComponent<CColliderMeshRendererComponent>();
				auto* debugCollider = debugObj->AddComponent<CColliderComponent>(EColliderType::OOBB);

				debugObj->CreateComponents(dev, cmd);

				// local unit box
				debugCollider->SetOOBB(
					XMFLOAT3(-0.5f, -0.5f, -0.5f),
					XMFLOAT3(0.5f, 0.5f, 0.5f)
				);

				// 이 월드행렬로 unit box -> subOOBB world box
				const XMFLOAT4X4 subWorld = BuildWorldMatrixFromOOBB(subOOBB);
				debugObj->SetWorldMatrix(subWorld);

				// OnUpdate는 빈 함수라 의미 없음
				debugCollider->UpdateWorldBounds();

				// 여기서 이미 world-space 꼭짓점으로 메쉬가 bake됨
				std::shared_ptr<CMesh> debugMesh =
					std::make_shared<CBoxMeshDiffused>(dev, cmd, debugCollider);

				debugObj->SetMesh(0, debugMesh);

				// bake 끝났으니 object transform은 identity로 돌려야 이중 변환이 안 생김
				debugObj->SetWorldMatrix(BuildIdentityMatrix4x4());

				CGameObject* raw = debugObj.get();
				m_colliderObjects.push_back(std::move(debugObj));
				m_colliderBatch.objectRefs.push_back(raw);
				m_colliderBatch.count = ( UINT ) m_colliderBatch.objectRefs.size();

				++m_ColliderCount;
			}
		}
	}
}

float CGameScene::QuaternionToYawDegrees(const XMFLOAT4& q)
{
    // yaw(heading) only
    const float siny_cosp = 2.0f * (q.w * q.y + q.x * q.z);
    const float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);

    const float yawRad = std::atan2(siny_cosp, cosy_cosp);
    return XMConvertToDegrees(yawRad);
}

void CGameScene::ResetStaticPlacementCounts()
{
    m_grassCount = 0;
    m_groundCount = 0;
    m_villagewallCount = 0;
	m_castleCount = 0;
    m_dirtRoadCount = 0;

    m_building1Count = 0;
    m_building2Count = 0;
    m_building3Count = 0;
    m_building4Count = 0;
    m_building5Count = 0;
    m_building6Count = 0;
    m_building7Count = 0;
    m_building8Count = 0;
    m_building9Count = 0;
    m_towerCount = 0;
}

void CGameScene::ApplyStaticPlacementCounts()
{
    ResetStaticPlacementCounts();

    for (const auto& e : m_staticPlacementEntries)
    {
        if (e.assetName == "Grass")       ++m_grassCount;
        else if (e.assetName == "Ground")      ++m_groundCount;
        else if (e.assetName == "VillageWall") ++m_villagewallCount;
		else if ( e.assetName == "Castle" )    ++m_castleCount;
        else if (e.assetName == "DirtRoad")    ++m_dirtRoadCount;
        else if (e.assetName == "Building1")   ++m_building1Count;
        else if (e.assetName == "Building2")   ++m_building2Count;
        else if (e.assetName == "Building3")   ++m_building3Count;
        else if (e.assetName == "Building4")   ++m_building4Count;
        else if (e.assetName == "Building5")   ++m_building5Count;
        else if (e.assetName == "Building6")   ++m_building6Count;
        else if (e.assetName == "Building7")   ++m_building7Count;
        else if (e.assetName == "Building8")   ++m_building8Count;
        else if (e.assetName == "Building9")   ++m_building9Count;
        else if (e.assetName == "Tower")       ++m_towerCount;
    }
}

#ifndef USING_NETWORK
bool CGameScene::LoadMonsterSpawnFile(const std::string& filePath)
{
	m_monsterSpawnEntries.clear();

	std::ifstream fin(filePath);
	if ( !fin.is_open() )
	{
		m_ghoulCount = 0;
		m_swordManCount = 0;
		m_bowManCount = 0;
		m_MutantCount = 0;
		m_bossCount = 0;
		return false;
	}

	std::string line;
	while ( std::getline(fin, line) )
	{
		if ( !line.empty() && line.back() == '\r' )
			line.pop_back();

		if ( line.rfind("SPAWN|", 0) != 0 )
			continue;

		MonsterSpawnEntry entry{};

		char type[32] = {};
		float px = 0.0f;
		float py = 0.0f;
		float pz = 0.0f;
		float yawDeg = 0.0f;
		int megaX = -1;
		int megaZ = -1;

		const int matched = sscanf_s(
			line.c_str(),
			"SPAWN|index=%d|type=\"%31[^\"]\"|mega_id=%d|mega=(%d,%d)|pos=(%f,%f,%f)|yaw_deg=%f",
			&entry.index,
			type, ( unsigned ) _countof(type),
			&entry.megaId,
			&megaX,
			&megaZ,
			&px,
			&py,
			&pz,
			&yawDeg
		);

		if ( matched != 9 )
			continue;

		entry.type = type;
		entry.megaX = megaX;
		entry.megaZ = megaZ;
		entry.pos = XMFLOAT3(px, py, pz);
		entry.yawDeg = yawDeg;

		m_monsterSpawnEntries.push_back(std::move(entry));
	}

	std::sort(
		m_monsterSpawnEntries.begin(),
		m_monsterSpawnEntries.end(),
		[ ] (const MonsterSpawnEntry& a, const MonsterSpawnEntry& b)
		{
			return a.index < b.index;
		}
	);

	ApplyMonsterSpawnCounts();
	return !m_monsterSpawnEntries.empty();
}

void CGameScene::ApplyMonsterSpawnCounts()
{
	m_ghoulCount = 0;
	m_swordManCount = 0;
	m_bowManCount = 0;
	m_MutantCount = 0;
	m_bossCount = 0;

	for ( const MonsterSpawnEntry& entry : m_monsterSpawnEntries )
	{
		if ( entry.type == "Ghoul" ) ++m_ghoulCount;
		else if ( entry.type == "SwordMan" ) ++m_swordManCount;
		else if ( entry.type == "BowMan" ) ++m_bowManCount;
		else if ( entry.type == "Mutant" ) ++m_MutantCount;
		else if ( entry.type == "Boss" ) ++m_bossCount;
	}
}
#endif

bool CGameScene::LoadStaticPlacementFile(const std::string& filePath)
{
    m_staticPlacementEntries.clear();
    ResetStaticPlacementCounts();

    std::ifstream fin(filePath);
    if (!fin.is_open())
        return false;

    std::string line;
    while (std::getline(fin, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.rfind("ENTRY|", 0) != 0)
            continue;

        StaticPlacementEntry entry{};
        if (!ParsePlacementEntryLine(line, entry))
            continue;

        entry.yawDeg = QuaternionToYawDegrees(entry.rot);
        m_staticPlacementEntries.push_back(std::move(entry));
    }

    ApplyStaticPlacementCounts();
    return !m_staticPlacementEntries.empty();
}

bool CGameScene::LoadSceneCubeBoxColliderReport(const std::string& filePath)
{
	mSceneCubeBoxColliderTable.clear();

	std::ifstream fin(filePath);
	if ( !fin.is_open() )
		return false;

	const std::string kTopRootPrefix = "TopRootName:";
	const std::string kAPathPrefix = "APath:";
	const std::string kCenterPrefix = "CenterInTopRoot_Local:";
	const std::string kRotationPrefix = "RotationInTopRoot_LocalQuat:";
	const std::string kSizePrefix = "SizeInTopRoot_Local:";

	std::string line;
	std::string currentTopRootName;
	std::string currentRelativeAPath;

	AuthoredSubMeshOOBB currentBox{};
	bool hasCenter = false;
	bool hasRotation = false;
	bool hasSize = false;

	while ( std::getline(fin, line) )
	{
		if ( !line.empty() && line.back() == '\r' )
			line.pop_back();

		const std::string trimmed = TrimString(line);

		if ( trimmed.rfind(kTopRootPrefix, 0) == 0 )
		{
			currentTopRootName = TrimString(trimmed.substr(kTopRootPrefix.size()));
			continue;
		}

		if ( trimmed.rfind(kAPathPrefix, 0) == 0 )
		{
			const std::string fullAPath = TrimString(trimmed.substr(kAPathPrefix.size()));
			currentRelativeAPath = StripTopRootFromAPath(fullAPath);
			continue;
		}

		if ( trimmed.rfind(kCenterPrefix, 0) == 0 )
		{
			hasCenter = ParseVector3Tuple(
				TrimString(trimmed.substr(kCenterPrefix.size())),
				currentBox.Center
			);
			continue;
		}

		if ( trimmed.rfind(kRotationPrefix, 0) == 0 )
		{
			hasRotation = ParseVector4Tuple(
				TrimString(trimmed.substr(kRotationPrefix.size())),
				currentBox.RotationQuat
			);
			continue;
		}

		if ( trimmed.rfind(kSizePrefix, 0) == 0 )
		{
			hasSize = ParseVector3Tuple(
				TrimString(trimmed.substr(kSizePrefix.size())),
				currentBox.Size
			);

			if ( hasCenter && hasRotation && hasSize &&
				!currentTopRootName.empty() &&
				!currentRelativeAPath.empty() )
			{
				mSceneCubeBoxColliderTable[currentTopRootName][currentRelativeAPath].push_back(currentBox);
			}

			currentBox = AuthoredSubMeshOOBB{};
			hasCenter = false;
			hasRotation = false;
			hasSize = false;
		}
	}

	return !mSceneCubeBoxColliderTable.empty();
}

bool CGameScene::ExportStaticWorldLocalOOBBReport(
	const std::string& filePath,
	const std::vector<size_t>& placementIndices,
	const std::vector<CGameObject*>& objects
) const
{
	if ( placementIndices.size() != objects.size() )
		return false;

	std::ofstream fout(filePath, std::ios::out | std::ios::trunc);
	if ( !fout.is_open() )
		return false;

	fout << std::fixed << std::setprecision(6);

	auto WriteFloat3 = [ &fout ] (const char* label, const XMFLOAT3& v)
		{
			fout
				<< label
				<< "(" << v.x << ", " << v.y << ", " << v.z << ")\n";
		};

	auto WriteFloat4 = [ &fout ] (const char* label, const XMFLOAT4& v)
		{
			fout
				<< label
				<< "(" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")\n";
		};

	auto WriteOOBB = [ & ] (const std::string& prefix, const BoundingOrientedBox& box)
		{
			fout
				<< prefix
				<< "Center: ("
				<< box.Center.x << ", "
				<< box.Center.y << ", "
				<< box.Center.z << ")\n";

			fout
				<< prefix
				<< "RotationQuat: ("
				<< box.Orientation.x << ", "
				<< box.Orientation.y << ", "
				<< box.Orientation.z << ", "
				<< box.Orientation.w << ")\n";

			fout
				<< prefix
				<< "Extents: ("
				<< box.Extents.x << ", "
				<< box.Extents.y << ", "
				<< box.Extents.z << ")\n";

			fout
				<< prefix
				<< "Size: ("
				<< box.Extents.x * 2.0f << ", "
				<< box.Extents.y * 2.0f << ", "
				<< box.Extents.z * 2.0f << ")\n";
		};

	fout << "StaticWorldLocalOOBBReportBegin\n";
	fout << "ObjectCount: " << objects.size() << "\n\n";

	for ( size_t i = 0; i < objects.size(); ++i )
	{
		const size_t placementIndex = placementIndices[i];
		if ( placementIndex >= m_staticPlacementEntries.size() )
			continue;

		CGameObject* obj = objects[i];
		if ( !obj )
			continue;

		auto* collider = obj->GetComponent<CColliderComponent>();
		if ( !collider )
			continue;

		const StaticPlacementEntry& placement = m_staticPlacementEntries[placementIndex];
		const BoundingOrientedBox& objectLocalOOBB = collider->GetLocalOOBB();
		const std::vector<MeshOOBBSet>& meshSets = collider->GetMeshOOBBSets();

		fout << "ObjectBegin\n";
		fout << "PlacementIndex: " << placementIndex << "\n";
		fout << "AssetName: " << placement.assetName << "\n";
		fout << "ObjectName: " << placement.objectName << "\n";
		WriteFloat3("ObjectWorldPosition: ", placement.pos);
		WriteFloat4("ObjectWorldRotationQuat: ", placement.rot);
		fout << "ObjectWorldYawDeg: " << placement.yawDeg << "\n";

		WriteOOBB("OverallLocalOOBB.", objectLocalOOBB);

		fout << "MeshOOBBSetCount: " << meshSets.size() << "\n";

		for ( size_t meshIndex = 0; meshIndex < meshSets.size(); ++meshIndex )
		{
			const MeshOOBBSet& set = meshSets[meshIndex];

			fout << "MeshSetBegin\n";
			fout << "MeshIndex: " << meshIndex << "\n";

			WriteOOBB("MeshLocalOOBB.", set.LocalMeshOOBB);

			fout << "SubOOBBCount: " << set.LocalSubOOBBs.size() << "\n";

			for ( size_t subIndex = 0; subIndex < set.LocalSubOOBBs.size(); ++subIndex )
			{
				fout << "SubOOBBBegin\n";
				fout << "SubIndex: " << subIndex << "\n";
				WriteOOBB("SubLocalOOBB.", set.LocalSubOOBBs[subIndex]);
				fout << "SubOOBBEnd\n";
			}

			fout << "MeshSetEnd\n";
		}

		fout << "ObjectEnd\n\n";
	}

	fout << "StaticWorldLocalOOBBReportEnd\n";
	return true;
}

void CGameScene::BuildLightsAndMaterials()
{
	m_lightObjects.clear();
	m_lightObjects.reserve(1);
	m_pPlayerSpotFollower = nullptr;

	// [0] Directional Light only
	{
		auto obj = std::make_unique<CGameObject>(0);

		if ( auto* tr = obj->GetComponent<CTransformComponent>() )
		{
			// 오른쪽 위 앞쪽에서 왼쪽 아래 뒤쪽으로 비추는 방향광
			tr->SetLookDirection(XMFLOAT3(1.0f, -1.0f, 0.3f));
		}

		auto* lc = obj->AddComponent<CLightComponent>();
		lc->type = ELightType::Directional;

		// 전역 환경광은 LIGHTS::m_xmf4GlobalAmbient에서 따로 넣고 있으므로
		// 여기 directional ambient는 0으로 유지.
		lc->ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		// 실질적으로 의미 있는 방향광.
		lc->diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		// 스펙큘러가 필요 없으면 0 유지.
		lc->specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		m_lightObjects.push_back(std::move(obj));
	}

	m_pMaterials = make_unique<MATERIALS>();
	::ZeroMemory(m_pMaterials.get(), sizeof(MATERIALS));

    m_pMaterials->m_pReflections[0] = {
        XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 5.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[1] = {
        XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f),
        XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 10.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[2] = {
        XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 15.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[3] = {
        XMFLOAT4(0.5f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(0.0f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 20.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[4] = {
        XMFLOAT4(0.0f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(0.5f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 25.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[5] = {
        XMFLOAT4(0.0f, 0.5f, 0.5f, 1.0f),
        XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 30.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[6] = {
        XMFLOAT4(0.5f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(0.5f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 35.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[7] = {
        XMFLOAT4(1.0f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 40.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    for (int i = 0; i < MAX_MATERIALS; ++i)
        m_pMaterials->m_pReflections[i].m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);

	{
		MATERIAL& keyMat = m_pMaterials->m_pReflections[kItemBillboardKeyMaterialId];

		keyMat.m_xmf4Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		keyMat.m_xmf4Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		keyMat.m_xmf4Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		keyMat.m_xmf4Emissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		keyMat.m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);

		keyMat.m_xmf4DiffuseUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		keyMat.m_xmf4NormalUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		keyMat.m_xmf4EmissiveUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		keyMat.m_xmf4SpecularUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);

		keyMat.m_xmn4WrapModes0 = XMUINT4(0, 0, 0, 0);
		keyMat.m_xmn4WrapModes1 = XMUINT4(0, 0, 0, 0);
	}
	{
		MATERIAL& transparentMat =
			m_pMaterials->m_pReflections[kTransparentItemBillboardMaterialId];

		transparentMat.m_xmf4Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		transparentMat.m_xmf4Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

		transparentMat.m_xmf4Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		transparentMat.m_xmf4Emissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

		transparentMat.m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);

		transparentMat.m_xmf4DiffuseUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		transparentMat.m_xmf4NormalUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		transparentMat.m_xmf4EmissiveUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
		transparentMat.m_xmf4SpecularUVST = XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);

		transparentMat.m_xmn4WrapModes0 = XMUINT4(0, 0, 0, 0);
		transparentMat.m_xmn4WrapModes1 = XMUINT4(0, 0, 0, 0);
	}
}

void CGameScene::CreateShaderVariables(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
    UINT ncbElementBytes = ((sizeof(LIGHTS) + 255) & ~255);
    m_pd3dcbLights = ::CreateBufferResource(
        dev, cmd, nullptr,
        ncbElementBytes,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        nullptr);
    m_pd3dcbLights->Map(0, nullptr, (void**)&m_pcbMappedLights);

    UINT ncbMaterialBytes = ((sizeof(MATERIALS) + 255) & ~255);
    m_pd3dcbMaterials = ::CreateBufferResource(
        dev, cmd, nullptr,
        ncbMaterialBytes,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        nullptr);
    m_pd3dcbMaterials->Map(0, nullptr, (void**)&m_pcbMappedMaterials);

	m_depthFog.CreateConstantBuffer(dev, cmd);
}

void CGameScene::BuildStaticBatch(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	const std::shared_ptr<CStaticObjectsShader>& pStaticShader,
	UINT nRenderTargets,
	DXGI_FORMAT* rtvFormats,
	DXGI_FORMAT dsvFormat)
{
	auto* b = &m_staticBatch;
	if ( !b ) return;

	if ( b->capacity < 4 ) b->capacity = 4;
	const UINT cap = b->capacity;
	if ( cap == 0 ) return;

	pStaticShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		nRenderTargets,
		rtvFormats,
		dsvFormat
	);

	b->cbElementBytes = ( ( sizeof(CB_GAMEOBJECT_INFO) + 255 ) & ~255 );

	b->cbGameObjects = ::CreateBufferResource(
		dev, cmd, nullptr,
		b->cbElementBytes * cap,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	b->cbGameObjects->Map(0, nullptr, ( void** ) &b->mappedGameObjects);

	b->baseCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
	b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	m_pDescriptorHeap->CreateConstantBufferViews(
		dev,
		cap,
		b->cbGameObjects.Get(),
		b->cbElementBytes
	);

	m_treeAlphaClipObjects.clear();

	m_staticObjects.clear();
	m_staticObjects.reserve(cap);

	b->objectRefs.clear();
	b->objectRefs.reserve(cap);

#ifndef USING_NETWORK
	m_towerDoorPortals.clear();
	m_castleDoorPortals.clear();
#endif

	m_staticShadowCasterFlags.clear();
	m_staticShadowCasterFlags.reserve(cap);

	m_staticTreeObjectIndices.clear();
	m_staticTreeObjectIndices.reserve(cap);

	m_staticShadowOcclusionEntryIndices.clear();

	m_staticCollisionMegaGridMasks.clear();
	m_staticCollisionMegaGridMasks.reserve(cap);

	m_collisionMegaGridMaskByObject.clear();
	m_collisionMegaGridMaskByObject.reserve(cap + m_skinnedBatch.capacity);

	b->count = 0;
	ResetStaticWorldLodEntries();

	std::vector<size_t> exportedWorldStaticPlacementIndices;
	std::vector<CGameObject*> exportedWorldStaticObjects;

	exportedWorldStaticPlacementIndices.reserve(m_staticPlacementEntries.size());
	exportedWorldStaticObjects.reserve(m_staticPlacementEntries.size());

	auto MakeStaticContext = [ & ] (UINT objectIndex)
		{
			GameSceneObjectFactory::CreateContext ctx{};
			ctx.device = dev;
			ctx.cmd = cmd;
			ctx.mappedGameObjectCB =
				reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >( b->mappedGameObjects ) +
					objectIndex * b->cbElementBytes
				);
			ctx.cbvGpuHandle.ptr =
				b->baseCbvGpu.ptr + ( UINT64 ) objectIndex * b->cbvInc;
			return ctx;
		};

	// ------------------------------------------------------------------------
	// Static world objects from placement file
	// ------------------------------------------------------------------------
	for ( UINT k = 0; k < ( UINT ) m_staticPlacementEntries.size(); ++k )
	{
		if ( b->objectRefs.size() >= b->capacity ) break;

		const UINT i = ( UINT ) b->objectRefs.size();
		const StaticPlacementEntry& placement = m_staticPlacementEntries[k];

		const bool createWorldStaticCollider =
			ShouldCreateWorldStaticCollider(placement.assetName);
		const bool isStaticWorldLodTarget =
			IsStaticWorldLodSupportedAssetName(placement.assetName);
		const bool enableDistanceCull =
			ShouldUseStaticWorldDistanceCull(placement.assetName);

		AssetBuildDesc desc{};
		AssetType resolvedAssetType{};
		if ( !ResolveStaticAssetDesc(placement.assetName, desc, &resolvedAssetType) )
			continue;

		std::shared_ptr<CMesh> selectedMesh = nullptr;
		std::array<std::shared_ptr<CMesh>, 3> loadedLodMeshes = { nullptr, nullptr, nullptr };
		bool enableStaticWorldLod = false;

		if ( isStaticWorldLodTarget )
		{
			enableStaticWorldLod = true;

			for ( int lodLevel = 0; lodLevel < 3; ++lodLevel )
			{
				AssetBuildDesc lodDesc{};
				AssetType lodResolvedType{};
				if ( !ResolveStaticLodAssetDesc(
					placement.assetName,
					lodLevel,
					lodDesc,
					&lodResolvedType) )
				{
					enableStaticWorldLod = false;
					break;
				}

				BuiltAsset lodAsset = AssetManager::BuildAsset(
					dev, cmd,
					m_pMaterials.get(),
					lodDesc
				);

				loadedLodMeshes[( size_t ) lodLevel] = lodAsset.mesh;
				if ( !loadedLodMeshes[( size_t ) lodLevel] )
				{
					enableStaticWorldLod = false;
					break;
				}
			}

			if ( enableStaticWorldLod )
			{
				selectedMesh = loadedLodMeshes[0];
			}
			else
			{
				loadedLodMeshes = { nullptr, nullptr, nullptr };
			}
		}

		if ( !selectedMesh )
		{
			BuiltAsset baseAsset = AssetManager::BuildAsset(
				dev, cmd,
				m_pMaterials.get(),
				desc
			);

			selectedMesh = baseAsset.mesh;
			loadedLodMeshes[0] = selectedMesh;
		}

		if ( !selectedMesh )
			continue;

		GameSceneObjectFactory::StaticRenderableDesc createDesc{};
		createDesc.ctx = MakeStaticContext(i);
		createDesc.mesh = selectedMesh;
		createDesc.position = placement.pos;
		createDesc.yawDeg = placement.yawDeg;

		createDesc.addCollider = createWorldStaticCollider;
		createDesc.colliderType = EColliderType::OOBB;
		createDesc.colliderLayer = kCollisionLayerWorldStatic;
		createDesc.colliderMask = CollisionBit(kCollisionLayerPlayer);

		const bool logCastleVillageWallColliderBuild =
			kEnableCastleVillageWallColliderBuildLog &&
			( placement.assetName == "Castle" || placement.assetName == "VillageWall" );

		createDesc.debugColliderBuildLog = logCastleVillageWallColliderBuild;
		createDesc.debugColliderAssetName = placement.assetName;
		createDesc.debugColliderObjectName = placement.objectName;

		if ( createWorldStaticCollider )
		{
			const auto authoredIt = mSceneCubeBoxColliderTable.find(placement.assetName);
			if ( authoredIt != mSceneCubeBoxColliderTable.end() )
			{
				createDesc.authoredStaticSubMeshOOBBs = &authoredIt->second;

				if ( logCastleVillageWallColliderBuild )
				{
					size_t authoredBoxTotal = 0;
					for ( const auto& kv : authoredIt->second )
						authoredBoxTotal += kv.second.size();

					char buf[512];
					sprintf_s(
						buf,
						"[ColliderBuild][SCENE_AUTHORED_TABLE_FOUND] asset=\"%s\" object=\"%s\" pathGroupCount=%zu authoredBoxTotal=%zu\n",
						placement.assetName.c_str(),
						placement.objectName.c_str(),
						authoredIt->second.size(),
						authoredBoxTotal
					);
					OutputDebugStringA(buf);
				}
			}
			else if ( logCastleVillageWallColliderBuild )
			{
				char buf[512];
				sprintf_s(
					buf,
					"[ColliderBuild][SCENE_AUTHORED_TABLE_MISSING] asset=\"%s\" object=\"%s\"\n",
					placement.assetName.c_str(),
					placement.objectName.c_str()
				);
				OutputDebugStringA(buf);
			}
		}

		auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
		if ( !obj )
			continue;

		CGameObject* raw = obj.get();

#ifndef USING_NETWORK
		if ( placement.assetName == "Tower" )
		{
			RegisterTowerDoorPortal(raw);
		}
		else if ( placement.assetName == "Castle" )
		{
			RegisterCastleDoorPortal(raw);
		}
#endif

		const bool isTreeObject = ( resolvedAssetType == AssetType::Tree );
		const bool castsShadow = ShouldStaticPlacementCastShadow(placement.assetName);

		if ( isTreeObject )
		{
			m_treeAlphaClipObjects.insert(raw);
			m_staticTreeObjectIndices.push_back(i);
		}

		if ( enableDistanceCull || isStaticWorldLodTarget )
		{
			StaticWorldLodEntry lodEntry{};
			lodEntry.object = raw;
			lodEntry.staticBatchObjectIndex = i;
			lodEntry.assetName = placement.assetName;
			lodEntry.lodReferencePosition = placement.pos;

			lodEntry.lodEnabled = ( isStaticWorldLodTarget && enableStaticWorldLod );
			lodEntry.useTreeShader = ( resolvedAssetType == AssetType::Tree );
			lodEntry.currentLod = 0;
			lodEntry.lodMeshes = loadedLodMeshes;

			lodEntry.distanceCullEnabled = enableDistanceCull;
			lodEntry.distanceCulled = false;

			if ( placement.assetName == "VillageWall" )
			{
				lodEntry.lodDistance01 = 250.0f;
				lodEntry.lodDistance12 = 600.0f;
				lodEntry.cullDistance = 900.0f;
			}
			else if (
				placement.assetName == "Building1" ||
				placement.assetName == "Building2" ||
				placement.assetName == "Building3" ||
				placement.assetName == "Building4" ||
				placement.assetName == "Building5" ||
				placement.assetName == "Building6" ||
				placement.assetName == "Building7" ||
				placement.assetName == "Building8" ||
				placement.assetName == "Building9" ||
				placement.assetName == "Tower" )
			{
				lodEntry.lodDistance01 = 50.0f;
				lodEntry.lodDistance12 = 300.0f;
				lodEntry.cullDistance = 400.0f;
			}
			else if (
				placement.assetName == "Tree1" ||
				placement.assetName == "Tree2" ||
				placement.assetName == "Tree3" ||
				placement.assetName == "Tree4" ||
				placement.assetName == "Tree5" ||
				placement.assetName == "Tree6" )
			{
				lodEntry.lodDistance01 = 40.0f;
				lodEntry.lodDistance12 = 120.0f;
				lodEntry.cullDistance = 500.0f;
			}
			else
			{
				lodEntry.cullDistance = 400.0f;
			}

			if ( !lodEntry.lodMeshes[0] )
				lodEntry.lodMeshes[0] = selectedMesh;

			m_staticWorldLodEntries.push_back(std::move(lodEntry));
		}

		if ( createWorldStaticCollider )
		{
			auto* collider = raw->GetComponent<CColliderComponent>();
			if ( collider && collider->GetType() == EColliderType::OOBB )
			{
				exportedWorldStaticPlacementIndices.push_back(static_cast< size_t >( k ));
				exportedWorldStaticObjects.push_back(raw);
			}
		}

		RegisterStaticPlacementToGrid(placement, raw);

		const uint16_t collisionMegaGridMask =
			createWorldStaticCollider ? ComputeStaticObjectMegaGridMask(raw) : 0;

		if ( collisionMegaGridMask != 0 )
			m_collisionMegaGridMaskByObject[raw] = collisionMegaGridMask;

		m_staticObjects.push_back(std::move(obj));
		b->objectRefs.push_back(raw);
		m_staticShadowCasterFlags.push_back(castsShadow ? 1 : 0);
		m_staticCollisionMegaGridMasks.push_back(collisionMegaGridMask);
		b->count = ( UINT ) b->objectRefs.size();
	}

#ifndef USING_NETWORK
	if ( kEnableStaticWorldLocalOOBBReportExport )
	{
		if ( !ExportStaticWorldLocalOOBBReport(
			kStaticWorldLocalOOBBReportPath,
			exportedWorldStaticPlacementIndices,
			exportedWorldStaticObjects) )
		{
			OutputDebugStringA("[StaticWorldLocalOOBBReport] export failed\n");
		}
		else
		{
			OutputDebugStringA("[StaticWorldLocalOOBBReport] export complete\n");
		}
	}
#endif

	// ------------------------------------------------------------------------
	// Arrow pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc arrowDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::Arrow, arrowDesc);

		BuiltAsset arrowAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			arrowDesc
		);

		m_arrowRefs.clear();
		m_arrowRefs.reserve(kArrowPoolSize);

		for ( UINT k = 0; k < kArrowPoolSize; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = arrowAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addCollider = true;
			createDesc.colliderType = EColliderType::OOBB;
			createDesc.colliderLayer = kCollisionLayerPlayerWeapon;
			createDesc.colliderMask = CollisionBit(kCollisionLayerMonster);
			createDesc.colliderEnabled = false;

			createDesc.addArrowComponent = true;

			createDesc.addAttackPower = true;
			createDesc.attackPower = 0;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(0);
			m_staticCollisionMegaGridMasks.push_back(0);
			b->count = ( UINT ) b->objectRefs.size();

			m_arrowRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// Bullet pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc bulletDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::Bullet, bulletDesc);

		BuiltAsset bulletAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			bulletDesc
		);

		m_bulletRefs.clear();
		m_bulletRefs.reserve(kBulletPoolSize);

		for ( UINT k = 0; k < kBulletPoolSize; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = bulletAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addCollider = true;
			createDesc.colliderType = EColliderType::BSphere;
			createDesc.colliderLayer = kCollisionLayerPlayerWeapon;
			createDesc.colliderMask = CollisionBit(kCollisionLayerMonster);
			createDesc.colliderEnabled = false;

			createDesc.addBulletComponent = true;

			createDesc.addAttackPower = true;
			createDesc.attackPower = 0;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(0);
			m_staticCollisionMegaGridMasks.push_back(0);
			b->count = ( UINT ) b->objectRefs.size();

			m_bulletRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// Helmet pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc helmetDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::Helmet, helmetDesc);

		BuiltAsset helmetAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			helmetDesc
		);

		m_helmetRefs.clear();
		m_helmetRefs.reserve(m_helmetCount);

		for ( UINT k = 0; k < m_helmetCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = helmetAsset.mesh;
			createDesc.spawnHidden = true;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(0);
			m_staticCollisionMegaGridMasks.push_back(0);
			b->count = ( UINT ) b->objectRefs.size();

			m_helmetRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// PlayerSword pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc swordDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerSword, swordDesc);

		BuiltAsset swordAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			swordDesc
		);

		m_PlayerSwordRefs.clear();
		m_PlayerSwordRefs.reserve(m_PlayerSwordCount);

		for ( UINT k = 0; k < m_PlayerSwordCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = swordAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addCollider = true;
			createDesc.colliderType = EColliderType::OOBB;
			createDesc.colliderLayer = kCollisionLayerPlayerWeapon;
			createDesc.colliderMask = CollisionBit(kCollisionLayerMonster);
			createDesc.colliderEnabled = false;

			createDesc.addPlayerWeaponHitbox = true;

			createDesc.addAttackPower = true;
			createDesc.attackPower = kAttackPowerPlayerSword;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(1);
			m_staticCollisionMegaGridMasks.push_back(0);
			b->count = ( UINT ) b->objectRefs.size();

			m_PlayerSwordRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// PlayerAxe pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc axeDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerAxe, axeDesc);

		BuiltAsset axeAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			axeDesc
		);

		m_PlayerAxeRefs.clear();
		m_PlayerAxeRefs.reserve(m_PlayerAxeCount);

		for ( UINT k = 0; k < m_PlayerAxeCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = axeAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addCollider = true;
			createDesc.colliderType = EColliderType::OOBB;
			createDesc.colliderLayer = kCollisionLayerPlayerWeapon;
			createDesc.colliderMask = CollisionBit(kCollisionLayerMonster);
			createDesc.colliderEnabled = false;

			createDesc.addPlayerWeaponHitbox = true;

			createDesc.addAttackPower = true;
			createDesc.attackPower = kAttackPowerPlayerAxe;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(1);
			m_staticCollisionMegaGridMasks.push_back(0);
			b->count = ( UINT ) b->objectRefs.size();

			m_PlayerAxeRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// PlayerGun pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc gunDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerGun, gunDesc);

		BuiltAsset gunAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			gunDesc
		);

		m_PlayerGunRefs.clear();
		m_PlayerGunRefs.reserve(m_PlayerGunCount);

		for ( UINT k = 0; k < m_PlayerGunCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = gunAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addCollider = true;
			createDesc.colliderType = EColliderType::OOBB;
			createDesc.configureColliderFiltering = false;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(1);
			m_staticCollisionMegaGridMasks.push_back(0);
			b->count = ( UINT ) b->objectRefs.size();

			m_PlayerGunRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// EnemySword pool
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc enemySwordDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::EnemySword, enemySwordDesc);

		BuiltAsset swordAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			enemySwordDesc
		);

		m_EnemySwordRefs.clear();
		m_EnemySwordRefs.reserve(m_swordManCount);

		for ( UINT k = 0; k < m_swordManCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::StaticRenderableDesc createDesc{};
			createDesc.ctx = MakeStaticContext(i);
			createDesc.mesh = swordAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addCollider = true;
			createDesc.colliderType = EColliderType::OOBB;
			createDesc.colliderLayer = kCollisionLayerMonsterWeapon;
			createDesc.colliderMask = CollisionBit(kCollisionLayerPlayer);
			createDesc.colliderEnabled = false;

			createDesc.addMonsterWeaponHitbox = true;
			createDesc.addAttackPower = true;
			createDesc.attackPower = kAttackPowerEnemySword;

			auto obj = GameSceneObjectFactory::CreateStaticRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			m_staticShadowCasterFlags.push_back(1);
			m_staticCollisionMegaGridMasks.push_back(0);
			b->count = ( UINT ) b->objectRefs.size();

			m_EnemySwordRefs.push_back(raw);
		}
	}

	BuildStaticOcclusionEntries();

	m_staticShadowOcclusionEntryIndices.assign(m_staticBatch.objectRefs.size(), -1);

	for ( UINT entryIndex = 0; entryIndex < ( UINT ) m_staticOcclusionEntries.size(); ++entryIndex )
	{
		const StaticOcclusionEntry& entry = m_staticOcclusionEntries[entryIndex];

		if ( entry.staticBatchObjectIndex >= ( UINT ) m_staticShadowOcclusionEntryIndices.size() )
			continue;

		m_staticShadowOcclusionEntryIndices[entry.staticBatchObjectIndex] =
			static_cast< int >(entryIndex);
	}

	BuildStaticOcclusionUnitBoxMesh(dev, cmd);
	BuildStaticOcclusionGpuResources(dev);
	BuildStaticInstanceGroups();
	BuildStaticRenderObjectCache();

	m_staticTreeGridCullFlags.assign(m_staticBatch.objectRefs.size(), 0);

	if ( m_pd3dStaticInstanceBuffer )
	{
		if ( m_pMappedStaticInstanceBuffer )
		{
			m_pd3dStaticInstanceBuffer->Unmap(0, NULL);
			m_pMappedStaticInstanceBuffer = nullptr;
		}
		m_pd3dStaticInstanceBuffer.Reset();
	}

	if ( m_staticInstanceBufferCapacity > 0 )
	{
		// pass 0: scene
		// pass 1: shadow
		const UINT kStaticInstancePassCount = 2;

		const UINT instanceBufferBytes =
			sizeof(StaticInstanceVertex) *
			m_staticInstanceBufferCapacity *
			kStaticInstancePassCount;

		m_pd3dStaticInstanceBuffer = ::CreateBufferResource(
			dev, cmd, nullptr,
			instanceBufferBytes,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr
		);

		m_pd3dStaticInstanceBuffer->Map(
			0, nullptr, ( void** ) &m_pMappedStaticInstanceBuffer);
	}
}

bool CGameScene::IsStaticTreeObject(const CGameObject* obj) const
{
	if ( !obj )
		return false;

	return m_treeAlphaClipObjects.find(obj) != m_treeAlphaClipObjects.end();
}

void CGameScene::UpdateStaticTreeGridCullSelection(CCamera* camera)
{
	const size_t objectCount = m_staticBatch.objectRefs.size();

	if ( m_staticTreeGridCullFlags.size() != objectCount )
		m_staticTreeGridCullFlags.assign(objectCount, 0);
	else
		std::fill(m_staticTreeGridCullFlags.begin(), m_staticTreeGridCullFlags.end(), 0);

	if ( !m_bStaticTreeGridCullingEnabled )
		return;

	if ( !camera )
		return;

	const bool shouldCullTrees = ShouldCullTreesByVillageGrid(camera);

	if ( !shouldCullTrees )
		return;

	for ( UINT objectIndex : m_staticTreeObjectIndices )
	{
		if ( objectIndex >= ( UINT ) m_staticTreeGridCullFlags.size() )
			continue;

		m_staticTreeGridCullFlags[objectIndex] = 1;
	}
}

void CGameScene::BuildStaticInstanceGroups()
{
	m_staticInstanceGroups.clear();

	for ( UINT objectIndex = 0; objectIndex < ( UINT ) m_staticBatch.objectRefs.size(); ++objectIndex )
	{
		CGameObject* obj = m_staticBatch.objectRefs[objectIndex];
		const bool useTreeShader =
			( m_treeAlphaClipObjects.find(obj) != m_treeAlphaClipObjects.end() );
		if ( !obj ) continue;

		const int meshCount = obj->GetMeshCount();
		for ( int meshIndex = 0; meshIndex < meshCount; ++meshIndex )
		{
			std::shared_ptr<CMesh> mesh = obj->GetMeshShared(meshIndex);
			if ( !mesh ) continue;

			for ( UINT subMeshIndex = 0; subMeshIndex < ( UINT ) mesh->m_SubMeshes.size(); ++subMeshIndex )
			{
				StaticInstanceGroup* targetGroup = nullptr;

				for ( StaticInstanceGroup& group : m_staticInstanceGroups )
				{
					if ( group.mesh.get() == mesh.get() &&
						group.subMeshIndex == subMeshIndex &&
						group.useTreeShader == useTreeShader )
					{
						targetGroup = &group;
						break;
					}
				}

				if ( !targetGroup )
				{
					StaticInstanceGroup newGroup{};
					newGroup.mesh = mesh;
					newGroup.subMeshIndex = subMeshIndex;
					newGroup.useTreeShader = useTreeShader;
					m_staticInstanceGroups.push_back(std::move(newGroup));
					targetGroup = &m_staticInstanceGroups.back();
				}

				targetGroup->objectIndices.push_back(objectIndex);
			}
		}
	}

	std::sort(
	m_staticInstanceGroups.begin(),
	m_staticInstanceGroups.end(),
	[ ] (const StaticInstanceGroup& a, const StaticInstanceGroup& b)
	{
		if ( a.useTreeShader != b.useTreeShader )
			return a.useTreeShader < b.useTreeShader; // opaque 먼저, tree alpha-clip 나중

		if ( a.mesh.get() != b.mesh.get() )
			return a.mesh.get() < b.mesh.get();

		return a.subMeshIndex < b.subMeshIndex;
	}
	);

	UINT runningStart = 0;
	for ( StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		group.instanceBufferStart = runningStart;
		runningStart += ( UINT ) group.objectIndices.size();
	}

	m_staticInstanceBufferCapacity = runningStart;

	for ( StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		group.visibleSceneObjectIndices.clear();
		group.visibleShadowObjectIndices.clear();

		group.visibleSceneObjectIndices.reserve(group.objectIndices.size());
		group.visibleShadowObjectIndices.reserve(group.objectIndices.size());
	}
}

void CGameScene::BuildSkinnedInstanceGroups()
{
	m_skinnedInstanceGroups.clear();

	for ( UINT objectIndex = 0; objectIndex < ( UINT ) m_skinnedBatch.objectRefs.size(); ++objectIndex )
	{
		CGameObject* obj = m_skinnedBatch.objectRefs[objectIndex];
		if ( !obj ) continue;

		const bool useAlphaClipShader =
			( m_skinnedAlphaClipObjects.find(obj) != m_skinnedAlphaClipObjects.end() );

		const int meshCount = obj->GetMeshCount();
		for ( int meshIndex = 0; meshIndex < meshCount; ++meshIndex )
		{
			std::shared_ptr<CMesh> mesh = obj->GetMeshShared(meshIndex);
			if ( !mesh ) continue;

			std::string geometryKey = mesh->GetSourceMeshPath();
			if ( geometryKey.empty() )
			{
				char buf[64];
				sprintf_s(buf, "meshptr_%p", mesh.get());
				geometryKey = buf;
			}

			for ( UINT subMeshIndex = 0; subMeshIndex < ( UINT ) mesh->m_SubMeshes.size(); ++subMeshIndex )
			{
				SkinnedInstanceGroup* targetGroup = nullptr;

				for ( SkinnedInstanceGroup& group : m_skinnedInstanceGroups )
				{
					if ( group.geometryKey == geometryKey &&
						group.meshIndex == ( UINT ) meshIndex &&
						group.subMeshIndex == subMeshIndex &&
						group.useAlphaClipShader == useAlphaClipShader )
					{
						targetGroup = &group;
						break;
					}
				}

				if ( !targetGroup )
				{
					SkinnedInstanceGroup newGroup{};
					newGroup.geometryKey = geometryKey;
					newGroup.mesh = mesh;
					newGroup.subMeshIndex = subMeshIndex;
					newGroup.meshIndex = ( UINT ) meshIndex;
					newGroup.useAlphaClipShader = useAlphaClipShader;
					m_skinnedInstanceGroups.push_back(std::move(newGroup));
					targetGroup = &m_skinnedInstanceGroups.back();
				}

				targetGroup->objectIndices.push_back(objectIndex);
			}
		}
	}

	std::sort(
	m_skinnedInstanceGroups.begin(),
	m_skinnedInstanceGroups.end(),
	[ ] (const SkinnedInstanceGroup& a, const SkinnedInstanceGroup& b)
	{
		if ( a.useAlphaClipShader != b.useAlphaClipShader )
			return a.useAlphaClipShader < b.useAlphaClipShader; // opaque 먼저, alpha-clip 나중

		if ( a.geometryKey != b.geometryKey )
			return a.geometryKey < b.geometryKey;

		if ( a.meshIndex != b.meshIndex )
			return a.meshIndex < b.meshIndex;

		return a.subMeshIndex < b.subMeshIndex;
	}
	);

	UINT runningStart = 0;
	for ( SkinnedInstanceGroup& group : m_skinnedInstanceGroups )
	{
		group.instanceBufferStart = runningStart;
		runningStart += ( UINT ) group.objectIndices.size();
	}

	m_skinnedInstanceBufferCapacity = runningStart;
}

bool CGameScene::IsDynamicStaticRenderObject(const CGameObject* obj) const
{
	if ( !obj )
		return false;

	// static batch에 들어가지만 transform이 바뀔 수 있는 오브젝트들.
	// PlayerBow / EnemyBow는 현재 skinned batch 쪽이므로 여기서 제외한다.
	if ( ContainsGameObjectPtr(m_helmetRefs, obj) )
		return true;

	if ( ContainsGameObjectPtr(m_PlayerSwordRefs, obj) )
		return true;

	if ( ContainsGameObjectPtr(m_PlayerAxeRefs, obj) )
		return true;

	if ( ContainsGameObjectPtr(m_PlayerGunRefs, obj) )
		return true;

	if ( ContainsGameObjectPtr(m_EnemySwordRefs, obj) )
		return true;

	if ( ContainsGameObjectPtr(m_arrowRefs, obj) )
		return true;

	if ( ContainsGameObjectPtr(m_bulletRefs, obj) )
		return true;

	return false;
}

void CGameScene::BuildStaticRenderObjectCache()
{
	m_staticRenderObjectCache.clear();
	m_staticRenderObjectCache.resize(m_staticBatch.objectRefs.size());

	for ( UINT i = 0; i < static_cast< UINT >(m_staticBatch.objectRefs.size()); ++i )
	{
		CGameObject* obj = m_staticBatch.objectRefs[i];

		StaticRenderObjectCache& cache = m_staticRenderObjectCache[i];
		cache.object = obj;

		if ( !obj )
			continue;

		cache.renderer = obj->GetComponent<CStaticMeshRendererComponent>();
		cache.dynamicWorldMatrix = IsDynamicStaticRenderObject(obj);

		const XMFLOAT4X4& W = obj->GetWorldMatrix();

		StoreStaticWorldRows(
			cache.world0,
			cache.world1,
			cache.world2,
			cache.world3,
			W
		);
	}
}

bool CGameScene::WriteStaticInstanceVertexFromCache(
	StaticInstanceVertex& dst,
	UINT objectIndex) const
{
	if ( objectIndex >= static_cast< UINT >( m_staticRenderObjectCache.size() ) )
		return false;

	const StaticRenderObjectCache& cache = m_staticRenderObjectCache[objectIndex];

	if ( !cache.object )
		return false;

	if ( !cache.renderer )
		return false;

	if ( !cache.renderer->IsEnabled() )
		return false;

	if ( cache.dynamicWorldMatrix )
	{
		const XMFLOAT4X4& W = cache.object->GetWorldMatrix();

		StoreStaticWorldRows(
			dst.world0,
			dst.world1,
			dst.world2,
			dst.world3,
			W
		);
	}
	else
	{
		dst.world0 = cache.world0;
		dst.world1 = cache.world1;
		dst.world2 = cache.world2;
		dst.world3 = cache.world3;
	}

	dst.objectId = objectIndex;

	return true;
}

void CGameScene::BuildStaticVisibleListsForFrame(CCamera* camera)
{
	for ( StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		group.visibleSceneObjectIndices.clear();

		for ( UINT objectIndex : group.objectIndices )
		{
			if ( objectIndex >= static_cast< UINT >( m_staticBatch.objectRefs.size() ) )
				continue;

			if ( objectIndex >= static_cast< UINT >( m_staticRenderObjectCache.size() ) )
				continue;

			const StaticRenderObjectCache& cache =
				m_staticRenderObjectCache[objectIndex];

			if ( !cache.object )
				continue;

			if ( !cache.renderer )
				continue;

			if ( !cache.renderer->IsEnabled() )
				continue;

			if ( objectIndex < static_cast< UINT >(m_staticDistanceCullFlags.size()) &&
				 m_staticDistanceCullFlags[objectIndex] != 0 )
			{
				continue;
			}

			if ( objectIndex < static_cast< UINT >(m_staticTreeGridCullFlags.size()) &&
				 m_staticTreeGridCullFlags[objectIndex] != 0 )
			{
				continue;
			}

			const bool cameraVisible =
				( camera == nullptr ) || cache.object->IsVisible(camera);

			if ( !cameraVisible )
				continue;

			if ( objectIndex < static_cast< UINT >(m_staticOcclusionCullFlags.size()) &&
				 m_staticOcclusionCullFlags[objectIndex] != 0 )
			{
				continue;
			}

			group.visibleSceneObjectIndices.push_back(objectIndex);
		}
	}
}

void CGameScene::BuildStaticShadowVisibleListsForFrame()
{
	for ( StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		group.visibleShadowObjectIndices.clear();

		for ( UINT objectIndex : group.objectIndices )
		{
			if ( objectIndex >= static_cast< UINT >( m_staticBatch.objectRefs.size() ) )
				continue;

			if ( objectIndex >= static_cast< UINT >( m_staticRenderObjectCache.size() ) )
				continue;

			const StaticRenderObjectCache& cache =
				m_staticRenderObjectCache[objectIndex];

			if ( !cache.object )
				continue;

			if ( !cache.renderer )
				continue;

			if ( !cache.renderer->IsEnabled() )
				continue;

			if ( objectIndex < static_cast< UINT >(m_staticShadowCasterFlags.size()) &&
				 m_staticShadowCasterFlags[objectIndex] == 0 )
			{
				continue;
			}

			if ( objectIndex < static_cast< UINT >(m_staticDistanceCullFlags.size()) &&
				 m_staticDistanceCullFlags[objectIndex] != 0 )
			{
				continue;
			}

			if ( objectIndex < static_cast< UINT >(m_staticTreeGridCullFlags.size()) &&
				 m_staticTreeGridCullFlags[objectIndex] != 0 )
			{
				continue;
			}

			if ( !IsStaticObjectInsideShadowBox(objectIndex) )
				continue;

			group.visibleShadowObjectIndices.push_back(objectIndex);
		}
	}
}

void CGameScene::RenderStaticInstanceGroups(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::RenderStaticInstanceGroups");

	if ( !cmd ) return;
	if ( !m_pd3dStaticInstanceBuffer ) return;
	if ( !m_pMappedStaticInstanceBuffer ) return;

	bool lastUseTreeShader = false;
	bool hasBoundAnyShader = false;

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for ( const StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		if ( !group.mesh ) continue;
		if ( group.subMeshIndex >= group.mesh->m_SubMeshes.size() ) continue;

		const SubMesh& sm = group.mesh->m_SubMeshes[group.subMeshIndex];
		if ( sm.indices.empty() ) continue;

		const UINT maxInstanceCount =
			static_cast< UINT >( group.visibleSceneObjectIndices.size() );

		if ( maxInstanceCount == 0 )
			continue;

		const UINT instanceBase = group.instanceBufferStart;

		if ( ( instanceBase + maxInstanceCount ) > m_staticInstanceBufferCapacity )
			continue;

		UINT visibleInstanceCount = 0;

		for ( UINT i = 0; i < maxInstanceCount; ++i )
		{
			const UINT objectIndex = group.visibleSceneObjectIndices[i];

			StaticInstanceVertex& dst =
				m_pMappedStaticInstanceBuffer[instanceBase + visibleInstanceCount];

			if ( !WriteStaticInstanceVertexFromCache(dst, objectIndex) )
				continue;

			++visibleInstanceCount;
		}

		if ( visibleInstanceCount == 0 )
			continue;

		D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
		vbViews[0] = sm.vbView;
		vbViews[1].BufferLocation =
			m_pd3dStaticInstanceBuffer->GetGPUVirtualAddress() +
			( UINT64 ) ( sizeof(StaticInstanceVertex) * instanceBase );
		vbViews[1].SizeInBytes = sizeof(StaticInstanceVertex) * visibleInstanceCount;
		vbViews[1].StrideInBytes = sizeof(StaticInstanceVertex);

		if ( !hasBoundAnyShader || lastUseTreeShader != group.useTreeShader )
		{
			if ( group.useTreeShader )
			{
				if ( m_treeStaticShader )
					m_treeStaticShader->Render(cmd, camera, &m_staticBatch);
				else if ( m_staticBatch.shader )
					m_staticBatch.shader->Render(cmd, camera, &m_staticBatch);
			}
			else
			{
				if ( m_staticBatch.shader )
					m_staticBatch.shader->Render(cmd, camera, &m_staticBatch);
			}

			lastUseTreeShader = group.useTreeShader;
			hasBoundAnyShader = true;
		}

		const UINT mid = ( sm.materialId == 0xFFFFFFFFu ) ? 0u : sm.materialId;
		cmd->SetGraphicsRoot32BitConstant(ROOT_PARAMETER_MATERIAL_ID, mid, 0);

		if ( sm.material && sm.material->NeedsLegacyBinding() )
			sm.material->UpdateShaderVariables(cmd);

		cmd->IASetVertexBuffers(0, 2, vbViews);
		cmd->IASetIndexBuffer(&sm.ibView);

		cmd->DrawIndexedInstanced(( UINT ) sm.indices.size(), visibleInstanceCount, 0, 0, 0);
	}
}

void CGameScene::RenderSkinnedInstanceGroups(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !m_pd3dSkinnedInstanceBuffer ) return;
	if ( !m_pMappedSkinnedInstanceBuffer ) return;
	if ( !m_pd3dSkinnedBonePaletteBuffer ) return;
	if ( !m_pMappedSkinnedBonePaletteBuffer ) return;

	cmd->SetGraphicsRootShaderResourceView(
		ROOT_PARAMETER_BONE_PALETTE,
		m_pd3dSkinnedBonePaletteBuffer->GetGPUVirtualAddress()
	);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for ( const SkinnedInstanceGroup& group : m_skinnedInstanceGroups )
	{
		if ( !group.mesh ) continue;
		if ( group.subMeshIndex >= group.mesh->m_SubMeshes.size() ) continue;

		const SubMesh& repSm = group.mesh->m_SubMeshes[group.subMeshIndex];
		if ( repSm.indices.empty() ) continue;

		const UINT maxInstanceCount = ( UINT ) group.objectIndices.size();
		if ( maxInstanceCount == 0 ) continue;

		const UINT instanceBase = group.instanceBufferStart;
		if ( ( instanceBase + maxInstanceCount ) > m_skinnedInstanceBufferCapacity ) continue;

		UINT visibleInstanceCount = 0;

		for ( UINT i = 0; i < maxInstanceCount; ++i )
		{
			const UINT objectIndex = group.objectIndices[i];
			if ( objectIndex >= ( UINT ) m_skinnedBatch.objectRefs.size() ) continue;

			if ( objectIndex < ( UINT ) m_skinnedDistanceCullFlags.size() )
			{
				if ( m_skinnedDistanceCullFlags[objectIndex] != 0 )
					continue;
			}

			if ( objectIndex < ( UINT ) m_skinnedOcclusionCullFlags.size() )
			{
				if ( m_skinnedOcclusionCullFlags[objectIndex] != 0 )
					continue;
			}

			CGameObject* obj = m_skinnedBatch.objectRefs[objectIndex];
			if ( !obj ) continue;
			if ( !obj->IsVisible(camera) ) continue;

			auto* renderer = obj->GetComponent<CSkinnedMeshRendererComponent>();
			if ( !renderer ) continue;
			if ( !renderer->IsEnabled() ) continue;

			auto* skin = obj->GetComponent<CSkinningComponent>();
			if ( !skin ) continue;
			if ( !skin->IsSkinned() ) continue;

			std::shared_ptr<CMesh> objMesh = obj->GetMeshShared(( int ) group.meshIndex); 
			if ( !objMesh ) continue;
			if ( group.subMeshIndex >= objMesh->m_SubMeshes.size() ) continue;

			const SubMesh& objSm = objMesh->m_SubMeshes[group.subMeshIndex];

			SkinnedInstanceVertex& dst =
				m_pMappedSkinnedInstanceBuffer[instanceBase + visibleInstanceCount];

			const XMFLOAT4X4& W = obj->GetWorldMatrix();

			dst.world0 = XMFLOAT4(W._11, W._12, W._13, W._14);
			dst.world1 = XMFLOAT4(W._21, W._22, W._23, W._24);
			dst.world2 = XMFLOAT4(W._31, W._32, W._33, W._34);
			dst.world3 = XMFLOAT4(W._41, W._42, W._43, W._44);

			dst.materialId = ( objSm.materialId == 0xFFFFFFFFu ) ? 0u : objSm.materialId;
			dst.bonePaletteBase = objectIndex * m_skinnedBonePaletteStride;

			const XMFLOAT4X4* srcBoneMats = skin->GetMappedBoneMatrices();
			const UINT boneCount = ( UINT ) skin->GetBoneCount();

			if ( srcBoneMats && boneCount > 0 )
			{
				memcpy(
					m_pMappedSkinnedBonePaletteBuffer + dst.bonePaletteBase,
					srcBoneMats,
					sizeof(XMFLOAT4X4) * boneCount
				);
			}

			++visibleInstanceCount;
		}

		if ( visibleInstanceCount == 0 ) continue;

		D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
		vbViews[0] = repSm.vbView;
		vbViews[1].BufferLocation =
			m_pd3dSkinnedInstanceBuffer->GetGPUVirtualAddress() +
			( UINT64 ) ( sizeof(SkinnedInstanceVertex) * instanceBase );
		vbViews[1].SizeInBytes = sizeof(SkinnedInstanceVertex) * visibleInstanceCount;
		vbViews[1].StrideInBytes = sizeof(SkinnedInstanceVertex);

		cmd->IASetVertexBuffers(0, 2, vbViews);
		cmd->IASetIndexBuffer(&repSm.ibView);

		cmd->DrawIndexedInstanced(( UINT ) repSm.indices.size(), visibleInstanceCount, 0, 0, 0);
	}
}

void CGameScene::RenderStaticInstanceGroupsToShadowMap(ID3D12GraphicsCommandList* cmd)
{
	PROFILE_RENDER_SCOPE("GameScene::RenderStaticInstanceGroupsToShadowMap");

	if ( !cmd ) return;
	if ( !m_pd3dStaticInstanceBuffer ) return;
	if ( !m_pMappedStaticInstanceBuffer ) return;
	if ( !m_shadowStaticShader ) return;
	if ( !m_shadowAlphaClipStaticShader ) return;

	bool lastUseAlphaClipShader = false;
	bool hasBoundAnyShader = false;

	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	for ( const StaticInstanceGroup& group : m_staticInstanceGroups )
	{
		if ( !group.mesh ) continue;
		if ( group.subMeshIndex >= group.mesh->m_SubMeshes.size() ) continue;

		const SubMesh& sm = group.mesh->m_SubMeshes[group.subMeshIndex];
		if ( sm.indices.empty() ) continue;

		const UINT maxInstanceCount =
			static_cast< UINT >( group.visibleShadowObjectIndices.size() );

		if ( maxInstanceCount == 0 )
			continue;

		// pass 1: shadow
		const UINT instanceBase =
			m_staticInstanceBufferCapacity + group.instanceBufferStart;

		const UINT totalStaticInstanceCapacity =
			m_staticInstanceBufferCapacity * 2;

		if ( ( instanceBase + maxInstanceCount ) > totalStaticInstanceCapacity )
			continue;

		UINT visibleInstanceCount = 0;

		for ( UINT i = 0; i < maxInstanceCount; ++i )
		{
			const UINT objectIndex = group.visibleShadowObjectIndices[i];

			StaticInstanceVertex& dst =
				m_pMappedStaticInstanceBuffer[instanceBase + visibleInstanceCount];

			if ( !WriteStaticInstanceVertexFromCache(dst, objectIndex) )
				continue;

			++visibleInstanceCount;
		}

		if ( visibleInstanceCount == 0 )
			continue;

		if ( !hasBoundAnyShader || lastUseAlphaClipShader != group.useTreeShader )
		{
			if ( group.useTreeShader )
				m_shadowAlphaClipStaticShader->Render(cmd, nullptr, &m_staticBatch);
			else
				m_shadowStaticShader->Render(cmd, nullptr, &m_staticBatch);

			lastUseAlphaClipShader = group.useTreeShader;
			hasBoundAnyShader = true;
		}

		D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
		vbViews[0] = sm.vbView;
		vbViews[1].BufferLocation =
			m_pd3dStaticInstanceBuffer->GetGPUVirtualAddress() +
			( UINT64 ) ( sizeof(StaticInstanceVertex) * instanceBase );
		vbViews[1].SizeInBytes = sizeof(StaticInstanceVertex) * visibleInstanceCount;
		vbViews[1].StrideInBytes = sizeof(StaticInstanceVertex);

		// opaque shadow는 PS가 없으므로 material binding 불필요.
		// alpha-clip tree shadow만 material/texture 정보가 필요하다.
		if ( group.useTreeShader )
		{
			const UINT mid = ( sm.materialId == 0xFFFFFFFFu ) ? 0u : sm.materialId;
			cmd->SetGraphicsRoot32BitConstant(ROOT_PARAMETER_MATERIAL_ID, mid, 0);

			if ( sm.material && sm.material->NeedsLegacyBinding() )
				sm.material->UpdateShaderVariables(cmd);
		}

		cmd->IASetVertexBuffers(0, 2, vbViews);
		cmd->IASetIndexBuffer(&sm.ibView);

		cmd->DrawIndexedInstanced(( UINT ) sm.indices.size(), visibleInstanceCount, 0, 0, 0);
	}
}

void CGameScene::RenderSkinnedInstanceGroupsToShadowMap(ID3D12GraphicsCommandList* cmd)
{
	if ( !cmd ) return;
	if ( !m_pd3dSkinnedInstanceBuffer ) return;
	if ( !m_pMappedSkinnedInstanceBuffer ) return;
	if ( !m_pd3dSkinnedBonePaletteBuffer ) return;
	if ( !m_pMappedSkinnedBonePaletteBuffer ) return;
	if ( !m_shadowSkinnedShader ) return;
	if ( !m_shadowAlphaClipSkinnedShader ) return;

	cmd->SetGraphicsRootShaderResourceView(
		ROOT_PARAMETER_BONE_PALETTE,
		m_pd3dSkinnedBonePaletteBuffer->GetGPUVirtualAddress()
	);
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	bool lastUseAlphaClipShader = false;
	bool hasBoundAnyShader = false;

	for ( const SkinnedInstanceGroup& group : m_skinnedInstanceGroups )
	{
		if ( !group.mesh ) continue;
		if ( group.subMeshIndex >= group.mesh->m_SubMeshes.size() ) continue;

		const SubMesh& repSm = group.mesh->m_SubMeshes[group.subMeshIndex];
		if ( repSm.indices.empty() ) continue;

		const UINT maxInstanceCount = ( UINT ) group.objectIndices.size();
		if ( maxInstanceCount == 0 ) continue;

		// pass 1: shadow
		const UINT instanceBase =
			m_skinnedInstanceBufferCapacity + group.instanceBufferStart;

		const UINT totalSkinnedInstanceCapacity =
			m_skinnedInstanceBufferCapacity * 2;

		if ( ( instanceBase + maxInstanceCount ) > totalSkinnedInstanceCapacity )
			continue;

		UINT visibleInstanceCount = 0;

		for ( UINT i = 0; i < maxInstanceCount; ++i )
		{
			const UINT objectIndex = group.objectIndices[i];
			if ( objectIndex >= ( UINT ) m_skinnedBatch.objectRefs.size() ) continue;

			if ( objectIndex < ( UINT ) m_skinnedDistanceCullFlags.size() )
			{
				if ( m_skinnedDistanceCullFlags[objectIndex] != 0 )
					continue;
			}

			if ( !IsSkinnedObjectInsideShadowBox(objectIndex) )
				continue;

			CGameObject* obj = m_skinnedBatch.objectRefs[objectIndex];
			if ( !obj ) continue;

			auto* renderer = obj->GetComponent<CSkinnedMeshRendererComponent>();
			if ( !renderer ) continue;
			if ( !renderer->IsEnabled() ) continue;

			auto* skin = obj->GetComponent<CSkinningComponent>();
			if ( !skin ) continue;
			if ( !skin->IsSkinned() ) continue;

			std::shared_ptr<CMesh> objMesh = obj->GetMeshShared(( int ) group.meshIndex);
			if ( !objMesh ) continue;
			if ( group.subMeshIndex >= objMesh->m_SubMeshes.size() ) continue;

			const SubMesh& objSm = objMesh->m_SubMeshes[group.subMeshIndex];

			SkinnedInstanceVertex& dst =
				m_pMappedSkinnedInstanceBuffer[instanceBase + visibleInstanceCount];

			const XMFLOAT4X4& W = obj->GetWorldMatrix();

			dst.world0 = XMFLOAT4(W._11, W._12, W._13, W._14);
			dst.world1 = XMFLOAT4(W._21, W._22, W._23, W._24);
			dst.world2 = XMFLOAT4(W._31, W._32, W._33, W._34);
			dst.world3 = XMFLOAT4(W._41, W._42, W._43, W._44);

			dst.materialId = ( objSm.materialId == 0xFFFFFFFFu ) ? 0u : objSm.materialId;
			dst.bonePaletteBase = objectIndex * m_skinnedBonePaletteStride;

			const XMFLOAT4X4* srcBoneMats = skin->GetMappedBoneMatrices();
			const UINT boneCount = ( UINT ) skin->GetBoneCount();

			if ( srcBoneMats && boneCount > 0 )
			{
				memcpy(
					m_pMappedSkinnedBonePaletteBuffer + dst.bonePaletteBase,
					srcBoneMats,
					sizeof(XMFLOAT4X4) * boneCount
				);
			}

			++visibleInstanceCount;
		}

		if ( visibleInstanceCount == 0 ) continue;

		if ( !hasBoundAnyShader || ( lastUseAlphaClipShader != group.useAlphaClipShader ) )
		{
			if ( group.useAlphaClipShader )
				m_shadowAlphaClipSkinnedShader->Render(cmd, nullptr, &m_skinnedBatch);
			else
				m_shadowSkinnedShader->Render(cmd, nullptr, &m_skinnedBatch);

			lastUseAlphaClipShader = group.useAlphaClipShader;
			hasBoundAnyShader = true;
		}

		D3D12_VERTEX_BUFFER_VIEW vbViews[2] = {};
		vbViews[0] = repSm.vbView;
		vbViews[1].BufferLocation =
			m_pd3dSkinnedInstanceBuffer->GetGPUVirtualAddress() +
			( UINT64 ) ( sizeof(SkinnedInstanceVertex) * instanceBase );
		vbViews[1].SizeInBytes = sizeof(SkinnedInstanceVertex) * visibleInstanceCount;
		vbViews[1].StrideInBytes = sizeof(SkinnedInstanceVertex);

		cmd->IASetVertexBuffers(0, 2, vbViews);
		cmd->IASetIndexBuffer(&repSm.ibView);

		cmd->DrawIndexedInstanced(( UINT ) repSm.indices.size(), visibleInstanceCount, 0, 0, 0);
	}
}

void CGameScene::BuildSkinnedBatch(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	const std::shared_ptr<CSkinnedObjectsShader>& pSkinnedShader,
	UINT nRenderTargets,
	DXGI_FORMAT* rtvFormats,
	DXGI_FORMAT dsvFormat
)
{
	auto* b = &m_skinnedBatch;
	if ( !b ) return;

	const UINT cap = b->capacity;
	if ( cap == 0 ) return;

	pSkinnedShader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		nRenderTargets,
		rtvFormats,
		dsvFormat
	);

	b->cbElementBytes = ( ( sizeof(CB_GAMEOBJECT_INFO) + 255 ) & ~255 );

	b->cbGameObjects = ::CreateBufferResource(
		dev, cmd, nullptr,
		b->cbElementBytes * cap,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	b->cbGameObjects->Map(0, nullptr, ( void** ) &b->mappedGameObjects);

	b->baseCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
	b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	m_pDescriptorHeap->CreateConstantBufferViews(
		dev,
		cap,
		b->cbGameObjects.Get(),
		b->cbElementBytes
	);

	m_skinnedObjects.clear();
	m_skinnedAlphaClipObjects.clear();
	m_skinnedObjects.reserve(cap);

	b->objectRefs.clear();
	b->objectRefs.reserve(cap);

	b->count = 0;
	ResetSkinnedWorldLodEntries();

	m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };

	auto MakeSkinnedContext = [ & ] (UINT objectIndex)
		{
			GameSceneObjectFactory::CreateContext ctx{};
			ctx.device = dev;
			ctx.cmd = cmd;
			ctx.mappedGameObjectCB =
				reinterpret_cast< CB_GAMEOBJECT_INFO* >(
					reinterpret_cast< UINT8* >( b->mappedGameObjects ) +
					objectIndex * b->cbElementBytes
				);
			ctx.cbvGpuHandle.ptr =
				b->baseCbvGpu.ptr + ( UINT64 ) objectIndex * b->cbvInc;
			return ctx;
		};

#ifndef USING_NETWORK
	auto AttachGhoulAIToMonster =
		[ this ] (std::unique_ptr<CGameObject>& obj)
		{
			if ( !obj )
				return;

			if ( obj->GetComponent<CGhoulAIComponent>() )
				return;

			auto* ghoulAI = obj->AddComponent<CGhoulAIComponent>();
			if ( ghoulAI )
			{
				ghoulAI->SetScene(this);
				ghoulAI->SetEnabledAI(true);
			}
		};
#endif

	auto ApplyPlayerBodyCollider =
		[ ] (GameSceneObjectFactory::SkinnedRenderableDesc& desc)
		{
			desc.addCollider = true;
			desc.colliderType = EColliderType::BCapsule;
			desc.colliderLayer = kCollisionLayerPlayer;
			desc.colliderMask =
				CollisionBit(kCollisionLayerWorldStatic) |
				CollisionBit(kCollisionLayerMonsterWeapon);
			desc.colliderEnabled = true;
		};

	auto ApplyMonsterBodyCollider =
		[ ] (GameSceneObjectFactory::SkinnedRenderableDesc& desc)
		{
			desc.addCollider = true;
			desc.colliderType = EColliderType::BCapsule;
			desc.colliderLayer = kCollisionLayerMonster;
			desc.colliderMask = CollisionBit(kCollisionLayerPlayerWeapon);
			desc.colliderEnabled = true;
		};

	auto RegisterSkinnedCullEntry =
		[this](
			CGameObject* raw,
			UINT objectIndex,
			const char* assetName,
			const XMFLOAT3& pos,
			const std::array<std::shared_ptr<CMesh>, 3>& lodMeshes,
			bool lodEnabled,
			float lodDistance01,
			float lodDistance12,
			float cullDistance)
		{
			if ( !raw || !assetName || !assetName[0] )
				return;

			SkinnedWorldLodEntry entry{};
			entry.object = raw;
			entry.skinnedBatchObjectIndex = objectIndex;
			entry.assetName = assetName;
			entry.lodReferencePosition = pos;

			entry.lodEnabled = lodEnabled;
			entry.currentLod = 0;
			entry.lodDistance01 = lodDistance01;
			entry.lodDistance12 = lodDistance12;
			entry.lodMeshes = lodMeshes;

			entry.distanceCullEnabled = true;
			entry.distanceCulled = false;
			entry.cullDistance = cullDistance;

			if ( !entry.lodMeshes[0] )
				entry.lodMeshes[0] = raw->GetMeshShared(0);

			m_skinnedWorldLodEntries.push_back(std::move(entry));
		};

	const UINT fighterCount = m_PlayerCount;
	const XMFLOAT3 playerBase(0.0f, 0.0f, -150.0f);

	m_swordManRefs.clear();
	m_swordManRefs.reserve(m_swordManCount);

	m_bowManRefs.clear();
	m_bowManRefs.reserve(m_bowManCount);

	m_MutantRefs.clear();
	m_MutantRefs.reserve(m_MutantCount);

	m_bossRefs.clear();
	m_bossRefs.reserve(m_bossCount);

	m_mutantKeyTriggerMegaByObject.clear();
	m_mutantKeyTriggerRegisteredByMega.fill(false);

	m_skinnedMonsterMegaGridNumbers.clear();
	m_skinnedMonsterMegaGridNumbers.reserve(m_skinnedBatch.capacity);
	m_sceneGrid.ClearMegaGridMonsters();

#ifdef USING_NETWORK
	GameStartData gameStartData{};
	if ( std::holds_alternative<GameStartData>(m_pendingNetworkMessage.data) )
	{
		gameStartData = std::get<GameStartData>(m_pendingNetworkMessage.data);
	}

	auto GetNetworkEnemySpawn = [ & ] (UINT index, XMFLOAT3& outPos, float& outYaw) -> bool
		{
			if ( index >= static_cast< UINT >( gameStartData.enemies.size() ) )
				return false;

			const auto& state = gameStartData.enemies[index];
			outPos = state.position;
			outYaw = state.yaw;
			return true;
		};

	auto GetNetworkPlayerSpawn = [ & ] (UINT index, XMFLOAT3& outPos, float& outYaw, EWeaponType& outWeapon) -> bool
		{
			if ( index >= static_cast< UINT >( gameStartData.players.size() ) )
				return false;

			const auto& state = gameStartData.players[index];
			outPos = state.position;
			outYaw = state.yaw;
			outWeapon = state.weaponType;
			return true;
		};
#endif

	UINT enemyIndex = 0;

#ifndef USING_NETWORK
	auto GatherLocalMonsterSpawns = [ this ] (const char* typeName)
		{
			std::vector<const MonsterSpawnEntry*> result;
			result.reserve(m_monsterSpawnEntries.size());

			for ( const MonsterSpawnEntry& entry : m_monsterSpawnEntries )
			{
				if ( entry.type == typeName )
					result.push_back(&entry);
			}

			std::sort(
				result.begin(),
				result.end(),
				[ ] (const MonsterSpawnEntry* a, const MonsterSpawnEntry* b)
				{
					return a->index < b->index;
				}
			);

			return result;
		};

	const auto ghoulSpawns = GatherLocalMonsterSpawns("Ghoul");
	const auto swordSpawns = GatherLocalMonsterSpawns("SwordMan");
	const auto bowSpawns = GatherLocalMonsterSpawns("BowMan");
	const auto mutantSpawns = GatherLocalMonsterSpawns("Mutant");
	const auto bossSpawns = GatherLocalMonsterSpawns("Boss");
#endif

	// ------------------------------------------------------------------------
	// Ghoul
	// ------------------------------------------------------------------------
	{
#ifdef USING_NETWORK
		const UINT countW = m_ghoulCount;
#else
		const UINT countW = static_cast< UINT >( ghoulSpawns.size() );
#endif

		const auto& ghoulClips = GetGhoulClipEntries();

		std::array<std::shared_ptr<CMesh>, 3> ghoulLodMeshes = { nullptr, nullptr, nullptr };

		for ( int lodLevel = 0; lodLevel < 3; ++lodLevel )
		{
			AssetBuildDesc ghoulLodDesc{};
			if ( !ResolveGhoulSkinnedLodAssetDesc(lodLevel, ghoulLodDesc) )
				continue;

			BuiltAsset ghoulLodAsset = AssetManager::BuildAsset(
				dev, cmd,
				m_pMaterials.get(),
				ghoulLodDesc
			);

			ghoulLodMeshes[( size_t ) lodLevel] = ghoulLodAsset.mesh;
		}

		std::shared_ptr<CMesh> ghoulBaseMesh = ghoulLodMeshes[0];
		if ( !ghoulBaseMesh )
		{
			AssetBuildDesc ghoulDesc{};
			GetGameSceneAssetBuildDesc(EGameSceneAssetId::Ghoul, ghoulDesc);

			BuiltAsset ghoulAsset = AssetManager::BuildAsset(
				dev, cmd,
				m_pMaterials.get(),
				ghoulDesc
			);

			ghoulBaseMesh = ghoulAsset.mesh;
			ghoulLodMeshes[0] = ghoulBaseMesh;
		}

		GameSceneObjectFactory::PreloadClipSet(
			ghoulBaseMesh.get(),
			"Ghoul",
			ghoulClips
		);

		for ( UINT k = 0; k < countW; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			XMFLOAT3 pos{};
			float yaw = 180.0f;

#ifdef USING_NETWORK
			if ( !GetNetworkEnemySpawn(enemyIndex, pos, yaw) )
				break;
#else
			if ( k >= ghoulSpawns.size() )
				break;

			pos = ghoulSpawns[k]->pos;
			yaw = ghoulSpawns[k]->yawDeg;
#endif

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = ghoulBaseMesh;
			createDesc.position = pos;
			createDesc.yawDeg = yaw;

			ApplyMonsterBodyCollider(createDesc);

			createDesc.addAnimator = true;
			createDesc.addActorTag = true;
			createDesc.actorKind = EActorKind::NPC;
			createDesc.playerControl = EPlayerControl::None;
			createDesc.playerSlot = -1;

			createDesc.addMonsterCombat = true;
			createDesc.addMonsterWeaponHitbox = true;

			createDesc.addHealth = true;
			createDesc.maxHp = kHpGhoul;

			createDesc.addAttackPower = true;
			createDesc.attackPower = kAttackPowerGhoul;

			createDesc.skeletonKey = "Ghoul";
			createDesc.clipEntries = &ghoulClips;

			createDesc.initMonsterController = true;
			createDesc.monsterInitialState = EMonsterAnimState::Idle;
			createDesc.monsterProfile.idleClip = "Idle";
			createDesc.monsterProfile.moveClip = "Walk";
			createDesc.monsterProfile.runClip = "Run";
			createDesc.monsterProfile.hitClip = "Hit";
			createDesc.monsterProfile.attackClip = "Attack";
			createDesc.monsterProfile.deathClip = "Death";

			createDesc.useOwnerBoneWeaponCapsules = true;
			createDesc.monsterWeaponConfigs.push_back(
				{ "Attack", 0.20f, 0.55f, { "hand_r" } }
			);

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

#ifndef USING_NETWORK
			AttachGhoulAIToMonster(obj);
#endif

			++enemyIndex;

			CGameObject* raw = obj.get();

			RegisterMonsterToMegaGrid(raw, pos, i);

			RegisterSkinnedCullEntry(
				raw, i, "Ghoul", pos,
				ghoulLodMeshes, true,
				35.0f, 90.0f, 120.0f
			);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();
		}
	}

	// ------------------------------------------------------------------------
	// SwordMan
	// ------------------------------------------------------------------------
	{
#ifdef USING_NETWORK
		const UINT countX = m_swordManCount;
#else
		const UINT countX = static_cast< UINT >( swordSpawns.size() );
#endif

		const auto& swordClips = GetEnemySwordClipEntries();

		AssetBuildDesc swordManDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::SwordMan, swordManDesc);

		BuiltAsset swordManAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			swordManDesc
		);

		GameSceneObjectFactory::PreloadClipSet(
			swordManAsset.mesh.get(),
			"EnemySword",
			swordClips
		);

		for ( UINT k = 0; k < countX; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			XMFLOAT3 pos{};
			float yaw = 180.0f;

#ifdef USING_NETWORK
			if ( !GetNetworkEnemySpawn(enemyIndex, pos, yaw) )
				break;
#else
			if ( k >= swordSpawns.size() )
				break;

			pos = swordSpawns[k]->pos;
			yaw = swordSpawns[k]->yawDeg;
#endif

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = swordManAsset.mesh;
			createDesc.position = pos;
			createDesc.yawDeg = yaw;

			ApplyMonsterBodyCollider(createDesc);

			createDesc.addAnimator = true;
			createDesc.addActorTag = true;
			createDesc.actorKind = EActorKind::NPC;
			createDesc.playerControl = EPlayerControl::None;
			createDesc.playerSlot = -1;

			createDesc.addMonsterCombat = true;

			createDesc.addHealth = true;
			createDesc.maxHp = kHpSwordMan;

			createDesc.skeletonKey = "EnemySword";
			createDesc.clipEntries = &swordClips;

			createDesc.initMonsterController = true;
			createDesc.monsterInitialState = EMonsterAnimState::Idle;
			createDesc.monsterProfile.idleClip = "Idle";
			createDesc.monsterProfile.moveClip = "Walk";
			createDesc.monsterProfile.hitClip = "Hit";
			createDesc.monsterProfile.attackClip = "Attack";
			createDesc.monsterProfile.deathClip = "Death";

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

#ifndef USING_NETWORK
			AttachGhoulAIToMonster(obj);
#endif

			++enemyIndex;

			CGameObject* raw = obj.get();

			RegisterMonsterToMegaGrid(raw, pos, i);

			std::array<std::shared_ptr<CMesh>, 3> noLodMeshes =
			{
				swordManAsset.mesh, nullptr, nullptr
			};

			RegisterSkinnedCullEntry(
				raw, i, "SwordMan", pos,
				noLodMeshes, false,
				0.0f, 0.0f, 90.0f
			);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_swordManRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// BowMan
	// ------------------------------------------------------------------------
	{
#ifdef USING_NETWORK
		const UINT countY = m_bowManCount;
#else
		const UINT countY = static_cast< UINT >( bowSpawns.size() );
#endif

		const auto& bowManClips = GetEnemyBowClipEntries();

		AssetBuildDesc bowManDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::BowMan, bowManDesc);

		BuiltAsset bowManAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			bowManDesc
		);

		GameSceneObjectFactory::PreloadClipSet(
			bowManAsset.mesh.get(),
			"EnemyBow",
			bowManClips
		);

		for ( UINT k = 0; k < countY; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			XMFLOAT3 pos{};
			float yaw = 180.0f;

#ifdef USING_NETWORK
			if ( !GetNetworkEnemySpawn(enemyIndex, pos, yaw) )
				break;
#else
			if ( k >= bowSpawns.size() )
				break;

			pos = bowSpawns[k]->pos;
			yaw = bowSpawns[k]->yawDeg;
#endif

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = bowManAsset.mesh;
			createDesc.position = pos;
			createDesc.yawDeg = yaw;

			ApplyMonsterBodyCollider(createDesc);

			createDesc.addAnimator = true;
			createDesc.addActorTag = true;
			createDesc.actorKind = EActorKind::NPC;
			createDesc.playerControl = EPlayerControl::None;
			createDesc.playerSlot = -1;

			createDesc.addMonsterCombat = true;

			createDesc.addHealth = true;
			createDesc.maxHp = kHpBowMan;

			createDesc.skeletonKey = "EnemyBow";
			createDesc.clipEntries = &bowManClips;

			createDesc.initMonsterController = true;
			createDesc.monsterInitialState = EMonsterAnimState::Idle;
			createDesc.monsterProfile.idleClip = "Idle";
			createDesc.monsterProfile.moveClip = "Walk";
			createDesc.monsterProfile.hitClip = "Hit";
			createDesc.monsterProfile.deathClip = "Death";
			createDesc.monsterProfile.attackClip = "Bow_Load";
			createDesc.monsterProfile.attackNextClip = "Bow_Release";
			createDesc.monsterProfile.attackHasChain = true;

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

#ifndef USING_NETWORK
			AttachGhoulAIToMonster(obj);
#endif

			++enemyIndex;

			CGameObject* raw = obj.get();

			RegisterMonsterToMegaGrid(raw, pos, i);

			std::array<std::shared_ptr<CMesh>, 3> noLodMeshes =
			{
				bowManAsset.mesh, nullptr, nullptr
			};

			RegisterSkinnedCullEntry(
				raw, i, "BowMan", pos,
				noLodMeshes, false,
				0.0f, 0.0f, 100.0f
			);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_bowManRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// Mutant
	// ------------------------------------------------------------------------
	{
#ifdef USING_NETWORK
		const UINT countZ = m_MutantCount;
#else
		const UINT countZ = static_cast< UINT >( mutantSpawns.size() );
#endif

		const auto& mutantClips = GetMutantClipEntries();

		AssetBuildDesc mutantDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::Mutant, mutantDesc);

		BuiltAsset mutantAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			mutantDesc
		);

		GameSceneObjectFactory::PreloadClipSet(
			mutantAsset.mesh.get(),
			"Mutant",
			mutantClips
		);

		for ( UINT k = 0; k < countZ; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			XMFLOAT3 pos{};
			float yaw = 180.0f;

#ifdef USING_NETWORK
			if ( !GetNetworkEnemySpawn(enemyIndex, pos, yaw) )
				break;
#else
			if ( k >= mutantSpawns.size() )
				break;

			pos = mutantSpawns[k]->pos;
			yaw = mutantSpawns[k]->yawDeg;
#endif

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = mutantAsset.mesh;
			createDesc.position = pos;
			createDesc.yawDeg = yaw;

			ApplyMonsterBodyCollider(createDesc);

			createDesc.addAnimator = true;
			createDesc.addActorTag = true;
			createDesc.actorKind = EActorKind::NPC;
			createDesc.playerControl = EPlayerControl::None;
			createDesc.playerSlot = -1;

			createDesc.addMonsterCombat = true;
			createDesc.addMonsterWeaponHitbox = true;

			createDesc.addHealth = true;
			createDesc.maxHp = kHpMutant;

			createDesc.addAttackPower = true;
			createDesc.attackPower = kAttackPowerMutant;

			createDesc.skeletonKey = "Mutant";
			createDesc.clipEntries = &mutantClips;

			createDesc.initMonsterController = true;
			createDesc.monsterInitialState = EMonsterAnimState::Idle;
			createDesc.monsterProfile.idleClip = "Idle";
			createDesc.monsterProfile.moveClip = "Walk";
			createDesc.monsterProfile.hitClip = "Hit";
			createDesc.monsterProfile.attackClip = "Attack";
			createDesc.monsterProfile.deathClip = "Death";

			createDesc.useOwnerBoneWeaponCapsules = true;
			createDesc.monsterWeaponConfigs.push_back(
				{ "Attack", 0.20f, 0.55f, { "CATRigRArmPalm" } }
			);

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

#ifndef USING_NETWORK
			AttachGhoulAIToMonster(obj);
#endif

			++enemyIndex;

			CGameObject* raw = obj.get();

			RegisterMonsterToMegaGrid(raw, pos, i);

			const int mutantMegaGridNumber =
				m_sceneGrid.MegaGridNumberFromWorldPosition(pos.x, pos.z);

			RegisterMutantKeyTriggerIfNeeded(raw, mutantMegaGridNumber);

			std::array<std::shared_ptr<CMesh>, 3> noLodMeshes =
			{
				mutantAsset.mesh, nullptr, nullptr
			};

			RegisterSkinnedCullEntry(
				raw, i, "Mutant", pos,
				noLodMeshes, false,
				0.0f, 0.0f, 110.0f
			);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_MutantRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// Boss
	// ------------------------------------------------------------------------
	{
#ifdef USING_NETWORK
		const UINT countOne = m_bossCount;
#else
		const UINT countOne = static_cast< UINT >( bossSpawns.size() );
#endif

		const auto& bossClips = GetBossClipEntries();

		AssetBuildDesc bossDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::Boss, bossDesc);

		BuiltAsset bossAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			bossDesc
		);

		GameSceneObjectFactory::PreloadClipSet(
			bossAsset.mesh.get(),
			"Boss",
			bossClips
		);

		for ( UINT k = 0; k < countOne; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			XMFLOAT3 pos{};
			float yaw = 180.0f;

#ifdef USING_NETWORK
			if ( !GetNetworkEnemySpawn(enemyIndex, pos, yaw) )
				break;
#else
			if ( k >= bossSpawns.size() )
				break;

			pos = bossSpawns[k]->pos;
			yaw = bossSpawns[k]->yawDeg;
#endif

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = bossAsset.mesh;
			createDesc.position = pos;
			createDesc.yawDeg = yaw;

			ApplyMonsterBodyCollider(createDesc);

			createDesc.addAnimator = true;
			createDesc.addActorTag = true;
			createDesc.actorKind = EActorKind::NPC;
			createDesc.playerControl = EPlayerControl::None;
			createDesc.playerSlot = -1;

			createDesc.addMonsterCombat = true;
			createDesc.addMonsterWeaponHitbox = true;

			createDesc.addHealth = true;
			createDesc.maxHp = kHpBoss;

			createDesc.addAttackPower = true;
			createDesc.attackPower = kAttackPowerBoss;

			createDesc.skeletonKey = "Boss";
			createDesc.clipEntries = &bossClips;

			createDesc.initMonsterController = true;
			createDesc.monsterInitialState = EMonsterAnimState::Idle;
			createDesc.monsterProfile.idleClip = "Idle";
			createDesc.monsterProfile.moveClip = "Walk";
			createDesc.monsterProfile.hitClip = "Hit";
			createDesc.monsterProfile.deathClip = "Death";
			createDesc.monsterProfile.attackClip = "AttackLeft";
			createDesc.monsterProfile.appearClip = "Appear";
			createDesc.monsterProfile.callClip = "Call";
			createDesc.monsterProfile.spellClip = "Spell";

			createDesc.useOwnerBoneWeaponCapsules = true;
			createDesc.monsterWeaponConfigs.push_back(
				{ "AttackLeft", 0.20f, 0.55f, { "Wrist_L" } }
			);
			createDesc.monsterWeaponConfigs.push_back(
				{ "AttackRight", 0.20f, 0.55f, { "Wrist_R" } }
			);

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

#ifndef USING_NETWORK
			AttachGhoulAIToMonster(obj);
#endif

			++enemyIndex;

			CGameObject* raw = obj.get();

			m_bossRefs.push_back(raw);

			RegisterMonsterToMegaGrid(raw, pos, i);

			std::array<std::shared_ptr<CMesh>, 3> noLodMeshes =
			{
				bossAsset.mesh, nullptr, nullptr
			};

			RegisterSkinnedCullEntry(
				raw, i, "Boss", pos,
				noLodMeshes, false,
				0.0f, 0.0f, 160.0f
			);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();
		}
	}

	// ------------------------------------------------------------------------
	// Player
	// ------------------------------------------------------------------------
	{
		const auto& playerClips = GetPlayerClipEntries();

		std::array<AssetBuildDesc, 4> playerDescs{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerMesh1, playerDescs[0]);
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerMesh2, playerDescs[1]);
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerMesh3, playerDescs[2]);
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerMesh4, playerDescs[3]);

		for ( UINT k = 0; k < fighterCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();
			const int slot = ( int ) k;
			const bool isLocal = ( slot == m_localPlayerSlot );

			BuiltAsset playerAsset = AssetManager::BuildAsset(
				dev, cmd,
				m_pMaterials.get(),
				playerDescs[( size_t ) slot]
			);

			XMFLOAT3 pos{};
			float yaw = 0.0f;

#ifdef USING_NETWORK
			EWeaponType initialWeapon = EWeaponType::Sword;
			if ( !GetNetworkPlayerSpawn(k, pos, yaw, initialWeapon) )
				break;
#else
			pos.x = playerBase.x + 2.0f * ( float ) slot;
			pos.y = playerBase.y;
			pos.z = playerBase.z;
#endif

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = playerAsset.mesh;
			createDesc.position = pos;
			createDesc.yawDeg = yaw;

			ApplyPlayerBodyCollider(createDesc);

			createDesc.addAnimator = true;
			createDesc.addActorTag = true;
			createDesc.actorKind = EActorKind::Player;
			createDesc.playerControl = isLocal ? EPlayerControl::Local : EPlayerControl::Remote;
			createDesc.playerSlot = slot;

			createDesc.addPlayerController = isLocal;
			createDesc.addPlayerEquipment = true;
			
			createDesc.addHealth = true;
			createDesc.maxHp = kHpPlayer;

			createDesc.skeletonKey = "Player";
			createDesc.clipEntries = &playerClips;

			createDesc.initPlayerController = true;
			createDesc.playerIdleClip = "Idle_Normal";
			createDesc.playerMoveClip = "Walk_F";
			createDesc.playerHitClip = "Hit_Normal";
			createDesc.playerAttackClip = "Attack_Sword";

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

#ifdef USING_NETWORK
			if ( auto* equipComp = obj->GetComponent<CPlayerEquipmentComponent>() )
			{
				equipComp->SetLoadout(initialWeapon);
			}
#endif

			CGameObject* raw = obj.get();
			if ( auto* equip = raw->GetComponent<CPlayerEquipmentComponent>() )
			{
				equip->SetAudioManager(m_pAudioManager);
			}

			if ( slot >= 0 && slot <= 3 )
				m_playersBySlot[( size_t ) slot] = raw;

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();
		}
	}

	// ------------------------------------------------------------------------
	// PlayerBow pool
	// ------------------------------------------------------------------------
	{
		const auto& playerBowClips = GetPlayerBowClipEntries();

		AssetBuildDesc playerBowDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::PlayerBow, playerBowDesc);

		BuiltAsset bowAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			playerBowDesc
		);

		GameSceneObjectFactory::PreloadClipSet(
			bowAsset.mesh.get(),
			"BowP",
			playerBowClips
		);

		m_PlayerBowRefs.clear();
		m_PlayerBowRefs.reserve(m_PlayerBowCount);

		for ( UINT k = 0; k < m_PlayerBowCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = bowAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addAnimator = true;
			createDesc.skeletonKey = "BowP";
			createDesc.clipEntries = &playerBowClips;

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_skinnedAlphaClipObjects.insert(raw);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_PlayerBowRefs.push_back(raw);
		}
	}

	// ------------------------------------------------------------------------
	// EnemyBow pool
	// ------------------------------------------------------------------------
	{
		const auto& enemyBowWeaponClips = GetEnemyBowWeaponClipEntries();

		AssetBuildDesc enemyBowDesc{};
		GetGameSceneAssetBuildDesc(EGameSceneAssetId::EnemyBow, enemyBowDesc);

		BuiltAsset bowAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			enemyBowDesc
		);

		GameSceneObjectFactory::PreloadClipSet(
			bowAsset.mesh.get(),
			"BowE",
			enemyBowWeaponClips
		);

		m_EnemyBowRefs.clear();
		m_EnemyBowRefs.reserve(m_bowManCount);

		for ( UINT k = 0; k < m_bowManCount; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			GameSceneObjectFactory::SkinnedRenderableDesc createDesc{};
			createDesc.ctx = MakeSkinnedContext(i);
			createDesc.mesh = bowAsset.mesh;
			createDesc.spawnHidden = true;

			createDesc.addAnimator = true;
			createDesc.skeletonKey = "BowE";
			createDesc.clipEntries = &enemyBowWeaponClips;

			auto obj = GameSceneObjectFactory::CreateSkinnedRenderable(createDesc);
			if ( !obj )
				continue;

			CGameObject* raw = obj.get();
			m_skinnedAlphaClipObjects.insert(raw);

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_EnemyBowRefs.push_back(raw);
		}
	}

	m_preparedBowmanArrows.assign(m_bowManRefs.size(), nullptr);
	m_prevEnemyBowReleasePhase.assign(m_bowManRefs.size(), false);

	BuildSkinnedInstanceGroups();

	if ( m_pd3dSkinnedInstanceBuffer )
	{
		if ( m_pMappedSkinnedInstanceBuffer )
		{
			m_pd3dSkinnedInstanceBuffer->Unmap(0, NULL);
			m_pMappedSkinnedInstanceBuffer = nullptr;
		}
		m_pd3dSkinnedInstanceBuffer.Reset();
	}

	if ( m_pd3dSkinnedBonePaletteBuffer )
	{
		if ( m_pMappedSkinnedBonePaletteBuffer )
		{
			m_pd3dSkinnedBonePaletteBuffer->Unmap(0, NULL);
			m_pMappedSkinnedBonePaletteBuffer = nullptr;
		}
		m_pd3dSkinnedBonePaletteBuffer.Reset();
	}

	m_skinnedBonePaletteStride = 1;
	for ( UINT i = 0; i < ( UINT ) m_skinnedBatch.objectRefs.size(); ++i )
	{
		CGameObject* obj = m_skinnedBatch.objectRefs[i];
		if ( !obj ) continue;

		const UINT boneCount = ( UINT ) obj->GetBoneCount();
		if ( boneCount > m_skinnedBonePaletteStride )
			m_skinnedBonePaletteStride = boneCount;
	}

	m_skinnedBonePaletteCapacity =
		m_skinnedBonePaletteStride * ( UINT ) m_skinnedBatch.objectRefs.size();

	if ( m_skinnedInstanceBufferCapacity > 0 )
	{
		// pass 0: scene
		// pass 1: shadow
		const UINT kSkinnedInstancePassCount = 2;

		const UINT instanceBufferBytes =
			sizeof(SkinnedInstanceVertex) *
			m_skinnedInstanceBufferCapacity *
			kSkinnedInstancePassCount;

		m_pd3dSkinnedInstanceBuffer = ::CreateBufferResource(
			dev, cmd, nullptr,
			instanceBufferBytes,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr
		);

		m_pd3dSkinnedInstanceBuffer->Map(
			0, nullptr, ( void** ) &m_pMappedSkinnedInstanceBuffer);
	}

	if ( m_skinnedBonePaletteCapacity > 0 )
	{
		const UINT bonePaletteBufferBytes =
			sizeof(XMFLOAT4X4) * m_skinnedBonePaletteCapacity;

		m_pd3dSkinnedBonePaletteBuffer = ::CreateBufferResource(
			dev, cmd, nullptr,
			bonePaletteBufferBytes,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr
		);

		m_pd3dSkinnedBonePaletteBuffer->Map(
			0, nullptr, ( void** ) &m_pMappedSkinnedBonePaletteBuffer);
	}

	BuildSkinnedOcclusionEntries();

	m_skinnedShadowOcclusionEntryIndices.assign(m_skinnedBatch.objectRefs.size(), -1);

	for ( UINT entryIndex = 0; entryIndex < ( UINT ) m_skinnedOcclusionEntries.size(); ++entryIndex )
	{
		const SkinnedOcclusionEntry& entry = m_skinnedOcclusionEntries[entryIndex];

		if ( entry.skinnedBatchObjectIndex >= ( UINT ) m_skinnedShadowOcclusionEntryIndices.size() )
			continue;

		m_skinnedShadowOcclusionEntryIndices[entry.skinnedBatchObjectIndex] =
			static_cast< int >(entryIndex);
	}

	BuildSkinnedOcclusionGpuResources(dev);
}

void CGameScene::BuildColliderBatch(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	const std::shared_ptr<CDiffusedShader>& pshader,
	UINT nRenderTargets,
	DXGI_FORMAT* rtvFormats,
	DXGI_FORMAT dsvFormat)
{
	auto* b = &m_colliderBatch;
	if ( !b ) return;

	const UINT cap = kDebugSubmeshOOBBCapacity;
	b->capacity = cap;

	if ( cap == 0 ) 
		return;

	pshader->CreateShader(
		dev,
		m_pd3dGraphicsRootSignature.Get(),
		nRenderTargets,
		rtvFormats,
		dsvFormat
	);

	b->cbElementBytes = ( ( sizeof(CB_GAMEOBJECT_INFO) + 255 ) & ~255 );

	b->cbGameObjects = ::CreateBufferResource(
		dev, cmd, nullptr,
		b->cbElementBytes * cap,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	b->cbGameObjects->Map(0, nullptr, ( void** ) &b->mappedGameObjects);

	b->baseCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
	b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	m_pDescriptorHeap->CreateConstantBufferViews(
		dev,
		cap,
		b->cbGameObjects.Get(),
		b->cbElementBytes
	);

	m_staticObjects.clear();
	m_staticObjects.reserve(cap);

	b->objectRefs.clear();
	b->objectRefs.reserve(cap);

	m_colliderObjects.clear();
	m_colliderObjects.reserve(cap);

	b->count = 0;
	m_ColliderCount = 0;
}

void CGameScene::LinkSceneObjects()
{
	GameSceneAttachmentBinder::LinkInput input{};
	input.playerSpotFollower = nullptr;

	CGameObject* local = GetPlayer();
	if ( !local )
		local = GetPlayerBySlot(0);

	input.preferredPlayer = local;
	input.playersBySlot = &m_playersBySlot;
	input.playerCount = m_PlayerCount;

	input.playerSwordRefs = &m_PlayerSwordRefs;
	input.playerBowRefs = &m_PlayerBowRefs;
	input.playerAxeRefs = &m_PlayerAxeRefs;
	input.playerGunRefs = &m_PlayerGunRefs;

	input.enemySwordRefs = &m_EnemySwordRefs;
	input.enemyBowRefs = &m_EnemyBowRefs;

	input.swordManRefs = &m_swordManRefs;
	input.bowManRefs = &m_bowManRefs;
	input.mutantRefs = &m_MutantRefs;
	input.helmetRefs = &m_helmetRefs;

#ifndef USING_NETWORK
	input.applyOfflineTestLoadout = true;
#else
	input.applyOfflineTestLoadout = false;
#endif

	GameSceneAttachmentBinder::LinkSceneObjects(input, m_attachmentBinds);
}

void CGameScene::CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* target)
{
	m_pMainCameraObject = std::make_unique<CGameObject>(0);

	auto* cam = m_pMainCameraObject->AddComponent<CThirdPersonCamera>();
	m_pMainCamera = cam;

	cam->SetMode(THIRD_PERSON_CAMERA);
	cam->SetTarget(target);

	cam->SetTimeLag(0.0f);

	cam->SetOffset(XMFLOAT3(0.0f, 0.0f, -2.0f));

	cam->GetPitch() = 12.0f;

	if ( target )
	{
		const XMFLOAT4X4& W = target->GetWorldMatrix();
		cam->GetYaw() = XMConvertToDegrees(std::atan2f(W._31, W._33));
	}
	else
	{
		cam->GetYaw() = 0.0f;
	}

	cam->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
	cam->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
	cam->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);

	m_pMainCameraObject->CreateComponents(dev, cmd);

	if ( target )
	{
		XMFLOAT3 lookAt = target->GetPosition();
		static constexpr float kCameraLookAtHeight = 1.7f;
		lookAt.y += kCameraLookAtHeight;

		cam->Update(lookAt, 0.0f);
		cam->SetLookAt(lookAt);
		cam->RegenerateViewMatrix();
	}
}

void CGameScene::BuildDepthFogResources(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
	m_depthFog.BuildResources(dev, cmd, GetGraphicsRootSignature());
}

void CGameScene::RenderDepthFog(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	m_depthFog.Render(cmd, camera);
}

bool CGameScene::IsStaticObjectInsideShadowBox(UINT objectIndex) const
{
	if ( objectIndex >= ( UINT ) m_staticShadowOcclusionEntryIndices.size() )
		return true;

	const int entryIndex = m_staticShadowOcclusionEntryIndices[objectIndex];

	if ( entryIndex < 0 )
		return true;

	if ( entryIndex >= ( int ) m_staticOcclusionEntries.size() )
		return true;

	const StaticOcclusionEntry& entry =
		m_staticOcclusionEntries[( size_t ) entryIndex];

	if ( !entry.hasWorldBounds )
		return true;

	return m_shadowMap.IsWorldOOBBInsideShadowBox(entry.worldBounds);
}

bool CGameScene::IsSkinnedObjectInsideShadowBox(UINT objectIndex) const
{
	if ( objectIndex >= ( UINT ) m_skinnedShadowOcclusionEntryIndices.size() )
		return true;

	const int entryIndex = m_skinnedShadowOcclusionEntryIndices[objectIndex];

	if ( entryIndex < 0 )
		return true;

	if ( entryIndex >= ( int ) m_skinnedOcclusionEntries.size() )
		return true;

	const SkinnedOcclusionEntry& entry =
		m_skinnedOcclusionEntries[( size_t ) entryIndex];

	if ( !entry.hasWorldBounds )
		return true;

	return m_shadowMap.IsWorldOOBBInsideShadowBox(entry.worldBounds);
}

void CGameScene::RenderShadowMap(ID3D12GraphicsCommandList* cmd)
{
	PROFILE_RENDER_SCOPE("GameScene::RenderShadowMap");

	if ( !cmd ) return;
	if ( !m_shadowMap.IsReady() ) return;
	if ( !m_shadowStaticShader ) return;
	if ( !m_shadowSkinnedShader ) return;

	const D3D12_GPU_VIRTUAL_ADDRESS materialCbGpuAddress =
		m_pd3dcbMaterials
		? m_pd3dcbMaterials->GetGPUVirtualAddress()
		: 0;

	const bool begun =
		m_shadowMap.BeginRender(
			cmd,
			GetGraphicsRootSignature(),
			m_pDescriptorHeap.get(),
			materialCbGpuAddress
		);

	if ( !begun ) return;

	RenderStaticInstanceGroupsToShadowMap(cmd);
	RenderSkinnedInstanceGroupsToShadowMap(cmd);

	m_shadowMap.EndRender(cmd);
}

void CGameScene::RestoreSceneRenderTargets(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd ) return;
	if ( !m_bSceneRenderTargetsReady ) return;
	if ( m_sceneRenderTargetCount == 0 ) return;

	cmd->OMSetRenderTargets(
		m_sceneRenderTargetCount,
		m_sceneRtvHandles.data(),
		FALSE,
		&m_sceneDsvHandle
	);

	if ( camera )
		camera->SetViewportsAndScissorRects(cmd);
}

void CGameScene::RenderShadowPrePass(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::RenderShadowPrePass(total)");

	if ( !cmd )
		return;

	{
		PROFILE_RENDER_SCOPE("GameScene::RenderShadowPrePass::PrepareFrame");

		CScene::OnPrepareRender(cmd, camera);

		UpdateFrameRenderState(camera);

		UpdateShaderVariables(cmd);

		BindFrameRootParameters(cmd);

		BuildStaticShadowVisibleListsForFrame();
	}

	{
		PROFILE_RENDER_SCOPE("GameScene::RenderShadowPrePass::RenderShadowMap");
		RenderShadowMap(cmd);
	}

	RestoreSceneRenderTargets(cmd, camera);
}

bool CGameScene::GetPauseOverlayRect(XMFLOAT4& outRect) const
{
	return m_hud.GetPauseOverlayRect(outRect);
}

bool CGameScene::IsPointInPauseOverlay(POINT clientPt) const
{
	return m_hud.IsPointInPauseOverlay(clientPt);
}

bool CGameScene::IsPointInResumeButton(POINT clientPt) const
{
	return m_hud.IsPointInResumeButton(clientPt);
}

bool CGameScene::IsPointInExitButton(POINT clientPt) const
{
	return m_hud.IsPointInExitButton(clientPt);
}

void CGameScene::SetMaterialDiffuseSrvIndex(int materialId, UINT srvIndex)
{
	if ( !m_pMaterials )
		return;

	if ( materialId < 0 || materialId >= MAX_MATERIALS )
		return;

	m_pMaterials->m_pReflections[materialId].m_xmn4TextureIndices.x =
		( srvIndex == UINT_MAX ) ? 0u : ( srvIndex + 1u );
}

void CGameScene::SetKeyItemDiffuseSrvIndex(UINT srvIndex)
{
	SetMaterialDiffuseSrvIndex(( int ) kItemBillboardKeyMaterialId, srvIndex);
}

void CGameScene::SetTransparentItemDiffuseSrvIndex(UINT srvIndex)
{
	SetMaterialDiffuseSrvIndex(
		static_cast< int >( kTransparentItemBillboardMaterialId ),
		srvIndex
	);
}

CGameObject* CGameScene::GetDemoFighter(int index) const
{
    if (index < 0 || index >= 3) return nullptr;
    return GetPlayerBySlot(index + 1);
}

void CGameScene::RequestDemoFighterAttack(int index)
{
    if (index < 0 || index >= 3) return;
    RequestPlayerAttackBySlot(index + 1);
}

void CGameScene::RequestPlayerAttackBySlot(int slot)
{
#ifdef USING_NETWORK
	UNREFERENCED_PARAMETER(slot);
	return;
#else
	CGameObject* obj = GetPlayerBySlot(slot);
	if ( !obj ) return;

	if ( slot == m_localPlayerSlot && m_bLocalPlayerDead )
		return;

	constexpr float kArrowPullBackDistance = 0.35f;
	constexpr float kBulletSpeed = 25.0f;
	constexpr float kBulletLife = 3.0f;

	bool shouldPlaySwordWhoosh = false;
	bool shouldPlayAxeWhoosh = false;
	bool shouldPrepareArrow = false;
	bool shouldFireBullet = false;

	CPlayerEquipmentComponent* equipComp = nullptr;

	if ( auto* equip = obj->GetComponent<CPlayerEquipmentComponent>() )
	{
		equipComp = equip;

		const EWeaponType weapon = equip->GetEquippedWeapon();

		shouldPlaySwordWhoosh = ( weapon == EWeaponType::Sword );
		shouldPlayAxeWhoosh = ( weapon == EWeaponType::Axe );
		shouldPrepareArrow = ( weapon == EWeaponType::Bow );
		shouldFireBullet = ( weapon == EWeaponType::Gun );
	}

	if ( auto* animComp = obj->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->EnsureController() )
		{
			const bool accepted = ctrl->RequestAttack();

			if ( accepted && equipComp )
			{
				if ( shouldPlaySwordWhoosh )
				{
					equipComp->RequestSwordAttackWhoosh();
					BeginSwordTrail(obj);
				}
				else if ( shouldPlayAxeWhoosh )
				{
					equipComp->RequestAxeAttackWhoosh();
					BeginAxeTrail(obj);
				}
			}

			if ( accepted && shouldPrepareArrow )
				RequestPrepareArrow(obj, kArrowPullBackDistance);

			if ( accepted && shouldFireBullet )
			{
				if ( equipComp )
					equipComp->RequestGunShotSfx();

				RequestFireBullet(obj, kBulletSpeed, kBulletLife);
			}

			return;
		}
	}

	if ( auto* ctrl = obj->GetAnimController() )
	{
		const bool accepted = ctrl->RequestAttack();

		if ( accepted && equipComp )
		{
			if ( shouldPlaySwordWhoosh )
			{
				equipComp->RequestSwordAttackWhoosh();
				BeginSwordTrail(obj);
			}
			else if ( shouldPlayAxeWhoosh )
			{
				equipComp->RequestAxeAttackWhoosh();
				BeginAxeTrail(obj);
			}
		}

		if ( accepted && shouldPrepareArrow )
			RequestPrepareArrow(obj, kArrowPullBackDistance);

		if ( accepted && shouldFireBullet )
		{
			if ( equipComp )
				equipComp->RequestGunShotSfx();

			RequestFireBullet(obj, kBulletSpeed, kBulletLife);
		}

		return;
	}
#endif
}

int CGameScene::GetPlayerSlotFromObject(const CGameObject* obj) const
{
    if (!obj) return -1;

    auto* tag = obj->GetComponent<CActorTagComponent>();
    if (!tag) return -1;
    if (tag->kind != EActorKind::Player) return -1;
    if (tag->playerSlot < 0 || tag->playerSlot > 3) return -1;

    return tag->playerSlot;
}

int CGameScene::GetBowManIndexFromObject(const CGameObject* obj) const
{
	if ( !obj ) return -1;

	for ( size_t i = 0; i < m_bowManRefs.size(); ++i )
	{
		if ( m_bowManRefs[i] == obj )
			return static_cast< int >(i);
	}

	return -1;
}

void CGameScene::RequestPrepareArrow(CGameObject* shooter, float pullBackDistance)
{
    if (!shooter) return;

    auto* equip = shooter->GetComponent<CPlayerEquipmentComponent>();
    if (!equip) return;
    if (equip->GetEquippedWeapon() != EWeaponType::Bow) return;

    CGameObject* bowObj = equip->GetWeaponObject(EWeaponType::Bow);
    if (!bowObj) return;

    const int slot = GetPlayerSlotFromObject(shooter);
    if (slot < 0 || slot > 3) return;

    // 이미 준비된 화살이 있으면 중복 생성 안 함
    if (m_preparedPlayerArrows[(size_t)slot])
        return;

    for (CGameObject* arrowObj : m_arrowRefs)
    {
        if (!arrowObj) continue;

        auto* arrow = arrowObj->GetComponent<CArrowComponent>();
        if (!arrow) continue;

        if (arrow->IsActive()) continue;

		SetObjectAttackPower(arrowObj, kAttackPowerPlayerArrow);

		arrow->Prepare(bowObj, shooter, pullBackDistance, true, true);
		m_preparedPlayerArrows[( size_t ) slot] = arrowObj;
		return;
    }
}

void CGameScene::RequestPrepareBowmanArrow(CGameObject* bowman, float pullBackDistance)
{
	if ( !bowman ) return;

	const int bowmanIndex = GetBowManIndexFromObject(bowman);
	if ( bowmanIndex < 0 ) return;

	const size_t idx = static_cast< size_t >(bowmanIndex);

	if ( idx >= m_preparedBowmanArrows.size() ) return;
	if ( idx >= m_EnemyBowRefs.size() ) return;

	// 이미 준비된 화살이 있으면 중복 생성 안 함
	if ( m_preparedBowmanArrows[idx] )
		return;

	CGameObject* bowObj = m_EnemyBowRefs[idx];
	if ( !bowObj ) return;

	for ( CGameObject* arrowObj : m_arrowRefs )
	{
		if ( !arrowObj ) continue;

		auto* arrow = arrowObj->GetComponent<CArrowComponent>();
		if ( !arrow ) continue;
		if ( arrow->IsActive() ) continue;

		SetObjectAttackPower(arrowObj, kAttackPowerEnemyArrow);

		arrow->Prepare(bowObj, bowman, pullBackDistance, false, true);
		m_preparedBowmanArrows[idx] = arrowObj;
		return;
	}
}

void CGameScene::RequestReleasePreparedBowmanArrow(CGameObject* bowman, float speed, float lifeSec)
{
	if ( !bowman ) return;

	const int bowmanIndex = GetBowManIndexFromObject(bowman);
	if ( bowmanIndex < 0 ) return;

	const size_t idx = static_cast< size_t >(bowmanIndex);
	if ( idx >= m_preparedBowmanArrows.size() ) return;

	CGameObject* arrowObj = m_preparedBowmanArrows[idx];
	if ( !arrowObj ) return;

	auto* arrow = arrowObj->GetComponent<CArrowComponent>();
	if ( !arrow || !arrow->IsPrepared() )
	{
		m_preparedBowmanArrows[idx] = nullptr;
		return;
	}

	if ( !arrow->LaunchPrepared(speed, lifeSec, true) )
	{
		m_preparedBowmanArrows[idx] = nullptr;
		return;
	}

	m_preparedBowmanArrows[idx] = nullptr;
}

void CGameScene::RequestReleasePreparedArrow(CGameObject* shooter, float speed, float lifeSec)
{
    if (!shooter) return;

    const int slot = GetPlayerSlotFromObject(shooter);
    if (slot < 0 || slot > 3) return;

    CGameObject* arrowObj = m_preparedPlayerArrows[(size_t)slot];
    if (!arrowObj) return;

    auto* arrow = arrowObj->GetComponent<CArrowComponent>();
    if (!arrow || !arrow->IsPrepared())
    {
        m_preparedPlayerArrows[(size_t)slot] = nullptr;
        return;
    }

	if ( !arrow->LaunchPrepared(speed, lifeSec, true) )
	{
		m_preparedPlayerArrows[( size_t ) slot] = nullptr;
		return;
	}

	m_preparedPlayerArrows[( size_t ) slot] = nullptr;
}

void CGameScene::UpdatePreparedBowArrows()
{
	constexpr float kArrowSpeed = 16.0f;
	constexpr float kArrowLife = 6.0f;

	constexpr float kEnemyArrowPullBackDistance = 0.35f * 1.5f;
	constexpr float kEnemyArrowSpeed = 14.0f;
	constexpr float kEnemyArrowLife = 6.0f;

    for (int slot = 0; slot < 4; ++slot)
    {
        CGameObject* player = GetPlayerBySlot(slot);

        bool isBowLoad = false;
        bool isBowRelease = false;
        bool hasBowEquipped = false;

        if (player)
        {
            if (auto* equip = player->GetComponent<CPlayerEquipmentComponent>())
            {
                hasBowEquipped = (equip->GetEquippedWeapon() == EWeaponType::Bow);
            }

            if (auto* animComp = player->GetComponent<CAnimatorComponent>())
            {
                if (auto* ctrl = animComp->EnsureController())
                {
                    isBowLoad = ctrl->IsBowLoadPhase();
                    isBowRelease = ctrl->IsBowReleasePhase();
                }
            }
            else if (auto* ctrl = player->GetAnimController())
            {
                isBowLoad = ctrl->IsBowLoadPhase();
                isBowRelease = ctrl->IsBowReleasePhase();
            }
        }

		const size_t slotIndex = static_cast< size_t >( slot );

		if ( hasBowEquipped && isBowLoad && !m_prevBowLoadPhase[slotIndex] )
		{
			if ( auto* equip = player ? player->GetComponent<CPlayerEquipmentComponent>() : nullptr )
			{
				equip->RequestBowLoadingSfx();

				// 릴리즈 사운드는 릴리즈 phase에서 틀지 않고,
				// 로딩 phase 시작 시점에 미리 예약한다.
				equip->RequestBowReleaseSfxFromLoadPhase();
			}
		}

		if ( hasBowEquipped && isBowRelease && !m_prevBowReleasePhase[slotIndex] )
		{
			// 사운드는 이미 Bow_Load 진입 시 예약했으므로 여기서는 화살만 발사.
			RequestReleasePreparedArrow(player, kArrowSpeed, kArrowLife);
		}

		// 공격이 끝났거나 장비가 바뀌면 준비 화살 정리
		if ( ( !hasBowEquipped || ( !isBowLoad && !isBowRelease ) ) && m_preparedPlayerArrows[slotIndex] )
		{
			if ( auto* arrow = m_preparedPlayerArrows[slotIndex]->GetComponent<CArrowComponent>() )
			{
				arrow->Deactivate();
			}
			m_preparedPlayerArrows[slotIndex] = nullptr;
		}

		m_prevBowLoadPhase[slotIndex] = isBowLoad;
		m_prevBowReleasePhase[slotIndex] = isBowRelease;
    }
	for ( size_t i = 0; i < m_bowManRefs.size(); ++i )
	{
		CGameObject* bowman = m_bowManRefs[i];
		if ( IsMonsterDead(bowman) )
		{
			if ( i < m_preparedBowmanArrows.size() && m_preparedBowmanArrows[i] )
			{
				if ( auto* arrow = m_preparedBowmanArrows[i]->GetComponent<CArrowComponent>() )
					arrow->Deactivate();

				m_preparedBowmanArrows[i] = nullptr;
			}

			if ( i < m_prevEnemyBowReleasePhase.size() )
				m_prevEnemyBowReleasePhase[i] = false;

			continue;
		}

		bool isBowLoad = false;
		bool isBowRelease = false;

		if ( bowman )
		{
			if ( auto* animComp = bowman->GetComponent<CAnimatorComponent>() )
			{
				if ( auto* ctrl = animComp->EnsureMonsterController() )
				{
					isBowLoad = ctrl->IsAttackPrimaryPhase();   // Bow_Load
					isBowRelease = ctrl->IsAttackChainPhase();  // Bow_Release
				}
			}
		}

		// Bow_Load 상태이면 준비 화살 생성
		if ( isBowLoad )
		{
			if ( i < m_preparedBowmanArrows.size() && m_preparedBowmanArrows[i] == nullptr )
			{
				RequestPrepareBowmanArrow(bowman, kEnemyArrowPullBackDistance);
			}
		}

		// Bow_Release 진입 순간에만 발사
		if ( i < m_prevEnemyBowReleasePhase.size() )
		{
			if ( isBowRelease && !m_prevEnemyBowReleasePhase[i] )
			{
				RequestReleasePreparedBowmanArrow(bowman, kEnemyArrowSpeed, kEnemyArrowLife);
			}
		}

		// 공격이 끝났거나 다른 액션으로 빠지면 준비 화살 정리
		if ( ( !isBowLoad && !isBowRelease ) &&
			i < m_preparedBowmanArrows.size() &&
			m_preparedBowmanArrows[i] )
		{
			if ( auto* arrow = m_preparedBowmanArrows[i]->GetComponent<CArrowComponent>() )
			{
				arrow->Deactivate();
			}
			m_preparedBowmanArrows[i] = nullptr;
		}

		if ( i < m_prevEnemyBowReleasePhase.size() )
		{
			m_prevEnemyBowReleasePhase[i] = isBowRelease;
		}
	}
}


CGameObject* CGameScene::GetPlayerBySlot(int slot) const
{
    if (slot < 0 || slot > 3) return nullptr;
    return m_playersBySlot[(size_t)slot];
}

bool CGameScene::IsLocalPlayerInsideMegaGridCenter() const
{
	if ( !m_sceneGrid.IsInitialized() )
		return false;

	if ( m_localPlayerSlot < 0 ||
		 m_localPlayerSlot >= static_cast< int >(m_playerGridTrackers.size()) )
	{
		return false;
	}

	const GridDynamicTracker& tracker =
		m_playerGridTrackers[static_cast< size_t >(m_localPlayerSlot)];

	if ( !tracker.occupied )
		return false;

	int megaX = -1;
	int megaZ = -1;

	if ( !m_sceneGrid.FineCellToMegaGridCell(
		tracker.prevCellX,
		tracker.prevCellZ,
		megaX,
		megaZ) )
	{
		return false;
	}

	if ( megaX == kCastleCenterMegaGridX &&
		 megaZ == kCastleCenterMegaGridZ )
	{
#ifdef USING_NETWORK
		return true;
#else
		return m_bLocalPlayerInsideCastleCenterMegaGrid;
#endif
	}

	return m_sceneGrid.IsFineCellInsideMegaGridApproachZone(
		megaX,
		megaZ,
		tracker.prevCellX,
		tracker.prevCellZ
	);
}

int CGameScene::GetLocalPlayerMegaGridNumberForDepthFog() const
{
	if ( !m_sceneGrid.IsInitialized() )
		return -1;

	if ( m_localPlayerSlot < 0 ||
		 m_localPlayerSlot >= static_cast< int >(m_playerGridTrackers.size()) )
	{
		return -1;
	}

	const GridDynamicTracker& tracker =
		m_playerGridTrackers[static_cast< size_t >(m_localPlayerSlot)];

	if ( !tracker.occupied )
		return -1;

	return m_sceneGrid.MegaGridNumberFromCell(tracker.prevCellX, tracker.prevCellZ);
}

void CGameScene::UpdateDepthFogState(float dt)
{
	const int megaGridNumber = GetLocalPlayerMegaGridNumberForDepthFog();

	const bool isDenseFogZone =
		( megaGridNumber == 1 || megaGridNumber == 9 );

	const EDepthFogPresetMode fogMode =
		isDenseFogZone
		? EDepthFogPresetMode::ZoneDense
		: EDepthFogPresetMode::OuterWide;

	m_depthFog.UpdateState(dt, fogMode);
}

bool CGameScene::IsLocalPlayer(const CGameObject* obj) const
{
    if (!obj) return false;
    auto* tag = obj->GetComponent<CActorTagComponent>();
    return tag && tag->kind == EActorKind::Player && tag->control == EPlayerControl::Local;
}

void CGameScene::SetLocalPlayerControlEnabled(bool enabled)
{
	CGameObject* player = GetPlayer();
	if ( !player )
		return;

	if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
	{
		controller->SetInputEnabled(enabled);

		if ( !enabled )
		{
			controller->SetInputDirection(static_cast< DWORD >( 0 ));
			controller->SetRunRequested(false);
			controller->SetVelocity(XMFLOAT3(0.0f, 0.0f, 0.0f));
		}
	}
}

void CGameScene::CancelLocalPlayerPreparedActions()
{
	const int slot = m_localPlayerSlot;

	if ( slot < 0 || slot >= 4 )
		return;

	if ( m_preparedPlayerArrows[( size_t ) slot] )
	{
		if ( auto* arrow = m_preparedPlayerArrows[( size_t ) slot]->GetComponent<CArrowComponent>() )
			arrow->Deactivate();

		m_preparedPlayerArrows[( size_t ) slot] = nullptr;
	}

	m_prevBowReleasePhase[( size_t ) slot] = false;

	auto DisableColliderOnly = [ ] (CGameObject* obj)
		{
			if ( !obj )
				return;

			if ( auto* collider = obj->GetComponent<CColliderComponent>() )
				collider->SetEnabled(false);
		};

	if ( slot >= 0 && slot < ( int ) m_PlayerSwordRefs.size() )
		DisableColliderOnly(m_PlayerSwordRefs[( size_t ) slot]);

	if ( slot >= 0 && slot < ( int ) m_PlayerAxeRefs.size() )
		DisableColliderOnly(m_PlayerAxeRefs[( size_t ) slot]);

	if ( slot >= 0 && slot < ( int ) m_PlayerGunRefs.size() )
		DisableColliderOnly(m_PlayerGunRefs[( size_t ) slot]);
}

bool CGameScene::IsMonsterDead(const CGameObject* monster) const
{
	if ( !monster )
		return true;

	if ( m_deadMonsters.find(const_cast< CGameObject* >(monster)) != m_deadMonsters.end() )
		return true;

	if ( auto* hp = monster->GetComponent<CHealthComponent>() )
		return hp->IsDead();

	return false;
}

void CGameScene::MarkMegaGridClearedByNumber(int megaGridNumber)
{
	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return;

	const int zeroBased = megaGridNumber - 1;
	const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
	const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

	if ( m_sceneGrid.IsMegaGridCleared(megaX, megaZ) )
		return;

	m_sceneGrid.SetMegaGridCleared(megaX, megaZ, true);
}

bool CGameScene::AreAllMonstersInMegaGridDead(int megaGridNumber) const
{
	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return false;

	const int zeroBased = megaGridNumber - 1;
	const int megaX = zeroBased % CSceneGrid::kMegaGridCols;
	const int megaZ = zeroBased / CSceneGrid::kMegaGridCols;

	const std::vector<CGameObject*>& monsters =
		m_sceneGrid.GetMegaGridMonsters(megaX, megaZ);

	if ( monsters.empty() )
		return false;

	for ( const CGameObject* monster : monsters )
	{
		if ( !IsMonsterDead(monster) )
			return false;
	}

	return true;
}

void CGameScene::RegisterMutantKeyTriggerIfNeeded(
	CGameObject* mutant,
	int megaGridNumber)
{
	if ( !mutant )
		return;

	if ( megaGridNumber != 6 && megaGridNumber != 8 )
		return;

	if ( megaGridNumber < 1 || megaGridNumber > CSceneGrid::kMegaGridCount )
		return;

	if ( m_mutantKeyTriggerRegisteredByMega[( size_t ) megaGridNumber] )
		return;

	m_mutantKeyTriggerRegisteredByMega[( size_t ) megaGridNumber] = true;
	m_mutantKeyTriggerMegaByObject[mutant] = megaGridNumber;

	char buf[256];
	sprintf_s(
		buf,
		"[MutantKeyTrigger] Registered first Mutant for mega grid %d. mutant=%p\n",
		megaGridNumber,
		static_cast< void* >( mutant )
	);
	OutputDebugStringA(buf);
}

void CGameScene::UnlockKeyBillboardForMegaGrid(int megaGridNumber)
{
	if ( megaGridNumber != 6 && megaGridNumber != 8 )
		return;

	for ( ItemBillboardEntry& item : m_itemBillboards )
	{
		if ( item.kind != EItemBillboardKind::Key )
			continue;

		if ( item.megaGridNumber != megaGridNumber )
			continue;

		// 이미 먹은 열쇠는 다시 살리지 않는다.
		if ( m_sceneGrid.IsMegaGridCleared(
			( megaGridNumber - 1 ) % CSceneGrid::kMegaGridCols,
			( megaGridNumber - 1 ) / CSceneGrid::kMegaGridCols) )
		{
			return;
		}

		item.active = true;
		item.distanceCulled = false;

		char buf[256];
		sprintf_s(
			buf,
			"[MutantKeyTrigger] Key billboard unlocked for mega grid %d.\n",
			megaGridNumber
		);
		OutputDebugStringA(buf);

		return;
	}
}

void CGameScene::HandleMutantKeyTriggerDeath(CGameObject* monster)
{
	if ( !monster )
		return;

	const auto it = m_mutantKeyTriggerMegaByObject.find(monster);
	if ( it == m_mutantKeyTriggerMegaByObject.end() )
		return;

	const int megaGridNumber = it->second;

	UnlockKeyBillboardForMegaGrid(megaGridNumber);

	m_mutantKeyTriggerMegaByObject.erase(it);
}

void CGameScene::UpdateMegaGridClearStateFromMonsterDeaths()
{
	// 2번 메가그리드: 해당 메가그리드의 모든 몬스터 사망 시 클리어.
	if ( !m_sceneGrid.IsMegaGridCleared(1, 0) )
	{
		if ( AreAllMonstersInMegaGridDead(2) )
			MarkMegaGridClearedByNumber(2);
	}

	// 5번 메가그리드: 보스 사망 시 클리어.
	if ( !m_sceneGrid.IsMegaGridCleared(1, 1) )
	{
		for ( const CGameObject* boss : m_bossRefs )
		{
			if ( IsMonsterDead(boss) )
			{
				MarkMegaGridClearedByNumber(5);
				break;
			}
		}
	}
}

void CGameScene::CancelMonsterPreparedActions(CGameObject* monster)
{
	if ( !monster )
		return;

	// ---------------------------------------------------------------------
	// 1) BowMan이 Bow_Load 중 죽은 경우 준비 중인 화살 제거
	// ---------------------------------------------------------------------
	const int bowmanIndex = GetBowManIndexFromObject(monster);
	if ( bowmanIndex >= 0 )
	{
		const size_t idx = static_cast< size_t >( bowmanIndex );

		if ( idx < m_preparedBowmanArrows.size() )
		{
			if ( m_preparedBowmanArrows[idx] )
			{
				if ( auto* arrow = m_preparedBowmanArrows[idx]->GetComponent<CArrowComponent>() )
					arrow->Deactivate();

				m_preparedBowmanArrows[idx] = nullptr;
			}
		}

		if ( idx < m_prevEnemyBowReleasePhase.size() )
			m_prevEnemyBowReleasePhase[idx] = false;
	}

	// ---------------------------------------------------------------------
	// 2) SwordMan의 외부 무기 collider 비활성화
	//    m_swordManRefs와 m_EnemySwordRefs는 같은 순서로 연결되어 있음.
	// ---------------------------------------------------------------------
	for ( size_t i = 0; i < m_swordManRefs.size(); ++i )
	{
		if ( m_swordManRefs[i] != monster )
			continue;

		if ( i < m_EnemySwordRefs.size() )
		{
			CGameObject* sword = m_EnemySwordRefs[i];

			if ( sword )
			{
				if ( auto* collider = sword->GetComponent<CColliderComponent>() )
					collider->SetEnabled(false);

				if ( auto* hitbox = sword->GetComponent<CMonsterWeaponHitboxComponent>() )
					hitbox->SetEnabled(false);
			}
		}

		break;
	}

	// ---------------------------------------------------------------------
	// 3) Ghoul / Mutant / Boss처럼 owner bone weapon capsule을 쓰는 경우
	// ---------------------------------------------------------------------
	if ( auto* ownerWeaponHitbox = monster->GetComponent<CMonsterWeaponHitboxComponent>() )
		ownerWeaponHitbox->SetEnabled(false);
}

void CGameScene::BeginMonsterDeath(CGameObject* monster)
{
	if ( !monster )
		return;

	if ( m_deadMonsters.find(monster) != m_deadMonsters.end() )
		return;

	auto* tag = monster->GetComponent<CActorTagComponent>();
	if ( !tag || tag->kind != EActorKind::NPC )
		return;

	m_deadMonsters.insert(monster);

	HandleMutantKeyTriggerDeath(monster);

	CancelMonsterPreparedActions(monster);

	// 더 이상 플레이어 무기 충돌을 받지 않게 함.
	if ( auto* collider = monster->GetComponent<CColliderComponent>() )
		collider->SetEnabled(false);

	// 현재 실제로 붙는 AI는 CGhoulAIComponent지만,
	// base 타입으로도 잡히는 구조라면 같이 처리.
	if ( auto* ai = monster->GetComponent<CMonsterAIComponent>() )
	{
		ai->SetEnabledAI(false);
		ai->ClearTarget();
		ai->ClearPath();
	}

	if ( auto* ghoulAI = monster->GetComponent<CGhoulAIComponent>() )
	{
		ghoulAI->SetEnabledAI(false);
		ghoulAI->ClearTarget();
		ghoulAI->ClearPath();
	}

	if ( auto* animComp = monster->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->EnsureMonsterController() )
		{
			ctrl->SetLocomotionState(EMonsterAnimState::Idle);
			ctrl->RequestCommand(EMonsterAnimCommand::Death);
			return;
		}
	}
}

void CGameScene::UpdateMonsterDeathStates()
{
	for ( CGameObject* obj : m_skinnedBatch.objectRefs )
	{
		if ( !obj )
			continue;

		auto* tag = obj->GetComponent<CActorTagComponent>();
		if ( !tag || tag->kind != EActorKind::NPC )
			continue;

		auto* hp = obj->GetComponent<CHealthComponent>();
		if ( !hp )
			continue;

		if ( hp->IsDead() )
			BeginMonsterDeath(obj);
	}

	UpdateMegaGridClearStateFromMonsterDeaths();
}

void CGameScene::BeginLocalPlayerDeath(CGameObject* player)
{
	if ( !player )
		return;

	if ( m_bLocalPlayerDead )
		return;

	m_bLocalPlayerDead = true;
	m_localPlayerRespawnTimer = 0.0f;

	SetLocalPlayerControlEnabled(false);
	CancelLocalPlayerPreparedActions();

	if ( auto* collider = player->GetComponent<CColliderComponent>() )
		collider->SetEnabled(false);

	if ( auto* animComp = player->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->EnsureController() )
		{
			ctrl->RequestDeath();
			return;
		}
	}

	if ( auto* ctrl = player->GetAnimController() )
		ctrl->RequestDeath();
}

void CGameScene::RespawnLocalPlayer(CGameObject* player)
{
	if ( !player )
		return;

	player->SetPosition(kLocalPlayerRespawnPosition);

	if ( auto* hp = player->GetComponent<CHealthComponent>() )
		hp->ResetToMax();

	if ( auto* collider = player->GetComponent<CColliderComponent>() )
	{
		collider->SetEnabled(true);
		collider->UpdateWorldBounds();
	}

	if ( auto* animComp = player->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->EnsureController() )
		{
			ctrl->ResetToIdleAfterRespawn();
		}
	}
	else if ( auto* ctrl = player->GetAnimController() )
	{
		ctrl->ResetToIdleAfterRespawn();
	}

	SetLocalPlayerControlEnabled(true);

	m_bLocalPlayerDead = false;
	m_bLocalPlayerRespawnUsed = true;
	m_localPlayerRespawnTimer = 0.0f;

	UpdateDynamicGridState();
}

void CGameScene::UpdateLocalPlayerDeathAndRespawn(float dt)
{
	CGameObject* player = GetPlayer();
	if ( !player )
		return;

	auto* hp = player->GetComponent<CHealthComponent>();
	if ( !hp )
		return;

	if ( !m_bLocalPlayerDead && hp->IsDead() )
	{
		BeginLocalPlayerDeath(player);
	}

	if ( !m_bLocalPlayerDead )
		return;

	if ( m_bLocalPlayerRespawnUsed )
		return;

	if ( dt > 0.0f )
		m_localPlayerRespawnTimer += dt;

	if ( m_localPlayerRespawnTimer >= kLocalPlayerRespawnDelay )
	{
		RespawnLocalPlayer(player);
	}
}

bool CGameScene::RollbackLocalPlayerMoveIfCollidingWorldStatic(const XMFLOAT3& previousPos)
{
	CGameObject* localPlayer = GetPlayer();
	if ( !localPlayer ) return false;
	if ( !m_Collision ) return false;

	auto* collider = localPlayer->GetComponent<CColliderComponent>();
	if ( !collider ) return false;

	const XMFLOAT3 currentPos = localPlayer->GetPosition();

	auto TestPositionAgainstWorldStatic = [ this, localPlayer, collider ] (const XMFLOAT3& testPos) -> bool
		{
			localPlayer->SetPosition(testPos);
			collider->UpdateWorldBounds();
			return m_Collision->HasCollisionWithWorldStatic(collider);
		};

	const bool hitWorldStaticAtCurrentPos = TestPositionAgainstWorldStatic(currentPos);

	// 현재 위치가 애초에 안 겹치면 아무 것도 안 함
	if ( !hitWorldStaticAtCurrentPos )
	{
#ifndef USING_NETWORK
		if ( kEnableTowerDoorPortalVerboseLog )
		{
			char buf[256];
			sprintf_s(
				buf,
				"[TowerDoorPortal][ROLLBACK_CHECK] no world-static hit pos=(%.3f, %.3f, %.3f)\n",
				currentPos.x,
				currentPos.y,
				currentPos.z
			);
			OutputDebugStringA(buf);
		}
#endif
		return false;
	}

#ifndef USING_NETWORK
	if ( kEnableTowerDoorPortalCollisionLog )
	{
		char buf[512];
		sprintf_s(
			buf,
			"[TowerDoorPortal][WORLD_STATIC_HIT] pos=(%.3f, %.3f, %.3f) prev=(%.3f, %.3f, %.3f) portalCount=%zu\n",
			currentPos.x,
			currentPos.y,
			currentPos.z,
			previousPos.x,
			previousPos.y,
			previousPos.z,
			m_towerDoorPortals.size()
		);
		OutputDebugStringA(buf);
	}

	localPlayer->SetPosition(currentPos);
	collider->UpdateWorldBounds();

	if ( TryTeleportLocalPlayerByTowerDoorPortal(true) )
		return true;

	if ( TryTeleportLocalPlayerByCastleDoorPortal(true) )
		return true;
#endif

#ifndef USING_NETWORK
	if ( IsTowerDoorPortalOnCooldown() )
	{
		if ( kEnableTowerDoorPortalCollisionLog )
		{
			OutputDebugStringA(
				"[DoorPortal][ROLLBACK_SKIP_DURING_COOLDOWN] skip normal world-static rollback after portal teleport\n"
			);
		}

		localPlayer->SetPosition(currentPos);
		collider->UpdateWorldBounds();
		return true;
	}
#endif

	// 후보 1: X만 롤백 (Z 이동은 살림)
	XMFLOAT3 candidateRollbackX = currentPos;
	candidateRollbackX.x = previousPos.x;

	// 후보 2: Z만 롤백 (X 이동은 살림)
	XMFLOAT3 candidateRollbackZ = currentPos;
	candidateRollbackZ.z = previousPos.z;

	const bool xResolved = !TestPositionAgainstWorldStatic(candidateRollbackX);
	const bool zResolved = !TestPositionAgainstWorldStatic(candidateRollbackZ);

	if ( xResolved && zResolved )
	{
		const float keepDistSqX =
			( candidateRollbackX.x - previousPos.x ) * ( candidateRollbackX.x - previousPos.x ) +
			( candidateRollbackX.z - previousPos.z ) * ( candidateRollbackX.z - previousPos.z );

		const float keepDistSqZ =
			( candidateRollbackZ.x - previousPos.x ) * ( candidateRollbackZ.x - previousPos.x ) +
			( candidateRollbackZ.z - previousPos.z ) * ( candidateRollbackZ.z - previousPos.z );

		const XMFLOAT3 resolvedPos = ( keepDistSqX >= keepDistSqZ ) ? candidateRollbackX : candidateRollbackZ;
		localPlayer->SetPosition(resolvedPos);
		collider->UpdateWorldBounds();
		return true;
	}

	if ( xResolved )
	{
		localPlayer->SetPosition(candidateRollbackX);
		collider->UpdateWorldBounds();
		return true;
	}

	if ( zResolved )
	{
		localPlayer->SetPosition(candidateRollbackZ);
		collider->UpdateWorldBounds();
		return true;
	}

	// 둘 다 안 되면 전체 롤백
	localPlayer->SetPosition(previousPos);
	collider->UpdateWorldBounds();
	return true;
}

bool CGameScene::OnProcessingMouseMessage(HWND /*hWnd*/, UINT msg, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	if ( msg == WM_LBUTTONDOWN )
	{
#ifndef USING_NETWORK
		if ( !m_bLocalPlayerDead )
			RequestPlayerAttackBySlot(m_localPlayerSlot);
#endif
		return true;
	}
	return false;
}

bool CGameScene::OnProcessingKeyboardMessage(HWND /*hWnd*/, UINT msg, WPARAM wParam, LPARAM /*lParam*/)
{
	return false;
}

void CGameScene::RequestFireArrow(CGameObject* shooter, float speed, float lifeSec, float yOffset)
{
    if (!shooter) return;

    CGameObject* bowObj = nullptr;

    if (auto* equip = shooter->GetComponent<CPlayerEquipmentComponent>())
    {
        // 장비 컴포넌트가 있는 플레이어라면 Bow 장착시에만 발사 허용
        if (equip->GetEquippedWeapon() != EWeaponType::Bow)
            return;

        bowObj = equip->GetWeaponObject(EWeaponType::Bow);
    }

    // 방향/회전은 기존 로직 유지: shooter의 forward 사용
    const XMFLOAT4X4& W = shooter->GetWorldMatrix();
    XMFLOAT3 dir = { W._31, W._32, W._33 };

    XMVECTOR dirV = XMLoadFloat3(&dir);
    const float lenSq = XMVectorGetX(XMVector3LengthSq(dirV));
    if (lenSq < 1e-8f)
    {
        dir = XMFLOAT3(0.0f, 0.0f, 1.0f);
        dirV = XMLoadFloat3(&dir);
    }

    dirV = XMVector3Normalize(dirV);

    XMFLOAT3 dirN{};
    XMStoreFloat3(&dirN, dirV);

    // 시작 위치는 활 위치 사용, 없으면 기존 fallback
    XMFLOAT3 startPos{};
    if (bowObj)
    {
        startPos = bowObj->GetPosition();
    }
    else
    {
        startPos = shooter->GetPosition();
        startPos.y += yOffset;
    }

    const XMFLOAT3 vel =
    {
        dirN.x * speed,
        dirN.y * speed,
        dirN.z * speed
    };

    for (CGameObject* arrowObj : m_arrowRefs)
    {
        if (!arrowObj) continue;

        auto* arrow = arrowObj->GetComponent<CArrowComponent>();
        if (!arrow) continue;

        if (arrow->IsActive()) continue;

		arrow->Activate(startPos, vel, lifeSec, true);
		return;
    }
}

void CGameScene::RequestFireBullet(CGameObject* shooter, float speed, float lifeSec)
{
	if ( !shooter ) return;

	CGameObject* gunObj = nullptr;

	if ( auto* equip = shooter->GetComponent<CPlayerEquipmentComponent>() )
	{
		if ( equip->GetEquippedWeapon() != EWeaponType::Gun )
			return;

		gunObj = equip->GetWeaponObject(EWeaponType::Gun);
	}

	CGameObject* spawnSource = gunObj ? gunObj : shooter;
	CGameObject* directionSource = shooter;

	for ( CGameObject* bulletObj : m_bulletRefs )
	{
		if ( !bulletObj ) continue;

		auto* bullet = bulletObj->GetComponent<CBulletComponent>();
		if ( !bullet ) continue;
		if ( bullet->IsActive() ) continue;

		SetObjectAttackPower(bulletObj, kAttackPowerPlayerBullet);

		if ( bullet->FireFromObjects(spawnSource, directionSource, speed, lifeSec, true) )
		{
			const XMFLOAT3 dirN = GetSafeObjectForward(directionSource);

			XMFLOAT3 muzzlePos = spawnSource->GetPosition();

			if ( gunObj )
			{
				muzzlePos.x += dirN.x * 0.55f;
				muzzlePos.y += 0.05f;
				muzzlePos.z += dirN.z * 0.55f;
			}
			else
			{
				muzzlePos.y += 1.15f;
				muzzlePos.x += dirN.x * 0.85f;
				muzzlePos.z += dirN.z * 0.85f;
			}

			SpawnMuzzleFlash(muzzlePos, dirN);

			return;
		}
	}
}

bool CGameScene::ProcessInput(UCHAR* /*pKeysBuffer*/)
{
	return false;
}

bool CGameScene::ShouldEvaluateSkinnedPoseThisFrame(UINT objectIndex, CCamera* camera) const
{
	if ( objectIndex >= static_cast< UINT >( m_skinnedBatch.objectRefs.size() ) )
		return false;

	CGameObject* obj = m_skinnedBatch.objectRefs[objectIndex];

	if ( !obj )
		return false;

	// 비네트워크 모드는 클라이언트가 실제 판정도 하므로 보수적으로 full pose 유지.
#ifndef USING_NETWORK
	return true;
#else
	// 로컬 플레이어는 항상 full pose 유지.
	if ( IsLocalPlayer(obj) )
		return true;

	auto* renderer = obj->GetComponent<CSkinnedMeshRendererComponent>();

	if ( !renderer || !renderer->IsEnabled() )
		return false;

	const bool distanceCulled =
		( objectIndex < static_cast< UINT >(m_skinnedDistanceCullFlags.size()) ) &&
		( m_skinnedDistanceCullFlags[objectIndex] != 0 );

	if ( distanceCulled )
		return false;

	const bool occlusionCulled =
		( objectIndex < static_cast< UINT >(m_skinnedOcclusionCullFlags.size()) ) &&
		( m_skinnedOcclusionCullFlags[objectIndex] != 0 );

	const bool frustumVisible =
		( camera == nullptr ) || obj->IsVisible(camera);

	const bool sceneVisible =
		!occlusionCulled && frustumVisible;

	// Shadow pass에서도 렌더될 수 있으면 pose를 갱신한다.
	// 이미 RenderSkinnedInstanceGroupsToShadowMap()도 distance cull +
	// IsSkinnedObjectInsideShadowBox() 기준으로 걸러낸다.
	const bool shadowVisible =
		IsSkinnedObjectInsideShadowBox(objectIndex);

	return sceneVisible || shadowVisible;
#endif
}

void CGameScene::AnimateObjects(float dt)
{
	m_fElapsedTime = dt;

	UpdateMuzzleFlashes(dt);
	UpdateSwordTrails(dt);

	UpdateMonsterDeathStates();

#ifndef USING_NETWORK
	UpdateLocalPlayerDeathAndRespawn(dt);
#endif

    // ------------------------------------------------------------------------
    // FrameSnapshot에서 좌표 업데이트
    // ------------------------------------------------------------------------

#ifdef USING_NETWORK
    DequeueNetworkMessage(NetworkMessageType::FrameState);
    if (std::holds_alternative<FrameSnapshot>(m_pendingNetworkMessage.data))
    {
        const FrameSnapshot& snapshot = std::get<FrameSnapshot>(m_pendingNetworkMessage.data);

        // Player 좌표 업데이트
        for (const auto& state : snapshot.players)
        {
            // id를 slot으로 사용 (0~3)
            int slot = static_cast<int>(state.id);
            CGameObject* player = GetPlayerBySlot(slot);
            if (!player) continue;


            // 로컬 플레이어는 서버 좌표로 덮어쓰지 않음 (선택적)
            // if (slot == m_localPlayerSlot) continue;

            player->SetPosition(state.position.x, state.position.y, state.position.z);

            // yaw 회전 적용
            if (auto* tr = player->GetComponent<CTransformComponent>())
            {
                tr->SetYawDegrees(state.yaw);
            } 

			if ( slot == m_localPlayerSlot )
			{
				if ( auto* controller = player->GetComponent<CPlayerControllerComponent>() )
					controller->SetYawDegrees(state.yaw);
			}
			
			if ( auto wc = player->GetComponent<CPlayerEquipmentComponent>() )
			{
				wc->SetLoadout(state.weaponType);
			}


			if ( slot == m_localPlayerSlot )
			{
				// 로컬 플레이어로 카메라 동기화
				auto pCamera = GetMainCamera();
				if ( pCamera )
				{
					XMFLOAT3 pos = player->GetPosition();
					pos.y += 1.7f; // 카메라 높이 보정 (플레이어 중심에서 약간 위)
					pCamera->Update(pos, dt);
					pCamera->SetLookAt(pos);
					pCamera->RegenerateViewMatrix();
				}
			}

            // 데모: animation state 강제 적용
            if (auto ac = player->GetAnimController())
            {
                const DecodedAnimStateCode decoded = DecodeStateCode(state.animation.stateCode);
				const auto prevIt = m_prevPlayerNetworkStateCode.find(state.id);
				const uint32_t prevStateCode =
					( prevIt != m_prevPlayerNetworkStateCode.end() ) ? prevIt->second : 0u;
				const DecodedAnimStateCode prevDecoded = DecodeStateCode(prevStateCode);

				if ( prevDecoded.die && !decoded.die )
				{
					ac->ResetToIdleAfterRespawn();

					if ( slot == m_localPlayerSlot )
					{
						m_bLocalPlayerDead = false;
						m_bLocalPlayerRespawnUsed = false;
						m_localPlayerRespawnTimer = 0.0f;

						if ( auto* camera = GetMainCamera() )
						{
							camera->GetYaw() = state.yaw;

							XMFLOAT3 cameraTarget = player->GetPosition();
							cameraTarget.y += 1.7f;
							camera->Update(cameraTarget, 0.0f);
							camera->SetLookAt(cameraTarget);
							camera->RegenerateViewMatrix();
						}
					}
				}

                ac->SetMoveDirection(decoded.hasMove ? decoded.moveDirBits : 0u);
                ac->SetRunRequested(decoded.run);

                if (decoded.die)
                {
					if ( slot == m_localPlayerSlot )
						m_bLocalPlayerDead = true;
					ac->RequestDeath();
                    ac->SetAnimState(EAnimState::Die);
					m_prevPlayerNetworkStateCode[state.id] = state.animation.stateCode;
					continue;
                }

				else if ( decoded.hit )
				{
					if ( !prevDecoded.hit )
					{
						SpawnBloodSplash(player, nullptr, nullptr);
					}

					ac->RequestHit();
					m_prevPlayerNetworkStateCode[state.id] = state.animation.stateCode;
					continue;
				}

				else if ( decoded.roll )
				{
					uint32_t rollDirBits = decoded.hasMove ? decoded.moveDirBits : DIR_FORWARD;

					if (!prevDecoded.roll && slot != m_localPlayerSlot)
					{
						if ( auto* equip = player->GetComponent<CPlayerEquipmentComponent>() )
							equip->RequestRollSfx(rollDirBits);
					}

					ac->RequestRoll(rollDirBits);
					ac->SetAnimState(EAnimState::Attack);

					m_prevPlayerNetworkStateCode[state.id] = state.animation.stateCode;
					continue;
				}
				else if ( decoded.attack )
				{
					if ( !prevDecoded.attack )
					{
						RequestPlayerAttackSfx(player);
					}

					ac->RequestAttack();
					m_prevPlayerNetworkStateCode[state.id] = state.animation.stateCode;
					continue;
				}
                else
                {
                    ac->SetAnimState(decoded.hasMove ? EAnimState::Move : EAnimState::Idle);
                }

				m_prevPlayerNetworkStateCode[state.id] = state.animation.stateCode;
            }
        }


		// Enemy 좌표 업데이트
		// 1) NPC 인덱스 → 오브젝트 매핑 구축
		std::unordered_map<uint64_t, CGameObject*> npcById;
		{
			UINT npcIndex = 0;
			for ( UINT j = 0; j < ( UINT ) m_skinnedObjects.size(); ++j )
			{
				auto* obj = m_skinnedObjects[j].get();
				if ( !obj ) continue;
				auto* tag = obj->GetComponent<CActorTagComponent>();
				if ( !tag || tag->kind != EActorKind::NPC ) continue;
				npcById[npcIndex] = obj;
				++npcIndex;
			}
		}

		// 2) snapshot enemy를 ID 기준으로 적용
		for ( const auto& state : snapshot.enemies )
		{
			auto it = npcById.find(state.id);
			if ( it == npcById.end() ) continue;

			auto* obj = it->second;
			obj->SetPosition(state.position.x, state.position.y, state.position.z);

			if ( auto* tr = obj->GetComponent<CTransformComponent>() )
				tr->SetYawDegrees(state.yaw);

			if ( auto* animComp = obj->GetComponent<CAnimatorComponent>() )
			{
				if ( auto* ctrl = animComp->EnsureMonsterController() )
				{
					const DecodedAnimStateCode decoded = DecodeStateCode(state.animation.stateCode);

					EMonsterAnimState locomotionState = EMonsterAnimState::Idle;
					if ( decoded.hasMove )
						locomotionState = decoded.run ? EMonsterAnimState::Run : EMonsterAnimState::Move;

					ctrl->SetLocomotionState(locomotionState);

					static std::unordered_map<uint64_t, uint32_t> s_prevEnemyStateCode;
					const uint32_t prevStateCode =
						( s_prevEnemyStateCode.find(state.id) != s_prevEnemyStateCode.end() )
						? s_prevEnemyStateCode[state.id]
						: 0u;
					const DecodedAnimStateCode prevDecoded = DecodeStateCode(prevStateCode);

					if ( decoded.die && !prevDecoded.die )
					{
						ctrl->RequestCommand(EMonsterAnimCommand::Death);
					}

					else if ( decoded.hit && !prevDecoded.hit )
					{
						ctrl->RequestCommand(EMonsterAnimCommand::Hit);

						SpawnBloodSplash(obj, nullptr, nullptr);

						if ( auto* hp = obj->GetComponent<CHealthComponent>() )
							hp->RequestHitSfx();
					}

					else if ( decoded.attack && !prevDecoded.attack )
					{
						ctrl->RequestCommand(EMonsterAnimCommand::Attack);
					}

					s_prevEnemyStateCode[state.id] = state.animation.stateCode;
					ctrl->Update(0.0f);
				}
			}
		}

		// Projectile 동기화
		size_t usedArrowCount = 0;
		size_t usedBulletCount = 0;

		for (const auto& b : snapshot.bullets)
		{
			const bool isArrow = (b.bulletType == 1u); // Protocol::BULLET_TYPE_ARROW

			if (isArrow)
			{
				if (usedArrowCount >= m_arrowRefs.size())
					continue;

				CGameObject* arrowObj = m_arrowRefs[usedArrowCount++];
				if (!arrowObj) continue;

				if ( auto* arrow = arrowObj->GetComponent<CArrowComponent>() )
				{
					arrow->Activate(b.position, b.velocity, 2.0f);
					auto arrowtransform = arrowObj->GetComponent<CTransformComponent>();
					arrowtransform->SetLookDirection(b.velocity);
				}
			}
			else
			{
				if (usedBulletCount >= m_bulletRefs.size())
					continue;

				CGameObject* bulletObj = m_bulletRefs[usedBulletCount++];
				if (!bulletObj) continue;

				if ( auto* bullet = bulletObj->GetComponent<CBulletComponent>() )
				{
					bullet->Activate(b.position, b.velocity, 2.0f, true);
				}
			}
		}

		for (size_t i = usedArrowCount; i < m_arrowRefs.size(); ++i)
		{
			if (!m_arrowRefs[i]) continue;
			if (auto* arrow = m_arrowRefs[i]->GetComponent<CArrowComponent>())
				arrow->Deactivate();
		}

		for (size_t i = usedBulletCount; i < m_bulletRefs.size(); ++i)
		{
			if (!m_bulletRefs[i]) continue;
			if (auto* bullet = m_bulletRefs[i]->GetComponent<CBulletComponent>())
				bullet->Deactivate();
		}

		// 사용이 끝난 data는 기본값으로 초기화 (선택적)
		m_pendingNetworkMessage.data = LoadoutData{};
    }
	else
	{
		// 이전 state code를 따라가면서 그때의 애니메이션을 유지
		for ( const auto& [id, stateCode] : m_prevPlayerNetworkStateCode )
		{
			CGameObject* player = GetPlayerBySlot(static_cast< int >( id ));
			if ( !player ) continue;
			if ( auto* ac = player->GetAnimController() )
			{
				const DecodedAnimStateCode decoded = DecodeStateCode(stateCode);
				if ( decoded.die )
					ac->SetAnimState(EAnimState::Die);
				else if ( decoded.hit )
					ac->SetAnimState(EAnimState::Hit);
				else if ( decoded.roll )
					ac->SetAnimState(EAnimState::Attack);
				else if ( decoded.attack )
					ac->SetAnimState(EAnimState::Attack);
				else
					ac->SetAnimState(decoded.hasMove ? EAnimState::Move : EAnimState::Idle);
			}
		}
	}
#endif

	CCamera* camera = GetMainCamera();

#ifndef USING_NETWORK
	const int activeMonsterMegaGridNumber =
		GetLocalPlayerMegaGridNumberForMonsterTick();
#else
	const int activeMonsterMegaGridNumber = -1;
#endif

	for ( UINT j = 0; j < static_cast< UINT >(m_skinnedObjects.size()); ++j )
	{
		if ( !m_skinnedObjects[j] )
			continue;

		CGameObject* obj = m_skinnedObjects[j].get();

#ifndef USING_NETWORK
		if ( ShouldSkipMonsterByMegaGrid(obj, j, activeMonsterMegaGridNumber) )
		{
			if ( auto* animComp = obj->GetComponent<CAnimatorComponent>() )
				animComp->SetPoseEvaluationEnabled(false);

			continue;
		}
#endif

#ifdef USING_NETWORK
		const bool shouldEvaluatePose =
			ShouldEvaluateSkinnedPoseThisFrame(j, camera);
#else
		const bool shouldEvaluatePose = true;
#endif

		if ( auto* animComp = obj->GetComponent<CAnimatorComponent>() )
		{
			animComp->SetPoseEvaluationEnabled(shouldEvaluatePose);
		}

		obj->Animate(dt);
	}

	UpdateDynamicGridState();
	UpdatePlayerFootstepSfx();

	for ( UINT j = 0; j < ( UINT ) m_staticObjects.size(); ++j )
	{
		if ( !m_staticObjects[j] )
			continue;

		if ( j < ( UINT ) m_staticDistanceCullFlags.size() )
		{
			if ( m_staticDistanceCullFlags[j] != 0 )
				continue;
		}

		if ( j < ( UINT ) m_staticOcclusionCullFlags.size() )
		{
			if ( m_staticOcclusionCullFlags[j] != 0 )
				continue;
		}

		if ( j < ( UINT ) m_staticTreeGridCullFlags.size() )
		{
			if ( m_staticTreeGridCullFlags[j] != 0 )
				continue;
		}

		m_staticObjects[j]->Animate(dt);
	}
#ifdef USING_NETWORK
	UpdatePlayerBowSfxOnly();
#else
	UpdatePreparedBowArrows();
#endif

	for ( UINT j = 0; j < ( UINT ) m_lightObjects.size(); ++j )
	{
		if ( !m_lightObjects[j] )
			continue;

		m_lightObjects[j]->Animate(dt);
	}
}

void CGameScene::CollisionObjects()
{
	if ( !m_Collision ) return;

	m_Collision->OnUpdateFiltered(
		[ this ](
			const CColliderComponent* a,
			const CColliderComponent* b) -> bool
		{
			return ShouldKeepCollisionPairByMegaGrid(a, b);
		}
	);

	// 아이템 빌보드는 CGameObject/Collider가 아니므로 별도 overlap 판정.
	UpdateItemBillboardPickupCollision();

#ifndef USING_NETWORK
	TickTowerDoorPortalCooldowns();
	TryTeleportLocalPlayerByTowerDoorPortal();
#endif

	UpdateMonsterDeathStates();

#ifndef USING_NETWORK
	UpdateLocalPlayerDeathAndRespawn(0.0f);
#endif
}

void CGameScene::UpdateShaderVariables(ID3D12GraphicsCommandList* /*cmd*/)
{
	PROFILE_RENDER_SCOPE("GameScene::UpdateShaderVariables");
    if (m_pcbMappedLights)
    {
        ::ZeroMemory(m_pcbMappedLights, sizeof(LIGHTS));
		m_pcbMappedLights->m_xmf4GlobalAmbient = XMFLOAT4(0.20f, 0.20f, 0.20f, 1.0f);

        UINT li = 0;
        for (auto& obj : m_lightObjects)
        {
            if (!obj) continue;

            auto* lc = obj->GetComponent<CLightComponent>();
            if (!lc) continue;
            if (!lc->IsEnabled()) continue;
            if (li >= MAX_LIGHTS) break;

            lc->Fill(m_pcbMappedLights->m_pLights[li]);
            ++li;
        }
    }

    if (m_pcbMappedMaterials && m_pMaterials)
        ::memcpy(m_pcbMappedMaterials, m_pMaterials.get(), sizeof(MATERIALS));
	
	CGameObject* shadowFocus = GetPlayer();
	if ( !shadowFocus )
		shadowFocus = GetPlayerBySlot(0);

	CGameObject* directionalLightObj = nullptr;

	for ( const auto& lightObj : m_lightObjects )
	{
		if ( !lightObj )
			continue;

		auto* lc = lightObj->GetComponent<CLightComponent>();
		if ( !lc )
			continue;

		if ( lc->type == ELightType::Directional )
		{
			directionalLightObj = lightObj.get();
			break;
		}
	}

	m_shadowMap.UpdateData(shadowFocus, directionalLightObj);
	m_shadowMap.UploadConstantBuffer();

	UpdateDepthFogState(m_fElapsedTime);
	m_depthFog.UploadConstantBuffer();

	{
		float hpRatio = 1.0f;

		CGameObject* localPlayer = GetPlayer();
		if ( !localPlayer )
			localPlayer = GetPlayerBySlot(0);

		if ( localPlayer )
		{
			if ( auto* hp = localPlayer->GetComponent<CHealthComponent>() )
				hpRatio = hp->GetHpRatio();
		}

		m_hud.SetHealthRatio(hpRatio);
	}

    if (m_staticBatch.mappedGameObjects && !m_staticBatch.objectRefs.empty())
    {
        const UINT ncb = m_staticBatch.cbElementBytes;

        for (UINT j = 0; j < (UINT)m_staticBatch.objectRefs.size(); ++j)
        {
            auto* obj = m_staticBatch.objectRefs[j];
            if (!obj) continue;

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)m_staticBatch.mappedGameObjects + j * ncb);

            const XMFLOAT4X4& W = obj->GetWorldMatrix();

            XMStoreFloat4x4(
                &cb->m_xmf4x4World,
                XMMatrixTranspose(XMLoadFloat4x4(&W))
            );

            cb->m_nObjectID = j;
        }
    }

    if (m_skinnedBatch.mappedGameObjects && !m_skinnedBatch.objectRefs.empty())
    {
        const UINT ncb = m_skinnedBatch.cbElementBytes;

        for (UINT j = 0; j < (UINT)m_skinnedBatch.objectRefs.size(); ++j)
        {
            auto* obj = m_skinnedBatch.objectRefs[j];
            if (!obj) continue;

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)m_skinnedBatch.mappedGameObjects + j * ncb);

            const XMFLOAT4X4& W = obj->GetWorldMatrix();

            XMStoreFloat4x4(
                &cb->m_xmf4x4World,
                XMMatrixTranspose(XMLoadFloat4x4(&W))
            );

            cb->m_nObjectID = j;
        }
    }

	if ( m_colliderBatch.mappedGameObjects && !m_colliderBatch.objectRefs.empty() )
	{
		const UINT ncb = m_colliderBatch.cbElementBytes;

		for ( UINT j = 0; j < ( UINT ) m_colliderBatch.objectRefs.size(); ++j )
		{
			auto* obj = m_colliderBatch.objectRefs[j];
			if ( !obj ) continue;

			auto* cb = ( CB_GAMEOBJECT_INFO* ) ( ( UINT8* ) m_colliderBatch.mappedGameObjects + j * ncb );

			const XMFLOAT4X4& W = obj->GetWorldMatrix();

			XMStoreFloat4x4(
				&cb->m_xmf4x4World,
				XMMatrixTranspose(XMLoadFloat4x4(&W))
			);

			cb->m_nObjectID = j;
		}
	}
}

void CGameScene::OnPrepareRender(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::OnPrepareRender");

	CScene::OnPrepareRender(cmd, camera);

	UpdateFrameRenderState(camera);
	UpdateShaderVariables(cmd);
	BindFrameRootParameters(cmd);
}

void CGameScene::UpdateFrameRenderState(CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::UpdateFrameRenderState");

	if ( !camera )
		return;

	UpdateStaticWorldLodSelection(camera);
	BeginStaticOcclusionReadback();
	UpdateStaticOcclusionCullSelection(camera);
	UpdateStaticTreeGridCullSelection(camera);

	BuildStaticVisibleListsForFrame(camera);

	UpdateItemBillboardDistanceCullSelection(camera);

	UpdateSkinnedWorldLodSelection(camera);
	BeginSkinnedOcclusionReadback();
	UpdateSkinnedOcclusionCullSelection(camera);
}

void CGameScene::BindFrameRootParameters(ID3D12GraphicsCommandList* cmd)
{
	PROFILE_RENDER_SCOPE("GameScene::BindFrameRootParameters");

	if ( !cmd )
		return;

	if ( m_pd3dcbLights )
	{
		cmd->SetGraphicsRootConstantBufferView(
			ROOT_PARAMETER_LIGHT,
			m_pd3dcbLights->GetGPUVirtualAddress()
		);
	}

	if ( m_pd3dcbMaterials )
	{
		cmd->SetGraphicsRootConstantBufferView(
			ROOT_PARAMETER_MATERIAL,
			m_pd3dcbMaterials->GetGPUVirtualAddress()
		);
	}

	m_depthFog.BindConstantBuffer(cmd);
	m_shadowMap.BindConstantBuffer(cmd);
}

void CGameScene::RebindFrameRenderState(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::RebindFrameRenderState");

	if ( !cmd )
		return;

	CScene::OnPrepareRender(cmd, camera);
	BindFrameRootParameters(cmd);
}

void CGameScene::RenderSceneGeometry(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::RenderSceneGeometry(total)");

	if ( !m_bStartedGameplayMusic )
	{
		PROFILE_RENDER_SCOPE("GameScene::RenderSceneGeometry::StartGameplayMusic");

		if ( m_pAudioManager )
		{
			if ( auto* music = m_pAudioManager->GetMusicDirector() )
			{
				music->RequestState(EMusicState::Gameplay, false);
				music->BeginPendingTransition();
			}
		}

		m_bStartedGameplayMusic = true;
	}

	if ( m_staticBatch.shader )
	{
		PROFILE_RENDER_SCOPE("GameScene::RenderSceneGeometry::StaticInstances");
		RenderStaticInstanceGroups(cmd, camera);
	}

	if ( m_itemBillboardShader )
	{
		RenderItemBillboards(cmd, camera);
	}

	if ( m_skinnedBatch.shader )
	{
		m_skinnedBatch.shader->Render(cmd, camera, &m_skinnedBatch);
		RenderSkinnedInstanceGroups(cmd, camera);
	}

	{
		PROFILE_RENDER_SCOPE("GameScene::RenderSceneGeometry::StaticOcclusionPass");
		RenderStaticOcclusionPass(cmd, camera);
	}

	{
		PROFILE_RENDER_SCOPE("GameScene::RenderSceneGeometry::SkinnedOcclusionPass");
		RenderSkinnedOcclusionPass(cmd, camera);
	}

#ifndef USING_NETWORK
	if ( m_colliderBatch.shader )
	{
		m_colliderBatch.shader->Render(cmd, camera, &m_colliderBatch);
		for ( UINT j = 0; j < ( UINT ) m_colliderObjects.size(); ++j )
		{
			if ( !m_colliderObjects[j] ) continue;
			m_colliderObjects[j]->Render(cmd, camera);
		}
	}
#endif

	{
		const bool isInsideMegaGridCenter = IsLocalPlayerInsideMegaGridCenter();

		if ( isInsideMegaGridCenter != m_bWasLocalPlayerInsideMegaGridCenter )
		{
			if ( m_pAudioManager )
			{
				if ( auto* music = m_pAudioManager->GetMusicDirector() )
				{
					music->SetCrossFadeSeconds(1.0f);

					if ( isInsideMegaGridCenter )
					{
						OutputDebugStringA("[MegaGridBGM] local player entered center zone\n");
						music->RequestState(EMusicState::None, false);
						music->BeginPendingTransition();
					}
					else
					{
						OutputDebugStringA("[MegaGridBGM] local player left center zone\n");
						music->RequestState(EMusicState::Gameplay, false);
						music->BeginPendingTransition();
					}
				}
			}

			m_bWasLocalPlayerInsideMegaGridCenter = isInsideMegaGridCenter;
		}
	}
}

void CGameScene::RenderSceneComposite(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	RenderDepthFog(cmd, camera);

	BindFrameRootParameters(cmd);

	if ( m_transparentItemBillboardShader )
	{
		RenderTransparentItemBillboards(cmd, camera);
	}

	if ( m_swordTrailShader )
	{
		RenderSwordTrails(cmd, camera);
	}

	if ( m_muzzleFlashShader )
	{
		RenderMuzzleFlashes(cmd, camera);
	}

	m_hud.Render(cmd, camera);
}

void CGameScene::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	PROFILE_RENDER_SCOPE("GameScene::Render(total)");

	if ( !cmd )
		return;

	{
		PROFILE_RENDER_SCOPE("GameScene::Render::PrepareFrame");
		CScene::OnPrepareRender(cmd, camera);
		UpdateFrameRenderState(camera);
		UpdateShaderVariables(cmd);
		BindFrameRootParameters(cmd);
	}

	{
		PROFILE_RENDER_SCOPE("GameScene::Render::RenderShadowMap");
		RenderShadowMap(cmd);
	}

	{
		PROFILE_RENDER_SCOPE("GameScene::Render::RestoreSceneRenderTargets");
		RestoreSceneRenderTargets(cmd, camera);
	}

	BindFrameRootParameters(cmd);

	{
		PROFILE_RENDER_SCOPE("GameScene::Render::RenderSceneGeometry");
		RenderSceneGeometry(cmd, camera);
	}

	RenderSceneComposite(cmd, camera);
}

void CGameScene::BuildObjectsCollider()
{
	m_Collision = make_unique<CCollisionSystem>();

	m_Collision->SetHitEffectCallback(
		[ this ](
			CGameObject* weaponObject,
			CGameObject* targetObject)
		{
			if ( !targetObject )
				return;

			XMFLOAT3 hitDir = XMFLOAT3(0.0f, 0.0f, 1.0f);

			if ( weaponObject )
			{
				const XMFLOAT3 weaponPos = weaponObject->GetPosition();
				const XMFLOAT3 targetPos = targetObject->GetPosition();

				XMVECTOR dirV =
					XMVectorSet(
						targetPos.x - weaponPos.x,
						0.0f,
						targetPos.z - weaponPos.z,
						0.0f
					);

				if ( XMVectorGetX(XMVector3LengthSq(dirV)) > 1.0e-6f )
				{
					dirV = XMVector3Normalize(dirV);
					XMStoreFloat3(&hitDir, dirV);
				}
				else
				{
					// weaponObject와 targetObject 위치가 거의 같으면 무기 방향 사용.
					hitDir = GetSafeObjectForward(weaponObject);
				}
			}

			// hitPosition은 아직 정확히 모르므로 nullptr.
			// SpawnBloodSplash 내부에서 targetObject 위치 + y 1m를 사용한다.
			SpawnBloodSplash(targetObject, nullptr, &hitDir);
		}
	);

	for ( auto& obj : m_staticObjects )
	{
		if ( obj )
			m_Collision->RegisterCollider(obj->GetComponent<CColliderComponent>());
	}

	for ( auto& obj : m_skinnedObjects )
	{
		if ( obj )
			m_Collision->RegisterCollider(obj->GetComponent<CColliderComponent>());
	}
}