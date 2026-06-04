//-----------------------------------------------------------------------------
// File: GameSceneHelper.h
// Description:
//   GameScene*.cpp 전용 공통 상수/구조체/helper 선언.
//   헤더에 익명 namespace를 두지 않기 위해 GameSceneHelper namespace를 사용한다.
//-----------------------------------------------------------------------------
#pragma once

#include "GameScenePrivate.h"

#include <cstdint>

namespace GameSceneHelper
{
	static constexpr uint32_t kCollisionLayerPlayer = 0; // 플레이어 본체
	static constexpr uint32_t kCollisionLayerMonster = 1; // 몬스터 본체
	static constexpr uint32_t kCollisionLayerWorldStatic = 2; // 월드 정적 오브젝트
	static constexpr uint32_t kCollisionLayerPlayerWeapon = 3; // 플레이어 무기
	static constexpr uint32_t kCollisionLayerMonsterWeapon = 4; // 몬스터 무기

	static constexpr uint32_t CollisionBit(uint32_t layer)
	{
		return 1u << layer;
	}

	static constexpr uint32_t kNetworkEnemyTypeNone = 0;
	static constexpr uint32_t kNetworkEnemyTypeBasic = 1;
	static constexpr uint32_t kNetworkEnemyTypeArcher = 2;
	static constexpr uint32_t kNetworkEnemyTypeWarrior = 3;
	static constexpr uint32_t kNetworkEnemyTypeBoss = 4;
	static constexpr uint32_t kNetworkEnemyTypeMutant = 5;

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

	void ConfigureProjectileCollider(CColliderComponent* collider, bool firedByPlayer);
	DecodedAnimStateCode DecodeStateCode(uint32_t stateCode);

	static constexpr UINT kDebugSubmeshOOBBCapacity = 8192;
	static constexpr bool kEnableStaticWorldLocalOOBBReportExport = false;
	static constexpr const char* kStaticWorldLocalOOBBReportPath = "MapData/StaticWorldLocalOOBBReport.txt";
	static constexpr bool kEnableCastleVillageWallColliderBuildLog = false;

	XMFLOAT4X4 BuildWorldMatrixFromOOBB(const BoundingOrientedBox& box);
	XMFLOAT4X4 BuildIdentityMatrix4x4();

	bool IsTreeCullBlockerAssetName(const std::string& assetName);
	bool ShouldStaticPlacementCastShadow(const std::string& assetName);
	void SetObjectAttackPower(CGameObject* obj, int attackPower);
	float DistanceSqXZ(const XMFLOAT3& a, const XMFLOAT3& b);

	void StoreStaticWorldRows(
		XMFLOAT4& out0,
		XMFLOAT4& out1,
		XMFLOAT4& out2,
		XMFLOAT4& out3,
		const XMFLOAT4X4& W
	);

	XMFLOAT3 GetSafeObjectForward(const CGameObject* obj);

	static const XMFLOAT3 kLocalPlayerRespawnPosition =
		XMFLOAT3(0.0f, 0.0f, -100.0f);

	static constexpr float kLocalPlayerRespawnDelay = 5.0f;

	static constexpr ELocalStagePreset kLocalStagePreset = ELocalStagePreset::FullStage;

	struct StaticGroupKey
	{
		const CMesh* mesh = nullptr;
		UINT subMeshIndex = 0;
		bool useTreeShader = false;
		bool useTerrainShader = false;
		bool useWaterShader = false;
		int lodLevel = 0;

		bool operator==(const StaticGroupKey& rhs) const
		{
			return mesh == rhs.mesh &&
				subMeshIndex == rhs.subMeshIndex &&
				useTreeShader == rhs.useTreeShader &&
				useTerrainShader == rhs.useTerrainShader &&
				useWaterShader == rhs.useWaterShader &&
				lodLevel == rhs.lodLevel;
		}
	};

	struct StaticGroupKeyHash
	{
		size_t operator()(const StaticGroupKey& k) const
		{
			size_t h = std::hash<const void*>{}( k.mesh );

			h ^= std::hash<UINT>{}( k.subMeshIndex )
				+ 0x9e3779b9
				+ ( h << 6 )
				+ ( h >> 2 );

			h ^= std::hash<bool>{}( k.useTreeShader )
				+ 0x9e3779b9
				+ ( h << 6 )
				+ ( h >> 2 );

			h ^= std::hash<bool>{}( k.useTerrainShader )
				+ 0x9e3779b9
				+ ( h << 6 )
				+ ( h >> 2 );
			
			h ^= std::hash<bool>{}( k.useWaterShader )
				+ 0x9e3779b9
				+ ( h << 6 )
				+ ( h >> 2 );

			h ^= std::hash<int>{}( k.lodLevel )
				+ 0x9e3779b9
				+ ( h << 6 )
				+ ( h >> 2 );

			return h;
		}
	};

	struct SkinnedGroupKey
	{
		std::string geometryKey;
		UINT meshIndex = 0;
		UINT subMeshIndex = 0;
		bool useAlphaClipShader = false;

		bool operator==(const SkinnedGroupKey& rhs) const
		{
			return geometryKey == rhs.geometryKey &&
				meshIndex == rhs.meshIndex &&
				subMeshIndex == rhs.subMeshIndex &&
				useAlphaClipShader == rhs.useAlphaClipShader;
		}
	};

	struct SkinnedGroupKeyHash
	{
		size_t operator()(const SkinnedGroupKey& k) const
		{
			size_t h = std::hash<std::string>{}( k.geometryKey );

			h ^= std::hash<UINT>{}( k.meshIndex )
				+ 0x9e3779b9
				+ ( h << 6 )
				+ ( h >> 2 );

			h ^= std::hash<UINT>{}( k.subMeshIndex )
				+ 0x9e3779b9
				+ ( h << 6 )
				+ ( h >> 2 );

			h ^= std::hash<bool>{}( k.useAlphaClipShader )
				+ 0x9e3779b9
				+ ( h << 6 )
				+ ( h >> 2 );

			return h;
		}
	};

	// -------------------------------------------------------------------------
	// HP
	// -------------------------------------------------------------------------
	static constexpr int kHpGhoul = 30;
	static constexpr int kHpBowMan = 120;
	static constexpr int kHpSwordMan = 120;
	static constexpr int kHpMutant = 240;
	static constexpr int kHpBoss = 4800;
	static constexpr int kHpPlayer = 100;

	// -------------------------------------------------------------------------
	// Attack power
	// -------------------------------------------------------------------------
	static constexpr int kPlayerWeaponDamageTierCount = 3;
	static constexpr int kPlayerWeaponDamageMaxTierIndex = kPlayerWeaponDamageTierCount - 1;

	// tier index: 0=1단계, 1=2단계, 2=3단계
	static constexpr std::array<int, kPlayerWeaponDamageTierCount> kAttackPowerPlayerArrowByTier =
	{
		15, 30, 50
	};

	static constexpr std::array<int, kPlayerWeaponDamageTierCount> kAttackPowerPlayerBulletByTier =
	{
		600, 600, 600
		//8, 18, 35
	};

	static constexpr std::array<int, kPlayerWeaponDamageTierCount> kAttackPowerPlayerAxeByTier =
	{
		15, 30, 50
	};

	static constexpr std::array<int, kPlayerWeaponDamageTierCount> kAttackPowerPlayerSwordByTier =
	{
		//600, 600, 600
		10, 20, 40
	};

	static constexpr int kAttackPowerGhoul = 5;
	static constexpr int kAttackPowerEnemySword = 10;
	static constexpr int kAttackPowerEnemyArrow = 10;
	static constexpr int kAttackPowerMutant = 20;
	static constexpr int kAttackPowerBoss = 50;

	static constexpr float kDisableVillageTreeCullPlayerHeight = 3.0f;

	static constexpr int kCastleCenterMegaGridX = 1;
	static constexpr int kCastleCenterMegaGridZ = 1;

	// Castle 텔레포트는 총 4개 이상의 메가그리드가 클리어된 뒤부터 허용한다.
	static constexpr int kRequiredClearedMegaGridCountForCastlePortal = 4;
	constexpr float kMegaGrid5CenterSquareHalfExtent = 125.0f;
	bool IsWorldPositionInsideMegaGrid5CenterSquare250(float worldX, float worldZ);

#ifndef USING_NETWORK
	static constexpr int kTowerDoorPortalCooldownFrames = 30;
	static constexpr float kTowerDoorPortalExitOffset = 2.0f;

	static constexpr int kCastleDoorPortalCooldownFrames = 30;
	static constexpr float kCastleDoorPortalExitOffset = 2.0f;

	static constexpr float kTowerDoorPortalLowerExitYOffset = 0.0f;
	static constexpr float kTowerDoorPortalUpperExitYOffset = 3.5f;
	static constexpr float kTowerDoorPortalUpperHeightThreshold = 10.0f;
	static constexpr float kTowerDoorPortalVerticalResolveStep = 0.25f;
	static constexpr float kTowerDoorPortalMaxVerticalResolveDistance = 8.0f;
	static constexpr float kTowerDoorPortalPlayerYawOffsetFromCamera = 0.0f;

	static constexpr bool kEnableTowerDoorPortalCollisionLog = false;
	static constexpr bool kEnableTowerDoorPortalVerboseLog = false;
#endif

	bool ParsePlacementEntryLine(const std::string& line, StaticPlacementEntry& outEntry);
	std::string TrimString(const std::string& text);
	std::string StripTopRootFromAPath(const std::string& fullAPath);
	bool ParseVector3Tuple(const std::string& text, XMFLOAT3& outValue);
	bool ParseVector4Tuple(const std::string& text, XMFLOAT4& outValue);
	std::string ToLowerAscii(const std::string& text);
	float NormalizeYawDegrees180(float yaw);

#ifndef USING_NETWORK
	std::string NormalizeTowerDoorNameForMatch(const std::string& text);
	bool IsTowerDoorFrame2Name(const std::string& meshName, const std::string& authoringPath);
	int GetCastleDoorFrameIndexFromMeshName(const std::string& meshName);
	const char* GetCastleDoorFrameDebugName(int index);
	bool IsTowerDoorFrame1Name(const std::string& meshName, const std::string& authoringPath);

	void DebugPrintTowerDoorPortalLine(const char* text);
	void DebugPrintTowerDoorPortalFloat3(const char* tag, const XMFLOAT3& v);
	void DebugPrintTowerDoorPortalOOBB(const char* tag, const BoundingOrientedBox& box);
#endif

	bool ResolveStageFileSetFromMapId(
	const std::string& mapId,
	GameSceneStageFileSet& outFileSet
	);

	bool ResolvePlacementFilePathFromMapId(
		const std::string& mapId,
		std::string& outPlacementFilePath
	);

	void TriggerMonsterTestCommand(
		CGameObject* obj,
		EMonsterAnimCommand cmd,
		EMonsterAnimState locomotion = EMonsterAnimState::Idle
	);

	// -------------------------------------------------------------------------
	// Terrain
	// -------------------------------------------------------------------------
	constexpr int kTerrainHeightMapSamples = 1025;
	constexpr float kTerrainWorldSize = 1200.0f;
	constexpr float kTerrainHorizontalScale = kTerrainWorldSize / static_cast< float >( kTerrainHeightMapSamples - 1 );
	constexpr float kTerrainVerticalScale = 0.0738f;
}
