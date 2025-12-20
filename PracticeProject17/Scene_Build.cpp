#include "stdafx.h"
#include "Scene.h"


void CScene::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
	D3D12_ROOT_PARAMETER pd3dRootParameters[5];
	pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[0].Descriptor.ShaderRegister = 0; //Player
	pd3dRootParameters[0].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[1].Descriptor.ShaderRegister = 1; //Camera
	pd3dRootParameters[1].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[2].Descriptor.ShaderRegister = 2; //GameObject
	pd3dRootParameters[2].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[3].Descriptor.ShaderRegister = 3; //Materials
	pd3dRootParameters[3].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[4].Descriptor.ShaderRegister = 4; //Lights
	pd3dRootParameters[4].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
	::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
	d3dRootSignatureDesc.NumParameters = _countof(pd3dRootParameters);
	d3dRootSignatureDesc.pParameters = pd3dRootParameters;
	d3dRootSignatureDesc.NumStaticSamplers = 0;
	d3dRootSignatureDesc.pStaticSamplers = NULL;
	d3dRootSignatureDesc.Flags = d3dRootSignatureFlags;

	ComPtr<ID3DBlob> pd3dSignatureBlob;
	ComPtr<ID3DBlob> pd3dErrorBlob;

	::D3D12SerializeRootSignature(
		&d3dRootSignatureDesc, 
		D3D_ROOT_SIGNATURE_VERSION_1,
		&pd3dSignatureBlob, 
		&pd3dErrorBlob
	);

	pd3dDevice->CreateRootSignature(
		0, 
		pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(), 
		IID_PPV_ARGS(&m_pd3dGraphicsRootSignature)
	);
}

ComPtr<ID3D12RootSignature> CScene::GetGraphicsRootSignature()
{
	return (m_pd3dGraphicsRootSignature);
}

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
		XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), 
		XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f), 
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
}

void CScene::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	UINT ncbElementBytes = ((sizeof(LIGHTS) + 255) & ~255); //256의 배수
	m_pd3dcbLights = ::CreateBufferResource(
		pd3dDevice, 
		pd3dCommandList, 
		NULL,
		ncbElementBytes, 
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 
		NULL
	);
	m_pd3dcbLights->Map(0, NULL, (void**)&m_pcbMappedLights);

	UINT ncbMaterialBytes = ((sizeof(MATERIALS) + 255) & ~255); //256의 배수
	m_pd3dcbMaterials = ::CreateBufferResource(
		pd3dDevice, 
		pd3dCommandList, 
		NULL,
		ncbMaterialBytes, 
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 
		NULL
	);
	m_pd3dcbMaterials->Map(0, NULL, (void**)&m_pcbMappedMaterials);
}

void CScene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	CreateGraphicsRootSignature(pd3dDevice);

	m_nShaders = 1;
	m_ppShaders.resize(m_nShaders);

	m_ppShaders[0] = make_shared<CObjectsShader>();
	m_ppShaders[0]->CreateShader(pd3dDevice, m_pd3dGraphicsRootSignature.Get());
	m_ppShaders[0]->BuildObjects(pd3dDevice, pd3dCommandList);

	BuildLightsAndMaterials();

	CreateShaderVariables(pd3dDevice, pd3dCommandList);
}