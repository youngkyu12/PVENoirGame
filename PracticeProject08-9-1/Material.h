#pragma once

#include "Texture.h"
#include "Shader.h"

class CMaterial
{
public:
	CMaterial();
	virtual ~CMaterial();

public:

	XMFLOAT4						m_xmf4Albedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	UINT							m_nReflection = 0;
	shared_ptr<CTexture> m_pTexture;
	shared_ptr<CShader> m_pShader;

	void SetAlbedo(XMFLOAT4 xmf4Albedo) { m_xmf4Albedo = xmf4Albedo; }
	void SetReflection(UINT nReflection) { m_nReflection = nReflection; }
	void SetTexture(shared_ptr<CTexture> pTexture);
	void SetShader(shared_ptr<CShader> pShader);

	void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);
	void ReleaseShaderVariables();

	void ReleaseUploadBuffers();
};
