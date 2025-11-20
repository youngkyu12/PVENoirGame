#include "stdafx.h"
#include "Scene.h"


void CScene::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
{

	//매개변수가 없는 루트 시그너쳐를 생성한다.
	D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
	::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
	d3dRootSignatureDesc.NumParameters = 0;
	d3dRootSignatureDesc.pParameters = NULL;
	d3dRootSignatureDesc.NumStaticSamplers = 0;
	d3dRootSignatureDesc.pStaticSamplers = NULL;
	d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* pd3dSignatureBlob = NULL;
	ID3DBlob* pd3dErrorBlob = NULL;

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

	if (pd3dSignatureBlob) pd3dSignatureBlob->Release();
	if (pd3dErrorBlob) pd3dErrorBlob->Release();
}

ComPtr<ID3D12RootSignature> CScene::GetGraphicsRootSignature()
{
	return(m_pd3dGraphicsRootSignature);
}

void CScene::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	//그래픽 루트 시그너쳐를 생성한다.
	CreateGraphicsRootSignature(pd3dDevice);

	//씬을 그리기 위한 셰이더 객체를 생성한다.
	m_nShaders = 1;

	m_ppShaders.emplace_back(make_shared<CShader>());

	m_ppShaders.back()->CreateShader(pd3dDevice, m_pd3dGraphicsRootSignature.Get());
	m_ppShaders.back()->BuildObjects(pd3dDevice, pd3dCommandList, NULL);
}