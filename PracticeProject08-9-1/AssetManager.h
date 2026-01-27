//------------------------------------------------------- ----------------------
// File: AssetManager.h
//-----------------------------------------------------------------------------

#pragma once
#include "stdafx.h"

class CMesh;
struct MATERIALS;

// 에셋 타입 (지금은 2개만)
enum class AssetType
{
    Unitychan,
    Castle,
    Plane
};

// BuildAsset에 넘길 데이터 묶음
struct AssetBuildDesc
{
    AssetType type;
    std::string meshBinPath;   // BIN 파일 경로
    std::string textureRoot;   // 텍스처 루트 디렉토리
};

// 빌드 결과물 (현재는 Mesh만)
// ※ 나중에 Skeleton, Animation 추가 가능
struct BuiltAsset
{
    std::shared_ptr<CMesh> mesh;
};

class AssetManager
{
public:
    // 에셋 빌드의 유일한 공개 진입점
    static BuiltAsset BuildAsset(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmd,
        MATERIALS* pMaterials,
        const AssetBuildDesc& desc
    );

private:
    // materialName + 정책(type) → 실제 텍스처 파일 경로
    static std::wstring ResolveTexturePath(
        AssetType type,
        const std::string& textureRoot,
        const std::string& materialName,
        const std::string& diffuseTextureName
    );
};
