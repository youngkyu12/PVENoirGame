//-----------------------------------------------------------------------------
// File: GameSceneContentCatalog.h
//-----------------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "AssetManager.h"

struct StaticAssetPathDesc
{
	AssetType type{};
	std::string meshBinPath;
	std::string texturePath;
};

struct GameSceneStageFileSet
{
	std::string placementFilePath;
	std::string navMeshFilePath;
	std::string cubeColliderReportFilePath;
	std::string monsterSpawnFilePath;
};

struct GameSceneClipEntry
{
	const char* filePath = nullptr;
	const char* clipName = nullptr;
};

enum class ELocalStagePreset : uint8_t
{
	Test,
	FullStage
};

enum class EGameSceneAssetId : uint8_t
{
	Arrow,
	Bullet,
	Helmet,

	PlayerSword,
	PlayerAxe,
	PlayerGun,
	PlayerBow,

	EnemySword,
	EnemyBow,

	PlayerMesh1,
	PlayerMesh2,
	PlayerMesh3,
	PlayerMesh4,

	Ghoul,
	SwordMan,
	BowMan,
	Mutant,
	Boss
};

enum class EGameSceneAttachmentPresetId : uint8_t
{
	PlayerSword = 0,
	PlayerBow,
	PlayerAxe,
	PlayerGun,
	EnemySword,
	EnemyBow,
	MutantHelmet
};

struct GameSceneAttachmentTransformDesc
{
	XMFLOAT3 pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 rotDeg = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3 scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
};

struct GameSceneAttachmentPresetDesc
{
	const char* boneName = nullptr;
	GameSceneAttachmentTransformDesc local{};
};

bool GetGameSceneAttachmentPresetDesc(
	EGameSceneAttachmentPresetId id,
	GameSceneAttachmentPresetDesc& outDesc
);

const GameSceneStageFileSet& GetLocalStageFileSet(ELocalStagePreset preset);

bool GetGameSceneAssetBuildDesc(EGameSceneAssetId assetId, AssetBuildDesc& outDesc);

bool ResolveStaticAssetPathDesc(const std::string& assetName, StaticAssetPathDesc& outDesc);
bool ResolveStaticLodAssetPathDesc(const std::string& assetName, int lodLevel, StaticAssetPathDesc& outDesc);
bool ResolveStaticAssetDesc(const std::string& assetName, AssetBuildDesc& outDesc, AssetType* outResolvedType = nullptr);
bool ResolveStaticLodAssetDesc(const std::string& assetName, int lodLevel, AssetBuildDesc& outDesc, AssetType* outResolvedType = nullptr);
bool ResolveGhoulSkinnedLodAssetDesc(int lodLevel, AssetBuildDesc& outDesc);

bool IsStaticWorldLodSupportedAssetName(const std::string& assetName);
bool ShouldUseStaticWorldDistanceCull(const std::string& assetName);
bool ShouldCreateWorldStaticCollider(const std::string& assetName);

const std::vector<GameSceneClipEntry>& GetGhoulClipEntries();
const std::vector<GameSceneClipEntry>& GetEnemySwordClipEntries();
const std::vector<GameSceneClipEntry>& GetEnemyBowClipEntries();
const std::vector<GameSceneClipEntry>& GetMutantClipEntries();
const std::vector<GameSceneClipEntry>& GetBossClipEntries();
const std::vector<GameSceneClipEntry>& GetPlayerClipEntries();
const std::vector<GameSceneClipEntry>& GetPlayerBowClipEntries();
const std::vector<GameSceneClipEntry>& GetEnemyBowWeaponClipEntries();