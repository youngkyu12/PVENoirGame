///-----------------------------------------------------------------------------
// File: Shader.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Shader.h"
#include "DDSTextureLoader12.h"
#include "Scene.h"
#include "Material.h"



CShader::CShader()
{
}

CShader::~CShader()
{
	if (m_pd3dPipelineState) 
		m_pd3dPipelineState.Reset();

	if (m_pd3dGraphicsRootSignature) 
		m_pd3dGraphicsRootSignature.Reset();
}

void CShader::ReleaseShaderVariables()
{
}

void CShader::ReleaseUploadBuffers()
{
}

D3D12_SHADER_BYTECODE CShader::CompileShaderFromFile(const WCHAR* pszFileName, LPCSTR pszShaderName, LPCSTR pszShaderProfile, ID3DBlob** ppd3dShaderBlob)
{
	UINT nCompileFlags = 0;
#if defined(_DEBUG)
	nCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	D3D12_SHADER_BYTECODE d3dShaderByteCode = {};
	ComPtr<ID3DBlob> pd3dErrorBlob;
	HRESULT hResult = ::D3DCompileFromFile(
		pszFileName,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		pszShaderName,
		pszShaderProfile,
		nCompileFlags,
		0,
		ppd3dShaderBlob,
		&pd3dErrorBlob
	);

	if (FAILED(hResult)) {
#if defined(_DEBUG)
		if (pd3dErrorBlob)
		{
			const char* msg = static_cast<const char*>(pd3dErrorBlob->GetBufferPointer());
			::OutputDebugStringA("D3DCompileFromFile failed:\n");
			::OutputDebugStringA(msg ? msg : "(no message)\n");
		}
		else
		{
			::OutputDebugStringA("D3DCompileFromFile failed (no error blob).\n");
		}
#endif
		return (d3dShaderByteCode);
	}

	d3dShaderByteCode.BytecodeLength = (*ppd3dShaderBlob)->GetBufferSize();
	d3dShaderByteCode.pShaderBytecode = (*ppd3dShaderBlob)->GetBufferPointer();

	return(d3dShaderByteCode);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CPlayerShader::CPlayerShader()
{
}

CPlayerShader::~CPlayerShader()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CTexturedShader::CTexturedShader()
{
}

CTexturedShader::~CTexturedShader()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CIlluminatedTexturedShader::CIlluminatedTexturedShader()
{
}

CIlluminatedTexturedShader::~CIlluminatedTexturedShader()
{
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CStaticObjectsShader::CStaticObjectsShader()
{
	//m_xObjects = 10, m_yObjects = 10, m_zObjects = 10;
	//m_nObjects = (m_xObjects * 2 + 1) * (m_yObjects * 2 + 1) * (m_zObjects * 2 + 1);
	m_nObjects = 1;
}

CStaticObjectsShader::~CStaticObjectsShader()
{
}

void CStaticObjectsShader::ReleaseShaderVariables()
{
	if (m_pd3dcbGameObjects)
	{
		m_pd3dcbGameObjects->Unmap(0, NULL);
		m_pd3dcbGameObjects.Reset();
	}

	CIlluminatedTexturedShader::ReleaseShaderVariables();
}

void CStaticObjectsShader::ReleaseObjects()
{
}

void CStaticObjectsShader::ReleaseUploadBuffers()
{
	if (!m_ppObjects.empty())
	{
		for (int j = 0; j < m_nObjects; j++)
			if (m_ppObjects[j])
				m_ppObjects[j]->ReleaseUploadBuffers();
	}

#ifdef _WITH_BATCH_MATERIAL
	if (m_pMaterial)
		m_pMaterial->ReleaseUploadBuffers();
#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CSkinnedObjectsShader::CSkinnedObjectsShader()
{
	//m_xObjects = 10, m_yObjects = 10, m_zObjects = 10;
	//m_nObjects = (m_xObjects * 2 + 1) * (m_yObjects * 2 + 1) * (m_zObjects * 2 + 1);
	m_nObjects = 1;
}

CSkinnedObjectsShader::~CSkinnedObjectsShader()
{
}

void CSkinnedObjectsShader::ReleaseShaderVariables()
{
	if (m_pd3dcbGameObjects)
	{
		m_pd3dcbGameObjects->Unmap(0, NULL);
		m_pd3dcbGameObjects.Reset();
	}

	CIlluminatedTexturedShader::ReleaseShaderVariables();
}

void CSkinnedObjectsShader::ReleaseObjects()
{
}

void CSkinnedObjectsShader::ReleaseUploadBuffers()
{
	if (!m_ppObjects.empty())
	{
		for (int j = 0; j < m_nObjects; j++)
			if (m_ppObjects[j])
				m_ppObjects[j]->ReleaseUploadBuffers();
	}

#ifdef _WITH_BATCH_MATERIAL
	if (m_pMaterial)
		m_pMaterial->ReleaseUploadBuffers();
#endif
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CPostProcessingShader::CPostProcessingShader()
{
}

CPostProcessingShader::~CPostProcessingShader()
{

}



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CTextureToFullScreenShader::CTextureToFullScreenShader()
{
}

CTextureToFullScreenShader::~CTextureToFullScreenShader()
{
	ReleaseShaderVariables();
}

void CTextureToFullScreenShader::ReleaseShaderVariables()
{
	if (m_pd3dcbDrawOptions) 
		m_pd3dcbDrawOptions.Reset();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
D3D12_INPUT_LAYOUT_DESC CTerrainShader::CreateInputLayout()
{
	UINT nInputElementDescs = 4;
	D3D12_INPUT_ELEMENT_DESC* pd3dInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	pd3dInputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[1] = { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[2] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[3] = { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
	d3dInputLayoutDesc.pInputElementDescs = pd3dInputElementDescs;
	d3dInputLayoutDesc.NumElements = nInputElementDescs;

	return(d3dInputLayoutDesc);
}

D3D12_SHADER_BYTECODE CTerrainShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSTerrain", "vs_5_1", ppd3dShaderBlob));
}

D3D12_SHADER_BYTECODE CTerrainShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "PSTerrain", "ps_5_1", ppd3dShaderBlob));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
D3D12_INPUT_LAYOUT_DESC CTerrainWaterShader::CreateInputLayout()
{
	UINT nInputElementDescs = 2;
	D3D12_INPUT_ELEMENT_DESC* pd3dInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	pd3dInputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	pd3dInputElementDescs[1] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
	d3dInputLayoutDesc.pInputElementDescs = pd3dInputElementDescs;
	d3dInputLayoutDesc.NumElements = nInputElementDescs;

	return(d3dInputLayoutDesc);
}

D3D12_SHADER_BYTECODE CTerrainWaterShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSTerrainWater", "vs_5_1", ppd3dShaderBlob));
}

D3D12_SHADER_BYTECODE CTerrainWaterShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "PSTerrainWater", "ps_5_1", ppd3dShaderBlob));
}
