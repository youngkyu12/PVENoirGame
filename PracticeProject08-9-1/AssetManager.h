//-----------------------------------------------------------------------------
// File: AssetManager.h
//-----------------------------------------------------------------------------
#pragma once

#include "stdafx.h"
#include "AnimatorData.h"
#include "SceneRenderTypes.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>

class CMesh;
class CTexture;
class CMaterial;
struct MATERIALS;

enum class AssetType
{
    Player,
    Ghoul,
    SwordMan,
    BowMan,
    Mutant,
    Boss,

    World,
    Grass,
    Ground,
    DirtRoad,
    VillageWall,
    House,
    Tower,
	Tree,
	Castle,

    Arrow,
	Bullet,
    Helmet,
    Sword,
    Bow,
    Axe,
    Gun
};

struct AssetBuildDesc
{
    AssetType type;
    std::string meshBinPath;
    std::string textureRoot;
};

struct BuiltAsset
{
    std::shared_ptr<CMesh> mesh;
};

class AssetManager
{
public:
	using AnimationClipRef = std::shared_ptr<const AnimationClip>;

public:
	static BuiltAsset BuildAsset(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* cmd,
		MATERIALS* pMaterials,
		const AssetBuildDesc& desc
	);

	static void BeginSceneMaterialBuild(MATERIALS* pMaterials);

	static AnimationClipRef LoadCachedClipRef(
		CMesh* mesh,
		const std::string& skeletonKey,
		const char* animBinPath,
		const char* clipName,
		float timeScale = 1.0f
	);

	static void ClearCache();

private:
    static BuiltAsset BuildAssetInternal(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmd,
        const AssetBuildDesc& desc
    );

    static void ApplyBuiltAssetToSceneMaterials(
        const BuiltAsset& asset,
        MATERIALS* pMaterials
    );

	static std::string MakeAssetKey(const AssetBuildDesc& desc);
	static std::string MakeMaterialKey(
		AssetType type,
		const std::string& textureRoot,
		const std::string& materialName,
		const std::string& materialFingerprint);

	static std::string MakeClipKey(
		const std::string& skeletonKey,
		const std::string& animBinPath,
		const std::string& clipName,
		float timeScale
	);

	static std::wstring ResolveTexturePath(
		AssetType type,
		const std::string& textureRoot,
		const std::string& materialName,
		const std::string& diffuseTextureName
	);

private:
	static std::unordered_map<std::string, BuiltAsset> s_assetCache;
	static std::unordered_map<std::string, std::shared_ptr<CMaterial>> s_materialCache;
	static std::unordered_map<std::string, std::shared_ptr<CTexture>> s_textureCache;
	static std::unordered_map<std::string, AnimationClipRef> s_clipCache;

	static std::unordered_map<MATERIALS*, std::unordered_set<std::string>>
		s_appliedAssetKeysByMaterials;

	static UINT s_nextMaterialID;
};