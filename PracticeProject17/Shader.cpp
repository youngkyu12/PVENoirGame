#include "stdafx.h"
#include "Shader.h"

void CObjectsShader::ReleaseObjects()
{
	for (int i = 0; i < m_nd3dPipelineState; ++i)
		if (m_ppd3dPipelineStates[i])
			m_ppd3dPipelineStates[i].Reset();

	for (int i = 0; i < m_nObjects; ++i)
		m_ppObjects[i]->ReleaseObjects();
	ReleaseShaderVariables();
}

void CObjectsShader::ReleaseUploadBuffers()
{
	for (int i = 0; i < m_nObjects; ++i)
		m_ppObjects[i]->ReleaseUploadBuffers();
}

void CObjectsShader::ReleaseShaderVariables()
{
	if (m_pd3dcbGameObjects)
	{
		m_pd3dcbGameObjects->Unmap(0, NULL);
		m_pd3dcbGameObjects.Reset();
	}
}


void CPlayerShader::ReleaseObjects()
{
	for (int i = 0; i < m_nd3dPipelineState; ++i)
		if (m_ppd3dPipelineStates[i])
			m_ppd3dPipelineStates[i].Reset();
	ReleaseShaderVariables();
}

void CPlayerShader::ReleaseShaderVariables()
{
	if (m_pd3dcbPlayer)
	{
		m_pd3dcbPlayer->Unmap(0, NULL);
		m_pd3dcbPlayer.Reset();
	}
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

	ComPtr<ID3DBlob> pd3dErrorBlob;
	HRESULT hr = ::D3DCompileFromFile(
		pszFileName,
		NULL,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		pszShaderName,
		pszShaderProfile,
		nCompileFlags,
		0,
		ppd3dShaderBlob,
		&pd3dErrorBlob
	);

	if (FAILED(hr) || !(*ppd3dShaderBlob)) {
		if (pd3dErrorBlob) OutputDebugStringA((char*)pd3dErrorBlob->GetBufferPointer());
		return { nullptr, 0 };
	}

	D3D12_SHADER_BYTECODE d3dShaderByteCode;
	d3dShaderByteCode.BytecodeLength = (*ppd3dShaderBlob)->GetBufferSize();
	d3dShaderByteCode.pShaderBytecode = (*ppd3dShaderBlob)->GetBufferPointer();

	return(d3dShaderByteCode);
}
