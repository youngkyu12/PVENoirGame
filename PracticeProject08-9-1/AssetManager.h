//------------------------------------------------------- ----------------------
// File: AssetManager.h
//-----------------------------------------------------------------------------

#pragma once
#include "stdafx.h"

class CMesh;
struct MATERIALS;

enum class AssetType
{
    Zombie,
    Fighter,
    World,
    Plane,
    House,
    Arrow
};

// BuildAsset�� �ѱ� ������ ����
struct AssetBuildDesc
{
    AssetType type;
    std::string meshBinPath;   // BIN ���� ���
    std::string textureRoot;   // �ؽ�ó ��Ʈ ���丮
};

// ���� ����� (����� Mesh��)
// �� ���߿� Skeleton, Animation �߰� ����
struct BuiltAsset
{
    std::shared_ptr<CMesh> mesh;
};

class AssetManager
{
public:
    // ���� ������ ������ ���� ������
    static BuiltAsset BuildAsset(
        ID3D12Device* device,
        ID3D12GraphicsCommandList* cmd,
        MATERIALS* pMaterials,
        const AssetBuildDesc& desc
    );

private:
    // materialName + ��å(type) �� ���� �ؽ�ó ���� ���
    static std::wstring ResolveTexturePath(
        AssetType type,
        const std::string& textureRoot,
        const std::string& materialName,
        const std::string& diffuseTextureName
    );
};
