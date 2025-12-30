#include "stdafx.h"
#include "Shader.h"

CShader::CShader()
{
}

CShader::~CShader()
{
}

CDiffusedShader::CDiffusedShader()
{
}

CDiffusedShader::~CDiffusedShader()
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