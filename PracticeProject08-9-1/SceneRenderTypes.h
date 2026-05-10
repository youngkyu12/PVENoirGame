//-----------------------------------------------------------------------------
// File: SceneRenderTypes.h
//-----------------------------------------------------------------------------
#pragma once

#include <memory>
#include <array>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;

class CMaterial;
class CGameObject;
class CStaticObjectsShader;
class CSkinnedObjectsShader;
class CShader;

struct CB_GAMEOBJECT_INFO;
static constexpr UINT kSceneBatchFrameResourceCount = 2;


// -----------------------------------------------------------------------------
// Materials (GPU constant buffer용)
// -----------------------------------------------------------------------------
struct MATERIAL
{
	DirectX::XMFLOAT4  m_xmf4Ambient = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	DirectX::XMFLOAT4  m_xmf4Diffuse = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	DirectX::XMFLOAT4  m_xmf4Specular = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f); // rgb=specular, a=shininess
	DirectX::XMFLOAT4  m_xmf4Emissive = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	// x=diffuse, y=normal, z=emissive, w=specular
	// 0 = not bound, n+1 = global SRV index
	DirectX::XMUINT4   m_xmn4TextureIndices = DirectX::XMUINT4(0, 0, 0, 0);

	// x=scaleU, y=scaleV, z=offsetU, w=offsetV
	DirectX::XMFLOAT4  m_xmf4DiffuseUVST = DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
	DirectX::XMFLOAT4  m_xmf4NormalUVST = DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
	DirectX::XMFLOAT4  m_xmf4EmissiveUVST = DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);
	DirectX::XMFLOAT4  m_xmf4SpecularUVST = DirectX::XMFLOAT4(1.0f, 1.0f, 0.0f, 0.0f);

	// wrap packed
	// m_xmn4WrapModes0: x=diffuseU, y=diffuseV, z=normalU, w=normalV
	// m_xmn4WrapModes1: x=emissiveU, y=emissiveV, z=specularU, w=specularV
	DirectX::XMUINT4   m_xmn4WrapModes0 = DirectX::XMUINT4(0, 0, 0, 0);
	DirectX::XMUINT4   m_xmn4WrapModes1 = DirectX::XMUINT4(0, 0, 0, 0);
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

	std::array<ComPtr<ID3D12Resource>, kSceneBatchFrameResourceCount> cbGameObjects;
	std::array<CB_GAMEOBJECT_INFO*, kSceneBatchFrameResourceCount> mappedGameObjects = {};

	UINT cbElementBytes = 0;

	std::array<D3D12_GPU_DESCRIPTOR_HANDLE, kSceneBatchFrameResourceCount> baseCbvGpu = {};
	UINT cbvInc = 0;

	std::shared_ptr<CMaterial> material;
};

struct SCENE_SKINNED_BATCH
{
	std::shared_ptr<CSkinnedObjectsShader> shader;

	UINT capacity = 0;
	UINT count = 0;

	UINT cbElementBytes = 0;

	std::array<ComPtr<ID3D12Resource>, kSceneBatchFrameResourceCount> cbGameObjects;
	std::array<CB_GAMEOBJECT_INFO*, kSceneBatchFrameResourceCount> mappedGameObjects = {};

	std::array<D3D12_GPU_DESCRIPTOR_HANDLE, kSceneBatchFrameResourceCount> baseCbvGpu = {};
	UINT cbvInc = 0;

	std::vector<CGameObject*> objectRefs;
};

struct SCENE_COLLIDER_BATCH
{
	std::shared_ptr<CShader> shader;

	UINT capacity = 0;
	UINT count = 0;

	UINT cbElementBytes = 0;

	std::array<ComPtr<ID3D12Resource>, kSceneBatchFrameResourceCount> cbGameObjects;
	std::array<CB_GAMEOBJECT_INFO*, kSceneBatchFrameResourceCount> mappedGameObjects = {};

	std::array<D3D12_GPU_DESCRIPTOR_HANDLE, kSceneBatchFrameResourceCount> baseCbvGpu = {};
	UINT cbvInc = 0;

	std::vector<CGameObject*> objectRefs;
};