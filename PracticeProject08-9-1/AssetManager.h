//-----------------------------------------------------------------------------
// File: AssetManager.h
//-----------------------------------------------------------------------------
#pragma once

#include "stdafx.h"
#include "SceneRenderTypes.h"

#include <string>
#include <unordered_map>
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
    static BuiltAsset BuildAsset(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmd,
        MATERIALS* pMaterials,
        const AssetBuildDesc& desc
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
    static UINT s_nextMaterialID;
};