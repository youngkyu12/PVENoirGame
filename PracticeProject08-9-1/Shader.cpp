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

	ComPtr<ID3DBlob> pd3dErrorBlob;
	::D3DCompileFromFile(
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

	D3D12_SHADER_BYTECODE d3dShaderByteCode;
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
	ReleaseShaderVariables();
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
}

CStaticObjectsShader::~CStaticObjectsShader()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CSkinnedObjectsShader::CSkinnedObjectsShader()
{
}

CSkinnedObjectsShader::~CSkinnedObjectsShader()
{
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


