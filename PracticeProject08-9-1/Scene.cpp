//-----------------------------------------------------------------------------
// File: Scene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"
#include "AnimatorComponent.h"
#include "AnimController.h"
#include "Material.h"
#include "AssetManager.h"
#include "LightComponent.h"
#include "PlayerControllerComponent.h"
#include "Object.h"
#include "ActorTagComponent.h"
#include "ArrowComponent.h"
#include "ColliderComponent.h"

#include "ThreadManager.h"
#include "Service.h"
#include "Session.h"
#include "BufferReader.h"
#include "ServerPacketHandler.h"

std::unique_ptr<CDescriptorHeap> CScene::m_pDescriptorHeap = std::make_unique<CDescriptorHeap>();


CScene::CScene()
{
	m_staticBatch.capacity = 4 + kArrowPoolSize;
	m_staticBatch.count = 0;

	m_skinnedBatch.capacity = 10;
	m_skinnedBatch.count = 0;

	m_demoFighters.fill(nullptr);
	m_arrowRefs.clear();
	m_arrowRefs.shrink_to_fit();
}

CScene::~CScene()
{
}

void CScene::ReleaseObjects()
{
	if (m_pd3dGraphicsRootSignature)
		m_pd3dGraphicsRootSignature.Reset();

	// shaders: shared_ptr reset��(�ʿ��ϸ�)
	m_staticBatch.shader.reset();
	m_skinnedBatch.shader.reset();

	// objects clear
	m_staticObjects.clear();
	m_skinnedObjects.clear();
	m_pPlayer.reset();

	m_lightObjects.clear();
	m_pPlayerSpotFollower = nullptr;

	m_demoFighters.fill(nullptr);

	// Main Camera (GameObject + Camera Component)
	if (m_pMainCamera)
	{
		m_pMainCamera->ReleaseShaderVariables();
		m_pMainCamera = nullptr;
	}
	m_pMainCameraObject.reset();

	// Batch ref lists clear
	m_staticBatch.objectRefs.clear();
	m_skinnedBatch.objectRefs.clear();

	// scene-owned CB/���ҽ� ����
	ReleaseShaderVariables();
}

void CScene::ReleaseUploadBuffers()
{
	for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
	{
		if (!m_staticObjects[j]) continue;
		m_staticObjects[j]->ReleaseUploadBuffers();
	}
	for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
	{
		if (!m_skinnedObjects[j]) continue;
		m_skinnedObjects[j]->ReleaseUploadBuffers();
	}
	if (m_pPlayer)
		m_pPlayer->ReleaseUploadBuffers();

#ifdef _WITH_BATCH_MATERIAL
	if (m_staticBatch.material)  m_staticBatch.material->ReleaseUploadBuffers();
#endif
}



void CScene::ReleaseShaderVariables()
{
	// ---- Static batch CB ----
	if (m_staticBatch.cbGameObjects)
	{
		if (m_staticBatch.mappedGameObjects)
		{
			m_staticBatch.cbGameObjects->Unmap(0, NULL);
			m_staticBatch.mappedGameObjects = nullptr;
		}
		m_staticBatch.cbGameObjects.Reset();
	}

	// ---- Skinned batch CBs ----	if (m_skinnedBatch.cbGameObjects)
	{
		if (m_skinnedBatch.mappedGameObjects)
		{
			m_skinnedBatch.cbGameObjects->Unmap(0, NULL);
			m_skinnedBatch.mappedGameObjects = nullptr;
		}
		m_skinnedBatch.cbGameObjects.Reset();
	}

	if (m_pd3dcbLights)
	{
		m_pd3dcbLights->Unmap(0, NULL);
		m_pd3dcbLights.Reset();
	}
	m_pcbMappedLights = nullptr;

	if (m_pd3dcbMaterials)
	{
		m_pd3dcbMaterials->Unmap(0, NULL);
		m_pd3dcbMaterials.Reset();
	}

	if (m_pd3dcbPlayerGameObject)
	{
		if (m_pcbMappedPlayerGameObject)
		{
			m_pd3dcbPlayerGameObject->Unmap(0, NULL);
			m_pcbMappedPlayerGameObject = nullptr;
		}
		m_pd3dcbPlayerGameObject.Reset();
	}
}

void CScene::BuildObjects(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList)
{
	// 1. Root Signature
	CreateGraphicsRootSignature(pd3dDevice);

	// 2. Shader 인스턴스 생성 + 오브젝트 개수 선확보
	constexpr int MAX_GLOBAL_SRVS = 1024;

	auto pStaticShader = std::make_shared<CStaticObjectsShader>();
	auto pSkinnedShader = std::make_shared<CSkinnedObjectsShader>();

	m_staticBatch.shader = pStaticShader;
	m_skinnedBatch.shader = pSkinnedShader;

	const UINT cbvTotal =
		m_staticBatch.capacity +
		m_skinnedBatch.capacity +
		1 /*Camera*/ +
		1 /*Player*/ +
		1 /*etc*/;

	// 3. DescriptorHeap은 "한 번만" 생성
	m_pDescriptorHeap->CreateCbvSrvDescriptorHeaps(
		pd3dDevice,
		cbvTotal,
		MAX_GLOBAL_SRVS);

	// 공통 RTV 포맷
	DXGI_FORMAT rtvFormats[5] =
	{
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R32_FLOAT
	};

	// 4/5. Materials + Batches
	BuildLightsAndMaterials();

	// Light objects components create
	for (auto& lo : m_lightObjects)
	{
		if (lo) lo->CreateComponents(pd3dDevice, pd3dCommandList);
	}

	constexpr UINT kRTCount = 5;
	const DXGI_FORMAT kDsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	BuildStaticBatch(pd3dDevice, pd3dCommandList, pStaticShader, kRTCount, rtvFormats, kDsvFormat);
	BuildSkinnedBatch(pd3dDevice, pd3dCommandList, pSkinnedShader, kRTCount, rtvFormats, kDsvFormat);

	// 6. Scene 공통 Shader Variables
	CreateShaderVariables(pd3dDevice, pd3dCommandList);
	BuildPlayer(pd3dDevice, pd3dCommandList);
	CreateMainCamera(pd3dDevice, pd3dCommandList, m_pPlayer.get());
}

void CScene::BuildLightsAndMaterials()
{
	// Light Objects (Empty GameObjects + Components)
	m_lightObjects.clear();
	m_lightObjects.reserve(4);
	m_pPlayerSpotFollower = nullptr;

	// [0] Point Light
	{
		auto obj = std::make_unique<CGameObject>(0); // empty
		obj->SetPosition(1.0f, 0.0f, 0.0f);

		auto* lc = obj->AddComponent<CLightComponent>();
		lc->type = ELightType::Point;
		lc->range = 100.0f;
		lc->ambient = XMFLOAT4(0.1f, 0.0f, 0.0f, 1.0f);
		lc->diffuse = XMFLOAT4(0.8f, 0.0f, 0.0f, 1.0f);
		lc->specular = XMFLOAT4(0.1f, 0.1f, 0.1f, 0.0f);
		lc->attenuation = XMFLOAT3(1.0f, 0.001f, 0.0001f);

		m_lightObjects.push_back(std::move(obj));
	}

	// [1] Spot Light (player follow)
	{
		auto obj = std::make_unique<CGameObject>(0); // empty

		auto* lc = obj->AddComponent<CLightComponent>();
		lc->type = ELightType::Spot;
		lc->range = 50.0f;
		lc->ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
		lc->diffuse = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
		lc->specular = XMFLOAT4(0.1f, 0.1f, 0.1f, 0.0f);
		lc->attenuation = XMFLOAT3(1.0f, 0.01f, 0.0001f);
		lc->falloff = 8.0f;
		lc->cosPhi = (float)cos(XMConvertToRadians(40.0f));
		lc->cosTheta = (float)cos(XMConvertToRadians(20.0f));

		auto* follow = obj->AddComponent<CFollowTransformComponent>();
		m_pPlayerSpotFollower = follow;

		m_lightObjects.push_back(std::move(obj));
	}

	// [2] Directional Light
	{
		auto obj = std::make_unique<CGameObject>(0); // empty
		if (auto* tr = obj->GetComponent<CTransformComponent>())
			tr->SetLookDirection(XMFLOAT3(1.0f, 0.0f, 0.0f));

		auto* lc = obj->AddComponent<CLightComponent>();
		lc->type = ELightType::Directional;
		lc->ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
		lc->diffuse = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
		lc->specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		m_lightObjects.push_back(std::move(obj));
	}

	// [3] Spot Light
	{
		auto obj = std::make_unique<CGameObject>(0); // empty
		obj->SetPosition(-150.0f, 30.0f, 30.0f);
		if (auto* tr = obj->GetComponent<CTransformComponent>())
			tr->SetLookDirection(XMFLOAT3(0.0f, 1.0f, 1.0f));

		auto* lc = obj->AddComponent<CLightComponent>();
		lc->type = ELightType::Spot;
		lc->range = 60.0f;
		lc->ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
		lc->diffuse = XMFLOAT4(0.5f, 0.0f, 0.0f, 1.0f);
		lc->specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		lc->attenuation = XMFLOAT3(1.0f, 0.01f, 0.0001f);
		lc->falloff = 8.0f;
		lc->cosPhi = (float)cos(XMConvertToRadians(90.0f));
		lc->cosTheta = (float)cos(XMConvertToRadians(30.0f));

		m_lightObjects.push_back(std::move(obj));
	}

	m_pMaterials = make_unique<MATERIALS>();
	::ZeroMemory(m_pMaterials.get(), sizeof(MATERIALS));

	m_pMaterials->m_pReflections[0] = {
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 5.0f),
		XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
	};

	m_pMaterials->m_pReflections[1] = {
		XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f),
		XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 10.0f),
		XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
	};

	m_pMaterials->m_pReflections[2] = {
		XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
		XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 15.0f),
		XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
	};

	m_pMaterials->m_pReflections[3] = {
		XMFLOAT4(0.5f, 0.0f, 1.0f, 1.0f),
		XMFLOAT4(0.0f, 0.5f, 1.0f, 1.0f),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 20.0f),
		XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
	};

	m_pMaterials->m_pReflections[4] = {
		XMFLOAT4(0.0f, 0.5f, 1.0f, 1.0f),
		XMFLOAT4(0.5f, 0.0f, 1.0f, 1.0f),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 25.0f),
		XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
	};

	m_pMaterials->m_pReflections[5] = {
		XMFLOAT4(0.0f, 0.5f, 0.5f, 1.0f),
		XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 30.0f),
		XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
	};

	m_pMaterials->m_pReflections[6] = {
		XMFLOAT4(0.5f, 0.5f, 1.0f, 1.0f),
		XMFLOAT4(0.5f, 0.5f, 1.0f, 1.0f),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 35.0f),
		XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
	};

	m_pMaterials->m_pReflections[7] = {
		XMFLOAT4(1.0f, 0.5f, 1.0f, 1.0f),
		XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f),
		XMFLOAT4(1.0f, 1.0f, 1.0f, 40.0f),
		XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
	};

	for (int i = 0; i < MAX_MATERIALS; ++i)
	{
		m_pMaterials->m_pReflections[i].m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);
	}
}

void CScene::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
	D3D12_DESCRIPTOR_RANGE pd3dDescriptorRanges[2] = {};

	pd3dDescriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	pd3dDescriptorRanges[0].NumDescriptors = 1;
	pd3dDescriptorRanges[0].BaseShaderRegister = 2; // b2
	pd3dDescriptorRanges[0].RegisterSpace = 0;
	pd3dDescriptorRanges[0].OffsetInDescriptorsFromTableStart = 0;

	constexpr UINT MAX_GLOBAL_SRVS = 1024;
	pd3dDescriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[1].NumDescriptors = MAX_GLOBAL_SRVS;
	pd3dDescriptorRanges[1].BaseShaderRegister = 0; // t0부터
	pd3dDescriptorRanges[1].RegisterSpace = 0;      // space0
	pd3dDescriptorRanges[1].OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER pd3dRootParameters[9] = {};

	pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[0].Descriptor.ShaderRegister = 1;
	pd3dRootParameters[0].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[1].Descriptor.ShaderRegister = 0;
	pd3dRootParameters[1].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[2].DescriptorTable.pDescriptorRanges = &pd3dDescriptorRanges[0];
	pd3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[3].Descriptor.ShaderRegister = 3;
	pd3dRootParameters[3].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[4].Descriptor.ShaderRegister = 4;
	pd3dRootParameters[4].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[5].Descriptor.ShaderRegister = 5;
	pd3dRootParameters[5].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[6].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[6].DescriptorTable.pDescriptorRanges = &pd3dDescriptorRanges[1];
	pd3dRootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	pd3dRootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[7].Constants.Num32BitValues = 1;
	pd3dRootParameters[7].Constants.ShaderRegister = 6; // b6
	pd3dRootParameters[7].Constants.RegisterSpace = 0;
	pd3dRootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[8].Descriptor.ShaderRegister = 7; // b7
	pd3dRootParameters[8].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	D3D12_STATIC_SAMPLER_DESC d3dSamplerDesc = {};
	d3dSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	d3dSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	d3dSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	d3dSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	d3dSamplerDesc.MipLODBias = 0;
	d3dSamplerDesc.MaxAnisotropy = 1;
	d3dSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	d3dSamplerDesc.MinLOD = 0;
	d3dSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	d3dSamplerDesc.ShaderRegister = 0;
	d3dSamplerDesc.RegisterSpace = 0;
	d3dSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc = {};
	d3dRootSignatureDesc.NumParameters = _countof(pd3dRootParameters);
	d3dRootSignatureDesc.pParameters = pd3dRootParameters;
	d3dRootSignatureDesc.NumStaticSamplers = 1;
	d3dRootSignatureDesc.pStaticSamplers = &d3dSamplerDesc;
	d3dRootSignatureDesc.Flags = d3dRootSignatureFlags;

	ComPtr<ID3DBlob> pd3dSignatureBlob;
	ComPtr<ID3DBlob> pd3dErrorBlob;

	HRESULT hr = D3D12SerializeRootSignature(
		&d3dRootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&pd3dSignatureBlob,
		&pd3dErrorBlob
	);

	if (FAILED(hr))
	{
		if (pd3dErrorBlob)
			OutputDebugStringA((char*)pd3dErrorBlob->GetBufferPointer());
		return;
	}

	hr = pd3dDevice->CreateRootSignature(
		0,
		pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(m_pd3dGraphicsRootSignature.ReleaseAndGetAddressOf())
	);

	if (FAILED(hr))
	{
		OutputDebugStringA("CreateRootSignature failed.\n");
		return;
	}

	if (pd3dSignatureBlob) pd3dSignatureBlob.Reset();
	if (pd3dErrorBlob) pd3dErrorBlob.Reset();
}

void CScene::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	UINT ncbElementBytes = ((sizeof(LIGHTS) + 255) & ~255);
	m_pd3dcbLights = ::CreateBufferResource(
		pd3dDevice,
		pd3dCommandList,
		nullptr,
		ncbElementBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	m_pd3dcbLights->Map(0, nullptr, (void**)&m_pcbMappedLights);

	UINT ncbMaterialBytes = ((sizeof(MATERIALS) + 255) & ~255);
	m_pd3dcbMaterials = ::CreateBufferResource(
		pd3dDevice,
		pd3dCommandList,
		nullptr,
		ncbMaterialBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	m_pd3dcbMaterials->Map(0, nullptr, (void**)&m_pcbMappedMaterials);
}

void CScene::BuildStaticBatch(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	const std::shared_ptr<CStaticObjectsShader>& pStaticShader,
	UINT nRenderTargets,
	DXGI_FORMAT* rtvFormats,
	DXGI_FORMAT dsvFormat
)
{
	auto* b = &m_staticBatch;
	if (!b) return;

	if (b->capacity < 4) b->capacity = 4;
	const UINT cap = b->capacity;
	if (cap == 0) return;

	pStaticShader->CreateShader(
		pd3dDevice,
		m_pd3dGraphicsRootSignature.Get(),
		nRenderTargets,
		rtvFormats,
		dsvFormat
	);

	b->cbElementBytes = ((sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255);

	b->cbGameObjects = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList, nullptr,
		b->cbElementBytes * cap,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr);

	b->cbGameObjects->Map(0, nullptr, (void**)&b->mappedGameObjects);

	b->baseCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
	b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	m_pDescriptorHeap->CreateConstantBufferViews(
		pd3dDevice,
		cap,
		b->cbGameObjects.Get(),
		b->cbElementBytes
	);

	m_staticObjects.clear();
	m_staticObjects.reserve(cap);

	b->objectRefs.clear();
	b->objectRefs.reserve(cap);

	b->count = 0;

	struct StaticAssetDesc
	{
		AssetType type;
		const char* meshBin;
		const char* texDir;
	};

	StaticAssetDesc descs[4] =
	{
		{ AssetType::Plane, "Assets/GroundPlane/Mesh/Plane.bin", "Assets/GroundPlane/Texture" },
		{ AssetType::House, "Assets/House/Mesh/Barn1.bin", "Assets/House/Texture" },
		{ AssetType::House, "Assets/House/Mesh/Barn2.bin", "Assets/House/Texture" },
		{ AssetType::House, "Assets/House/Mesh/Cabin1.bin", "Assets/House/Texture" },
	};
	XMFLOAT3 positions[4] =
	{
		XMFLOAT3(0.0f, 0.0f, 0.0f),
		XMFLOAT3(22.0f, 0.0f, 12.0f),
		XMFLOAT3(-20.0f, 0.0f, 0.0f),
		XMFLOAT3(0.0f, 0.0f, -20.0f),
	};

	for (UINT k = 0; k < 4; ++k)
	{
		if (b->objectRefs.size() >= b->capacity) break;

		const UINT i = (UINT)b->objectRefs.size();

		AssetBuildDesc Desc =
		{
			descs[k].type,
			descs[k].meshBin,
			descs[k].texDir
		};

		BuiltAsset asset = AssetManager::BuildAsset(
			pd3dDevice, pd3dCommandList,
			m_pMaterials.get(),
			Desc);

		auto obj = std::make_unique<CGameObject>(1);

		auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
		obj->SetMappedGameObjectCB(cb);

		obj->SetMesh(0, asset.mesh);
		obj->AddComponent<CStaticMeshRendererComponent>();

		obj->AddComponent<CColliderComponent>(EColliderType::OOBB);

		obj->SetPosition(positions[k]);

		obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

		obj->CreateComponents(pd3dDevice, pd3dCommandList);

		CColliderComponent* collider = obj->GetComponent<CColliderComponent>();
		m_ColliderBoxes.push_back(*collider);

		CGameObject* raw = obj.get();
		m_staticObjects.push_back(std::move(obj));
		b->objectRefs.push_back(raw);
		b->count = (UINT)b->objectRefs.size();
	}
	// ------------------------------------------------------------------------
	// Arrow pool (Static) : same mesh, multiple instances, transform-only move
	// ------------------------------------------------------------------------
	{
		// ȭ�� �޽ô� �ϳ��� �����ؼ� ��� ȭ���� �����Ѵ�.
		AssetBuildDesc ArrowDesc =
		{
			AssetType::Arrow,                   
			"Assets/Arrow/Mesh/Arrow.bin",
			"Assets/Arrow/Texture"
		};

		BuiltAsset arrowAsset = AssetManager::BuildAsset(
			pd3dDevice, pd3dCommandList,
			m_pMaterials.get(),
			ArrowDesc
		);

		m_arrowRefs.clear();
		m_arrowRefs.reserve(kArrowPoolSize);

		for (UINT k = 0; k < kArrowPoolSize; ++k)
		{
			if (b->objectRefs.size() >= b->capacity) break;

			const UINT i = (UINT)b->objectRefs.size();

			auto obj = std::make_unique<CGameObject>(1);

			auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
			obj->SetMappedGameObjectCB(cb);

			obj->SetMesh(0, arrowAsset.mesh);
			obj->AddComponent<CStaticMeshRendererComponent>();

			obj->AddComponent<CColliderComponent>(EColliderType::OOBB);

			// ȭ�� �̵�/���� ������Ʈ
			auto* arrow = obj->AddComponent<CArrowComponent>();
			(void)arrow;

			// Ǯ �ʱ� ����: �ٴ� �Ʒ��� ������ ����
			obj->SetPosition(0.0f, -10000.0f, 0.0f);

			obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

			obj->CreateComponents(pd3dDevice, pd3dCommandList);

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = (UINT)b->objectRefs.size();

			m_arrowRefs.push_back(raw);
		}
	}
}

void CScene::BuildSkinnedBatch(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
	const std::shared_ptr<CSkinnedObjectsShader>& pSkinnedShader,
	UINT nRenderTargets,
	DXGI_FORMAT* rtvFormats,
	DXGI_FORMAT dsvFormat
)
{
	auto* b = &m_skinnedBatch;
	if (!b) return;

	const UINT cap = b->capacity;
	if (cap == 0) return;

	pSkinnedShader->CreateShader(
		pd3dDevice,
		m_pd3dGraphicsRootSignature.Get(),
		nRenderTargets,
		rtvFormats,
		dsvFormat
	);

	b->cbElementBytes = ((sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255);

	b->cbGameObjects = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList, nullptr,
		b->cbElementBytes * cap,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	b->cbGameObjects->Map(0, nullptr, (void**)&b->mappedGameObjects);

	b->baseCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
	b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	m_pDescriptorHeap->CreateConstantBufferViews(
		pd3dDevice,
		cap,
		b->cbGameObjects.Get(),
		b->cbElementBytes
	);

	m_skinnedObjects.clear();
	m_skinnedObjects.reserve(cap);

	b->objectRefs.clear();
	b->objectRefs.reserve(cap);

	b->count = 0;

	const UINT fighterCount = 3;
	const UINT zombieCount = 3;

	const XMFLOAT3 playerBase(0.0f, 0.0f, 0.0f);

	// ------------------------------------------------------------------------
	// Zombie (NPC)
	// ------------------------------------------------------------------------
	{
		AssetBuildDesc ZombieDesc =
		{
			AssetType::Zombie,
			"Assets/Zombie/Mesh/Zombie.bin",
			"Assets/Zombie/Texture"
		};

		BuiltAsset asset = AssetManager::BuildAsset(pd3dDevice, pd3dCommandList, m_pMaterials.get(), ZombieDesc);

		for (UINT k = 0; k < zombieCount; ++k)
		{
			if (b->objectRefs.size() >= b->capacity) break;

			const UINT i = (UINT)b->objectRefs.size();

			auto obj = std::make_unique<CGameObject>(1);

			auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
			obj->SetMappedGameObjectCB(cb);

			obj->SetMesh(0, asset.mesh);
			obj->AddComponent<CSkinnedMeshRendererComponent>();
			obj->AddComponent<CColliderComponent>(EColliderType::OOBB);


			// [ADD] Tag: NPC
			{
				auto* tag = obj->AddComponent<CActorTagComponent>();
				tag->kind = EActorKind::NPC;
				tag->control = EPlayerControl::None;
				tag->playerSlot = -1;
			}

			const float x = playerBase.x + 2.0f * (float)k;
			const float z = playerBase.z + 2.0f;

			obj->SetPosition(x, playerBase.y, z);
			obj->Rotate(0.0f, 180.0f, 0.0f);

			obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

			if (asset.mesh && asset.mesh->IsSkinnedMesh())
			{
				obj->EnableSkinning(pd3dDevice, asset.mesh->GetBoneCount());
			}

			AnimationClip idleClip;
			bool idleLoaded = false;

			auto mesh0 = obj->GetMeshShared(0);
			if (mesh0)
			{
				idleLoaded = mesh0->LoadAnimationFromBIN(
					"Assets/Zombie/Animation/ZombieIdle.bin",
					"Idle", idleClip, 1.0f
				);
			}

			if (idleLoaded)
			{
				idleClip.name = "Idle";

				CAnimator* anim = obj->EnsureAnimator();
				if (anim) anim->AddClip(idleClip);

				auto* ctrl = obj->EnsureAnimController();
				ctrl->SetIdleClip("Idle");
				ctrl->SetMoveClip("Idle");
				ctrl->SetSpeed(0.0f);
				ctrl->Update(0.0f);

				obj->Animate(0.0f);
			}

			obj->CreateComponents(pd3dDevice, pd3dCommandList);

			CGameObject* raw = obj.get();

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = (UINT)b->objectRefs.size();
		}
	}

	// ------------------------------------------------------------------------
	// Fighter (Remote Player 1..3)  +  m_demoFighters[0..2]
	// ------------------------------------------------------------------------
	{
		m_demoFighters.fill(nullptr);

		AssetBuildDesc FighterDesc =
		{
			AssetType::Fighter,
			"Assets/Fighter/Mesh/Fighter.bin",
			"Assets/Fighter/Texture"
		};

		BuiltAsset asset = AssetManager::BuildAsset(pd3dDevice, pd3dCommandList, m_pMaterials.get(), FighterDesc);

		m_playerCbElementBytes = ((sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255);

		m_pd3dcbPlayerGameObject = ::CreateBufferResource(
			pd3dDevice, pd3dCommandList, nullptr,
			m_playerCbElementBytes,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			nullptr
		);

		m_pd3dcbPlayerGameObject->Map(0, nullptr, (void**)&m_pcbMappedPlayerGameObject);

		m_playerCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
		m_pDescriptorHeap->CreateConstantBufferViews(
			pd3dDevice,
			1,
			m_pd3dcbPlayerGameObject.Get(),
			m_playerCbElementBytes
		);

		for (UINT k = 1; k < fighterCount + 1; ++k)
		{
			if (b->objectRefs.size() >= b->capacity) break;

			const UINT i = (UINT)b->objectRefs.size();

			auto obj = std::make_unique<CGameObject>(1);

			auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
			obj->SetMappedGameObjectCB(cb);

			obj->SetMesh(0, asset.mesh);
			obj->AddComponent<CSkinnedMeshRendererComponent>();
			obj->AddComponent<CColliderComponent>(EColliderType::OOBB);

			auto* animComp = obj->AddComponent<CAnimatorComponent>();

			// [ADD] Tag: Remote Player (slot = 1..3)
			{
				auto* tag = obj->AddComponent<CActorTagComponent>();
				tag->kind = EActorKind::Player;
				tag->control = EPlayerControl::Remote;
				tag->playerSlot = (int32_t)k; // k=1..3
			}

			UINT matId = 0;
			if (asset.mesh)
			{
				for (auto& sm : asset.mesh->m_SubMeshes)
				{
					if (sm.materialId == 0xFFFFFFFFu) continue;
					matId = sm.materialId;
					break;
				}
			}

			auto mat = std::make_shared<CMaterial>();
			mat->m_nReflection = matId;

			const float x = playerBase.x + 2.0f * (float)k;
			obj->SetPosition(x, playerBase.y, playerBase.z);

			obj->Rotate(0.0f, 0.0f, 0.0f);

			obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

			if (asset.mesh && asset.mesh->IsSkinnedMesh())
			{
				obj->EnableSkinning(pd3dDevice, asset.mesh->GetBoneCount());
			}

			auto mesh0 = obj->GetMeshShared(0);

			AnimationClip idleClip{};
			AnimationClip runClip{};
			AnimationClip atkClip{};
			bool idleLoaded = false;
			bool runLoaded = false;
			bool atkLoaded = false;

			if (mesh0)
			{
				idleLoaded = mesh0->LoadAnimationFromBIN(
					"Assets/Fighter/Animation/FighterIdle.bin",
					"Idle", idleClip, 1.0f
				);

				runLoaded = mesh0->LoadAnimationFromBIN(
					"Assets/Fighter/Animation/FighterRun.bin",
					"Run", runClip, 1.0f
				);

				atkLoaded = mesh0->LoadAnimationFromBIN(
					"Assets/Fighter/Animation/FighterAttack.bin",
					"Attack", atkClip, 1.0f
				);
			}

			if (animComp)
			{
				if (idleLoaded) { idleClip.name = "Idle";   animComp->AddClip(idleClip); }
				if (runLoaded) { runClip.name = "Run";    animComp->AddClip(runClip); }
				if (atkLoaded) { atkClip.name = "Attack"; animComp->AddClip(atkClip); }

				animComp->SetIdleClip("Idle");
				animComp->SetMoveClip(runLoaded ? "Run" : "Idle");

				auto* ctrl = animComp->EnsureController();
				if (ctrl)
				{
					ctrl->SetAttackClip("Attack");
					ctrl->SetSpeed(0.0f);
					ctrl->Update(0.0f);
				}
			}

			obj->CreateComponents(pd3dDevice, pd3dCommandList);
			if (animComp) animComp->EvaluatePose(0.0f);

			CGameObject* raw = obj.get();

			if ((k >= 1) && (k <= 3))
				m_demoFighters[k - 1] = raw;

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = (UINT)b->objectRefs.size();
		}
	}
}

void CScene::BuildPlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	AssetBuildDesc FighterDesc =
	{
		AssetType::Fighter,
		"Assets/Fighter/Mesh/Fighter.bin",
		"Assets/Fighter/Texture"
	};

	BuiltAsset asset = AssetManager::BuildAsset(pd3dDevice, pd3dCommandList, m_pMaterials.get(), FighterDesc);

	auto obj = std::make_shared<CGameObject>(1);
	obj->SetMappedGameObjectCB(m_pcbMappedPlayerGameObject);
	obj->SetCbvGPUDescriptorHandlePtr(m_playerCbvGpu.ptr);

	obj->SetMesh(0, asset.mesh);
	obj->AddComponent<CSkinnedMeshRendererComponent>();
	obj->AddComponent<CColliderComponent>(EColliderType::OOBB);

	// [ADD] Tag: Local Player (slot = 0)
	{
		auto* tag = obj->AddComponent<CActorTagComponent>();
		tag->kind = EActorKind::Player;
		tag->control = EPlayerControl::Local;
		tag->playerSlot = 0;
	}

	obj->SetPosition(0.0f, 0.0f, 0.0f);
	obj->Rotate(0.0f, 0.0f, 0.0f);

	if (asset.mesh && asset.mesh->IsSkinnedMesh())
	{
		obj->EnableSkinning(pd3dDevice, asset.mesh->GetBoneCount());
	}

	AnimationClip idleClip;
	bool idleLoaded = false;

	auto mesh0 = obj->GetMeshShared(0);
	if (mesh0)
	{
		idleLoaded = mesh0->LoadAnimationFromBIN(
			"Assets/Fighter/Animation/FighterIdle.bin",
			"Idle", idleClip, 1.0f
		);
	}

	if (idleLoaded)
	{
		idleClip.name = "Idle";

		CAnimator* anim = obj->EnsureAnimator();
		if (anim) anim->AddClip(idleClip);

		AnimationClip moveClip;
		bool moveLoaded = false;

		if (mesh0)
		{
			moveLoaded = mesh0->LoadAnimationFromBIN(
				"Assets/Fighter/Animation/FighterRun.bin",
				"Run", moveClip, 1.0f
			);
		}

		if (moveLoaded)
		{
			moveClip.name = "Run";
			if (anim) anim->AddClip(moveClip);
		}

		AnimationClip attackClip;
		bool attackLoaded = false;

		if (mesh0)
		{
			attackLoaded = mesh0->LoadAnimationFromBIN(
				"Assets/Fighter/Animation/FighterAttack.bin",
				"Attack", attackClip, 1.0f
			);
		}

		if (attackLoaded)
		{
			attackClip.name = "Attack";
			if (anim) anim->AddClip(attackClip);
		}

		auto* ctrl = obj->EnsureAnimController();
		ctrl->SetIdleClip("Idle");
		ctrl->SetMoveClip(moveLoaded ? "Run" : "Idle");
		ctrl->SetAttackClip("Attack");

		ctrl->SetSpeed(0.0f);
		ctrl->Update(0.0f);

		obj->Animate(0.0f);
	}

	obj->AddComponent<CPlayerControllerComponent>();

	obj->CreateComponents(pd3dDevice, pd3dCommandList);

	m_pPlayer = obj;
}

void CScene::CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* target)
{
	m_pMainCameraObject = std::make_unique<CGameObject>(0);

	auto* cam = m_pMainCameraObject->AddComponent<CThirdPersonCamera>();
	m_pMainCamera = cam;

	cam->SetMode(THIRD_PERSON_CAMERA);
	cam->SetTarget(target);

	cam->SetTimeLag(0.25f);
	cam->SetOffset(XMFLOAT3(0.0f, 1.0f, -2.0f));
	cam->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
	cam->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
	cam->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);

	m_pMainCameraObject->CreateComponents(dev, cmd);

	if (target)
	{
		XMFLOAT3 pos = target->GetPosition();
		cam->SetPosition(Vector3::Add(pos, cam->GetOffset()));
		cam->Update(pos, 0.0f);
		cam->SetLookAt(pos);
		cam->RegenerateViewMatrix();
	}
}

CGameObject* CScene::GetDemoFighter(int index) const
{
	if (index < 0 || index >= (int)m_demoFighters.size()) return nullptr;
	return m_demoFighters[(size_t)index];
}

void CScene::RequestDemoFighterAttack(int index)
{
	// ���� API ����: index(0..2) -> slot(1..3)
	const int slot = index + 1;
	RequestPlayerAttackBySlot(slot);
}

void CScene::RequestPlayerAttackBySlot(int slot)
{
	CGameObject* obj = GetPlayerBySlot(slot);
	if (!obj) return;

	// ȭ�� �Ķ����(���ϸ� ���⸸ ����)
	const float kArrowSpeed = 3.0f;
	const float kArrowLife = 6.0f;
	const float kArrowYOffset = 1.0f; // �߻� �� pos.y += yOffset

	// AnimatorComponent ���
	if (auto* animComp = obj->GetComponent<CAnimatorComponent>())
	{
		if (auto* ctrl = animComp->EnsureController())
		{
			ctrl->RequestAttack();

			// ���� ��û�� ���ÿ� ȭ�� �߻�(���� 0..3 ����)
			RequestFireArrow(obj, kArrowSpeed, kArrowLife, kArrowYOffset);
			return;
		}
	}

	// ���Ž� AnimController ���
	if (auto* ctrl = obj->GetAnimController())
	{
		ctrl->RequestAttack();

		// ���Žõ� �����ϰ� ȭ�� �߻�
		RequestFireArrow(obj, kArrowSpeed, kArrowLife, kArrowYOffset);
		return;
	}

	// Animator�� �ִ� ���: ���ݵ�/ȭ�쵵 ���⼭�� ���� �Ұ�
}

CGameObject* CScene::GetPlayerBySlot(int slot) const
{
	if (slot < 0 || slot > 3) return nullptr;

	// slot 0�� m_pPlayer�� ����
	if (slot == 0) return m_pPlayer.get();

	// slot 1..3�� �±� or m_demoFighters ���
	// (4�� �����̸� m_demoFighters�� ���� ����)
	return m_demoFighters[(size_t)(slot - 1)];
}

bool CScene::IsLocalPlayer(const CGameObject* obj) const
{
	if (!obj) return false;
	auto* tag = obj->GetComponent<CActorTagComponent>();
	return tag && tag->kind == EActorKind::Player && tag->control == EPlayerControl::Local;
}

bool CScene::OnProcessingMouseMessage(HWND /*hWnd*/, UINT nMessageID, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	if (nMessageID == WM_LBUTTONDOWN)
	{
		RequestPlayerAttackBySlot(0);
		return true;
	}
	return false;
}
bool CScene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	return(false);
}

void CScene::RequestFireArrow(CGameObject* shooter, float speed, float lifeSec, float yOffset)
{
	if (!shooter) return;

	// 1) �߻� ���� = shooter�� Look ����(������� basis���� ����)
	const XMFLOAT4X4& W = shooter->GetWorldMatrix();

	// �� ������Ʈ�� CPU���� row-major�� ��� �ְ� ���̴��� �ѱ� �� Transpose �ϰ� �����Ƿ�,
	// Look(Forward) ���ʹ� ���� 3��° ��(_31,_32,_33)�� ����ִ�.
	XMFLOAT3 dir = { W._31, W._32, W._33 };

	XMVECTOR dirV = XMLoadFloat3(&dir);
	const float lenSq = XMVectorGetX(XMVector3LengthSq(dirV));
	if (lenSq < 1e-8f)
	{
		// ������(������)�̸� �⺻ ����
		dir = XMFLOAT3(0.0f, 0.0f, 1.0f);
		dirV = XMLoadFloat3(&dir);
	}

	dirV = XMVector3Normalize(dirV);

	XMFLOAT3 dirN{};
	XMStoreFloat3(&dirN, dirV);

	// 2) ���� ��ġ = shooter ��ġ + yOffset
	XMFLOAT3 startPos = shooter->GetPosition();
	startPos.y += yOffset;

	// 3) �ӵ� ���� = Look * speed
	const XMFLOAT3 vel =
	{
		dirN.x * speed,
		dirN.y * speed,
		dirN.z * speed
	};

	// 4) Ǯ���� ��Ȱ�� ȭ�� ã�� Activate + Look �������� ȸ�� ����
	for (CGameObject* arrowObj : m_arrowRefs)
	{
		if (!arrowObj) continue;

		auto* arrow = arrowObj->GetComponent<CArrowComponent>();
		if (!arrow) continue;

		if (arrow->IsActive()) continue;

		// (A) Ʈ������ ȸ��: Look ���� ����
		if (auto* tr = arrowObj->GetComponent<CTransformComponent>())
		{
			tr->SetLookDirection(dirN);
		}

		// (B) �̵�����(vel) + ��ġ(startPos) ����
		arrow->Activate(startPos, vel, lifeSec);
		return;
	}

	// Ǯ �����̸� ����(���ϸ� ���⼭ ���� Ȯ�� ����)
}

bool CScene::ProcessInput(UCHAR* pKeysBuffer)
{
	return(false);
}

void CScene::AnimateObjects(float fTimeElapsed)
{
	for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
	{
		if (!m_staticObjects[j]) continue;
		m_staticObjects[j]->Animate(fTimeElapsed);
	}

	for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
	{
		if (!m_skinnedObjects[j]) continue;
		m_skinnedObjects[j]->Animate(fTimeElapsed);
	}
	if (m_pPlayer)
		m_pPlayer->Animate(fTimeElapsed);

	if (m_pPlayer && m_pPlayerSpotFollower && (m_pPlayerSpotFollower->GetTarget() == nullptr))
	{
		m_pPlayerSpotFollower->SetTarget(m_pPlayer.get());
	}

	for (UINT j = 0; j < (UINT)m_lightObjects.size(); ++j)
	{
		if (!m_lightObjects[j]) continue;
		m_lightObjects[j]->Animate(fTimeElapsed);
	}
}

void CScene::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (m_pcbMappedLights)
	{
		::ZeroMemory(m_pcbMappedLights, sizeof(LIGHTS));
		m_pcbMappedLights->m_xmf4GlobalAmbient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);

		UINT li = 0;
		for (auto& obj : m_lightObjects)
		{
			if (!obj) continue;

			auto* lc = obj->GetComponent<CLightComponent>();
			if (!lc) continue;
			if (!lc->IsEnabled()) continue;
			if (li >= MAX_LIGHTS) break;

			lc->Fill(m_pcbMappedLights->m_pLights[li]);
			++li;
		}
	}

	::memcpy(m_pcbMappedMaterials, m_pMaterials.get(), sizeof(MATERIALS));

	if (m_staticBatch.mappedGameObjects && !m_staticBatch.objectRefs.empty())
	{
		const UINT ncb = m_staticBatch.cbElementBytes;

		for (UINT j = 0; j < (UINT)m_staticBatch.objectRefs.size(); ++j)
		{
			auto* obj = m_staticBatch.objectRefs[j];
			if (!obj) continue;

			auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)m_staticBatch.mappedGameObjects + j * ncb);

			const XMFLOAT4X4& W = obj->GetWorldMatrix();

			XMStoreFloat4x4(
				&cb->m_xmf4x4World,
				XMMatrixTranspose(XMLoadFloat4x4(&W))
			);

			cb->m_nObjectID = j;
		}
	}

	if (m_skinnedBatch.mappedGameObjects && !m_skinnedBatch.objectRefs.empty())
	{
		const UINT ncb = m_skinnedBatch.cbElementBytes;

		for (UINT j = 0; j < (UINT)m_skinnedBatch.objectRefs.size(); ++j)
		{
			auto* obj = m_skinnedBatch.objectRefs[j];
			if (!obj) continue;

			auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)m_skinnedBatch.mappedGameObjects + j * ncb);

			const XMFLOAT4X4& W = obj->GetWorldMatrix();

			XMStoreFloat4x4(
				&cb->m_xmf4x4World,
				XMMatrixTranspose(XMLoadFloat4x4(&W))
			);

			cb->m_nObjectID = j;
		}
	}

	if (m_pPlayer && m_pcbMappedPlayerGameObject)
	{
		const XMFLOAT4X4& W = m_pPlayer->GetWorldMatrix();

		XMStoreFloat4x4(
			&m_pcbMappedPlayerGameObject->m_xmf4x4World,
			XMMatrixTranspose(XMLoadFloat4x4(&W))
		);

		m_pcbMappedPlayerGameObject->m_nObjectID = 0;
	}
}

void CScene::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get());
	pd3dCommandList->SetDescriptorHeaps(1, m_pDescriptorHeap->m_pd3dCbvSrvDescriptorHeap.GetAddressOf());

	pd3dCommandList->SetGraphicsRootDescriptorTable(
		ROOT_PARAMETER_GLOBAL_SRV,
		m_pDescriptorHeap->GetGPUSrvDescriptorStartHandle()
	);

	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pCamera->UpdateShaderVariables(pd3dCommandList);

	UpdateShaderVariables(pd3dCommandList);

	D3D12_GPU_VIRTUAL_ADDRESS d3dcbLightsGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_LIGHT, d3dcbLightsGpuVirtualAddress); //Lights

	D3D12_GPU_VIRTUAL_ADDRESS d3dcbMaterialsGpuVirtualAddress = m_pd3dcbMaterials->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_MATERIAL, d3dcbMaterialsGpuVirtualAddress); //Materials
}

void CScene::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	if (m_staticBatch.shader)
	{
		m_staticBatch.shader->Render(pd3dCommandList, pCamera, &m_staticBatch);
		for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
		{
			if (!m_staticObjects[j]) continue;
			m_staticObjects[j]->Render(pd3dCommandList, pCamera);
		}
	}

	if (m_skinnedBatch.shader)
	{
		m_skinnedBatch.shader->Render(pd3dCommandList, pCamera, &m_skinnedBatch);
		for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
		{
			if (!m_skinnedObjects[j]) continue;
			m_skinnedObjects[j]->Render(pd3dCommandList, pCamera);
		}
	}

	if (m_pPlayer)
		m_pPlayer->Render(pd3dCommandList, pCamera);
}
