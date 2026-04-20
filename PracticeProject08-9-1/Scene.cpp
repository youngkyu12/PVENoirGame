//-----------------------------------------------------------------------------
// File: Scene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"
#include "Camera.h"
#include "Object.h"

#include "GlobalValues.h"

std::unique_ptr<CDescriptorHeap> CScene::m_pDescriptorHeap = std::make_unique<CDescriptorHeap>();

CScene::CScene()
{
}

CScene::~CScene()
{
}

void CScene::ReleaseObjects()
{
    // 카메라(컴포넌트) 리소스 정리
    if (m_pMainCamera)
    {
        m_pMainCamera->ReleaseShaderVariables();
        m_pMainCamera = nullptr;
    }

    m_pMainCameraObject.reset();

    if (m_pd3dGraphicsRootSignature)
        m_pd3dGraphicsRootSignature.Reset();
}

void CScene::OnPrepareRender(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
    if (!cmd) return;

    cmd->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get());

    if (m_pDescriptorHeap && m_pDescriptorHeap->m_pd3dCbvSrvDescriptorHeap)
    {
        cmd->SetDescriptorHeaps(1, m_pDescriptorHeap->m_pd3dCbvSrvDescriptorHeap.GetAddressOf());
        cmd->SetGraphicsRootDescriptorTable(
            ROOT_PARAMETER_GLOBAL_SRV,
            m_pDescriptorHeap->GetGPUSrvDescriptorStartHandle()
        );
    }

    if (camera)
    {
        camera->SetViewportsAndScissorRects(cmd);
        camera->UpdateShaderVariables(cmd);
    }
}

void CScene::DequeueNetworkMessage(const NetworkMessageType& type)
{
    m_pendingNetworkMessage.type = type;	
    g_NetworkQueue.TryPop(m_pendingNetworkMessage); // 기존 메시지 처리
}

void CScene::SetNetworkMessageType(NetworkMessageType type)
{
	
}


void CScene::CreateGraphicsRootSignature(ID3D12Device* dev)
{
    if (!dev) return;

    D3D12_DESCRIPTOR_RANGE pd3dDescriptorRanges[2] = {};

    pd3dDescriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    pd3dDescriptorRanges[0].NumDescriptors = 1;
    pd3dDescriptorRanges[0].BaseShaderRegister = 2; // b2
    pd3dDescriptorRanges[0].RegisterSpace = 0;
    pd3dDescriptorRanges[0].OffsetInDescriptorsFromTableStart = 0;

    pd3dDescriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    pd3dDescriptorRanges[1].NumDescriptors = GLOBAL_SRV_CAPACITY;
    pd3dDescriptorRanges[1].BaseShaderRegister = 0;
    pd3dDescriptorRanges[1].RegisterSpace = 0;
    pd3dDescriptorRanges[1].OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER pd3dRootParameters[11] = {};

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

	pd3dRootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
	pd3dRootParameters[8].Descriptor.ShaderRegister = 0; // t0
	pd3dRootParameters[8].Descriptor.RegisterSpace = 1;  // space1
	pd3dRootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	pd3dRootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[9].Descriptor.ShaderRegister = 7; // b7
	pd3dRootParameters[9].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	pd3dRootParameters[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[10].Descriptor.ShaderRegister = 8; // b8
	pd3dRootParameters[10].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_STATIC_SAMPLER_DESC d3dSamplerDescs[2] = {};

	d3dSamplerDescs[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	d3dSamplerDescs[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	d3dSamplerDescs[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	d3dSamplerDescs[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	d3dSamplerDescs[0].MipLODBias = 0;
	d3dSamplerDescs[0].MaxAnisotropy = 1;
	d3dSamplerDescs[0].ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	d3dSamplerDescs[0].MinLOD = 0;
	d3dSamplerDescs[0].MaxLOD = D3D12_FLOAT32_MAX;
	d3dSamplerDescs[0].ShaderRegister = 0; // s0
	d3dSamplerDescs[0].RegisterSpace = 0;
	d3dSamplerDescs[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	d3dSamplerDescs[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	d3dSamplerDescs[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	d3dSamplerDescs[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	d3dSamplerDescs[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	d3dSamplerDescs[1].MipLODBias = 0;
	d3dSamplerDescs[1].MaxAnisotropy = 1;
	d3dSamplerDescs[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	d3dSamplerDescs[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	d3dSamplerDescs[1].MinLOD = 0;
	d3dSamplerDescs[1].MaxLOD = D3D12_FLOAT32_MAX;
	d3dSamplerDescs[1].ShaderRegister = 1; // s1
	d3dSamplerDescs[1].RegisterSpace = 0;
	d3dSamplerDescs[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_FLAGS flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = _countof(pd3dRootParameters);
    desc.pParameters = pd3dRootParameters;
	desc.NumStaticSamplers = _countof(d3dSamplerDescs);
	desc.pStaticSamplers = d3dSamplerDescs;
    desc.Flags = flags;

    ComPtr<ID3DBlob> sigBlob;
    ComPtr<ID3DBlob> errBlob;

    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob) OutputDebugStringA((char*)errBlob->GetBufferPointer());
        return;
    }

    hr = dev->CreateRootSignature(
        0,
        sigBlob->GetBufferPointer(),
        sigBlob->GetBufferSize(),
        IID_PPV_ARGS(m_pd3dGraphicsRootSignature.ReleaseAndGetAddressOf()));

    if (FAILED(hr))
        OutputDebugStringA("CreateRootSignature failed.\n");
}
