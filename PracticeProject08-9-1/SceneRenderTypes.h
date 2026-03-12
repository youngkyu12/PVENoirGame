//-----------------------------------------------------------------------------
// File: SceneRenderTypes.h
//-----------------------------------------------------------------------------
#pragma once

#include <memory>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;

class CMaterial;
class CGameObject;
class CStaticObjectsShader;
class CSkinnedObjectsShader;

struct CB_GAMEOBJECT_INFO;

// -----------------------------------------------------------------------------
// Materials (GPU constant buffer용)
// -----------------------------------------------------------------------------
struct MATERIAL
{
    DirectX::XMFLOAT4  m_xmf4Ambient;
    DirectX::XMFLOAT4  m_xmf4Diffuse;
    DirectX::XMFLOAT4  m_xmf4Specular; // (r,g,b,a=power)
    DirectX::XMFLOAT4  m_xmf4Emissive;
    DirectX::XMUINT4   m_xmn4TextureIndices = DirectX::XMUINT4(0, 0, 0, 0);
};

struct MATERIALS
{
    MATERIAL m_pReflections[MAX_MATERIALS];
};

// -----------------------------------------------------------------------------
// Scene-owned batches (Shader::Render()에서 참조)
// -----------------------------------------------------------------------------
struct SCENE_STATIC_BATCH
{
    std::shared_ptr<CStaticObjectsShader> shader;

    UINT capacity = 0;
    UINT count = 0;

    std::vector<CGameObject*> objectRefs;

    ComPtr<ID3D12Resource> cbGameObjects;
    CB_GAMEOBJECT_INFO* mappedGameObjects = nullptr;

    UINT cbElementBytes = 0;

    D3D12_GPU_DESCRIPTOR_HANDLE baseCbvGpu = { 0 };
    UINT cbvInc = 0;

    std::shared_ptr<CMaterial> material;
};

struct SCENE_SKINNED_BATCH
{
    std::shared_ptr<CSkinnedObjectsShader> shader;

    UINT capacity = 0;
    UINT count = 0;

    UINT cbElementBytes = 0;

    ComPtr<ID3D12Resource> cbGameObjects;
    CB_GAMEOBJECT_INFO* mappedGameObjects = nullptr;

    D3D12_GPU_DESCRIPTOR_HANDLE baseCbvGpu = { 0 };
    UINT cbvInc = 0;

    std::vector<CGameObject*> objectRefs;
};