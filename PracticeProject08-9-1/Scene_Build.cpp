//-----------------------------------------------------------------------------
// File: CScene_Build.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"

void CScene::BuildLightsAndMaterials()
{
	m_pLights = make_unique<LIGHTS>();
	::ZeroMemory(m_pLights.get(), sizeof(LIGHTS));

	m_pLights->m_xmf4GlobalAmbient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);

	m_pLights->m_pLights[0].m_bEnable = true;
	m_pLights->m_pLights[0].m_nType = POINT_LIGHT;
	m_pLights->m_pLights[0].m_fRange = 100.0f;
	m_pLights->m_pLights[0].m_xmf4Ambient = XMFLOAT4(0.1f, 0.0f, 0.0f, 1.0f);
	m_pLights->m_pLights[0].m_xmf4Diffuse = XMFLOAT4(0.8f, 0.0f, 0.0f, 1.0f);
	m_pLights->m_pLights[0].m_xmf4Specular = XMFLOAT4(0.1f, 0.1f, 0.1f, 0.0f);
	m_pLights->m_pLights[0].m_xmf3Position = XMFLOAT3(130.0f, 30.0f, 30.0f);
	m_pLights->m_pLights[0].m_xmf3Direction = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_pLights->m_pLights[0].m_xmf3Attenuation = XMFLOAT3(1.0f, 0.001f, 0.0001f);

	m_pLights->m_pLights[1].m_bEnable = true;
	m_pLights->m_pLights[1].m_nType = SPOT_LIGHT;
	m_pLights->m_pLights[1].m_fRange = 50.0f;
	m_pLights->m_pLights[1].m_xmf4Ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	m_pLights->m_pLights[1].m_xmf4Diffuse = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
	m_pLights->m_pLights[1].m_xmf4Specular = XMFLOAT4(0.1f, 0.1f, 0.1f, 0.0f);
	m_pLights->m_pLights[1].m_xmf3Position = XMFLOAT3(-50.0f, 20.0f, -5.0f);
	m_pLights->m_pLights[1].m_xmf3Direction = XMFLOAT3(0.0f, 0.0f, 1.0f);
	m_pLights->m_pLights[1].m_xmf3Attenuation = XMFLOAT3(1.0f, 0.01f, 0.0001f);
	m_pLights->m_pLights[1].m_fFalloff = 8.0f;
	m_pLights->m_pLights[1].m_fPhi = (float)cos(XMConvertToRadians(40.0f));
	m_pLights->m_pLights[1].m_fTheta = (float)cos(XMConvertToRadians(20.0f));

	m_pLights->m_pLights[2].m_bEnable = true;
	m_pLights->m_pLights[2].m_nType = DIRECTIONAL_LIGHT;
	m_pLights->m_pLights[2].m_xmf4Ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	m_pLights->m_pLights[2].m_xmf4Diffuse = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
	m_pLights->m_pLights[2].m_xmf4Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_pLights->m_pLights[2].m_xmf3Direction = XMFLOAT3(1.0f, 0.0f, 0.0f);

	m_pLights->m_pLights[3].m_bEnable = true;
	m_pLights->m_pLights[3].m_nType = SPOT_LIGHT;
	m_pLights->m_pLights[3].m_fRange = 60.0f;
	m_pLights->m_pLights[3].m_xmf4Ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
	m_pLights->m_pLights[3].m_xmf4Diffuse = XMFLOAT4(0.5f, 0.0f, 0.0f, 1.0f);
	m_pLights->m_pLights[3].m_xmf4Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	m_pLights->m_pLights[3].m_xmf3Position = XMFLOAT3(-150.0f, 30.0f, 30.0f);
	m_pLights->m_pLights[3].m_xmf3Direction = XMFLOAT3(0.0f, 1.0f, 1.0f);
	m_pLights->m_pLights[3].m_xmf3Attenuation = XMFLOAT3(1.0f, 0.01f, 0.0001f);
	m_pLights->m_pLights[3].m_fFalloff = 8.0f;
	m_pLights->m_pLights[3].m_fPhi = (float)cos(XMConvertToRadians(90.0f));
	m_pLights->m_pLights[3].m_fTheta = (float)cos(XMConvertToRadians(30.0f));

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

void CScene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	CreateGraphicsRootSignature(pd3dDevice);

	m_nShaders = 1;
	m_ppShaders.resize(m_nShaders);

	shared_ptr<CObjectsShader> pObjectShader = make_shared<CObjectsShader>();
	int nObjects = pObjectShader->GetNumberOfObjects();
	constexpr int MAX_GLOBAL_SRVS = 1024;

	m_pDescriptorHeap->CreateCbvSrvDescriptorHeaps(
		pd3dDevice,
		nObjects + 1 + 1 + 1,
		MAX_GLOBAL_SRVS
	);

	DXGI_FORMAT pdxgiRtvFormats[5] = {
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R32_FLOAT
	};
	pObjectShader->CreateShader(
		pd3dDevice,
		m_pd3dGraphicsRootSignature.Get(),
		5,
		pdxgiRtvFormats,
		DXGI_FORMAT_D24_UNORM_S8_UINT/*DXGI_FORMAT_D32_FLOAT*/
	);
	BuildLightsAndMaterials();

	pObjectShader->BuildObjects(pd3dDevice, pd3dCommandList, m_pMaterials.get());
	m_ppShaders[0] = pObjectShader;

	CreateShaderVariables(pd3dDevice, pd3dCommandList);
}

void CScene::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
	// (1) Descriptor Ranges: CBV table 1개 + Global SRV table 1개만 남긴다.
	D3D12_DESCRIPTOR_RANGE pd3dDescriptorRanges[2] = {};

	// b2: Game Objects (CBV table)
	pd3dDescriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	pd3dDescriptorRanges[0].NumDescriptors = 1;
	pd3dDescriptorRanges[0].BaseShaderRegister = 2; // b2
	pd3dDescriptorRanges[0].RegisterSpace = 0;
	pd3dDescriptorRanges[0].OffsetInDescriptorsFromTableStart = 0;

	// Global Texture2D pool: t1~t1024, space1 (SRV table)
	constexpr UINT MAX_GLOBAL_SRVS = 1024;
	pd3dDescriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[1].NumDescriptors = MAX_GLOBAL_SRVS;
	pd3dDescriptorRanges[1].BaseShaderRegister = 1; // t1부터
	pd3dDescriptorRanges[1].RegisterSpace = 1;      // space1
	pd3dDescriptorRanges[1].OffsetInDescriptorsFromTableStart = 0;

	// (2) Root Parameters: SRV 분리(5/6) 제거, Global SRV 하나만 유지
	D3D12_ROOT_PARAMETER pd3dRootParameters[7] = {};

	// [0] b1: Camera
	pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[0].Descriptor.ShaderRegister = 1;
	pd3dRootParameters[0].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// [1] b0: Player
	pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[1].Descriptor.ShaderRegister = 0;
	pd3dRootParameters[1].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// [2] (Table) b2: Game Objects
	pd3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[2].DescriptorTable.pDescriptorRanges = &pd3dDescriptorRanges[0];
	pd3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// [3] b3: Materials
	pd3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[3].Descriptor.ShaderRegister = 3;
	pd3dRootParameters[3].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// [4] b4: Lights
	pd3dRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[4].Descriptor.ShaderRegister = 4;
	pd3dRootParameters[4].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// [5] b5: DrawOptions (PostProcessing 옵션 + SRV 인덱스들)
	pd3dRootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[5].Descriptor.ShaderRegister = 5;
	pd3dRootParameters[5].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// [6] (Table) Global SRV table (space1, t1~)
	pd3dRootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[6].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[6].DescriptorTable.pDescriptorRanges = &pd3dDescriptorRanges[1];
	pd3dRootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// Static sampler (s0)
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
	UINT ncbElementBytes = ((sizeof(LIGHTS) + 255) & ~255); //256의 배수
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

	UINT ncbMaterialBytes = ((sizeof(MATERIALS) + 255) & ~255); //256의 배수
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