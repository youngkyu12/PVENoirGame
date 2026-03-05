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
    Zombie,
    Fighter,
    Ghoul,
    Boss,
    World,
    Plane,
    House,
    Arrow
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
