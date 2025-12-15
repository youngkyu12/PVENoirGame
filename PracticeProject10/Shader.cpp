#include "stdafx.h"
#include "Shader.h"

void CObjectsShader::ReleaseObjects()
{
	for (int i = 0; i < m_nd3dPipelineState; ++i)
		if (m_ppd3dPipelineStates[i])
			m_ppd3dPipelineStates[i].Reset();

	for (int i = 0; i < m_nObjects; ++i)
		m_ppObjects[i]->ReleaseObjects();
}

void CObjectsShader::ReleaseUploadBuffers()
{
		for (int i = 0; i < m_nObjects; ++i) 
			m_ppObjects[i]->ReleaseUploadBuffers();
}

void CShader::ReleaseObjects()
{
}

void CShader::ReleaseUploadBuffers()
{
}

//셰이더 소스 코드를 컴파일하여 바이트 코드 구조체를 반환한다. 
D3D12_SHADER_BYTECODE CShader::CompileShaderFromFile(
	const WCHAR* pszFileName,
	LPCSTR pszShaderName,
	LPCSTR pszShaderProfile,
	ID3DBlob** ppd3dShaderBlob)
{
	UINT nCompileFlags = 0;

#if defined(_DEBUG)
	nCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	::D3DCompileFromFile(
		pszFileName,
		NULL,
		NULL,
		pszShaderName,
		pszShaderProfile,
		nCompileFlags,
		0,
		ppd3dShaderBlob,
		NULL
	);

	D3D12_SHADER_BYTECODE d3dShaderByteCode;
	d3dShaderByteCode.BytecodeLength = (*ppd3dShaderBlob)->GetBufferSize();
	d3dShaderByteCode.pShaderBytecode = (*ppd3dShaderBlob)->GetBufferPointer();

	return(d3dShaderByteCode);
}

