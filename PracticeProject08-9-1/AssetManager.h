//------------------------------------------------------- ----------------------
// File: AssetManager.h
//-----------------------------------------------------------------------------

#pragma once
#include "stdafx.h"
#include "SceneRenderTypes.h"

class CMesh;
struct MATERIALS;

enum class AssetType
{
    Player,
    Ghoul,
	SwordMan,
	BowMan,
    AxeMan,
    Boss,

    World,
    Grass,
    Ground,
    House,

    Arrow,
    Helmet,
    Sword,
    Bow,
    Axe,
    Gun
};

// BuildAsset
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

private:
    static std::wstring ResolveTexturePath(
        AssetType type,
        const std::string& textureRoot,
        const std::string& materialName,
        const std::string& diffuseTextureName
    );
};
