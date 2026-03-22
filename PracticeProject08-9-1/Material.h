#pragma once

#include "stdafx.h"
#include "Texture.h"
#include "Shader.h"

class CShader;
class CMaterial
{
public:
	CMaterial();
	virtual ~CMaterial();

public:

	XMFLOAT4						m_xmf4Albedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	UINT							m_nReflection = 0;
	shared_ptr<CTexture> m_pTexture;
	shared_ptr<CTexture> m_pNormalTexture;
	shared_ptr<CShader> m_pShader;

	void SetAlbedo(XMFLOAT4 xmf4Albedo) { m_xmf4Albedo = xmf4Albedo; }
	void SetReflection(UINT nReflection) { m_nReflection = nReflection; }
	void SetTexture(shared_ptr<CTexture> pTexture);
	void SetNormalTexture(shared_ptr<CTexture> pTexture);
	void SetShader(shared_ptr<CShader> pShader);
	void SetEmissiveTexture(shared_ptr<CTexture> pTexture);
	void SetSpecularTexture(shared_ptr<CTexture> pTexture);

	void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);
	void ReleaseShaderVariables();

	void ReleaseUploadBuffers();

	UINT m_nDiffuseSrvIndex = UINT_MAX;
	void SetDiffuseSrvIndex(UINT idx) { m_nDiffuseSrvIndex = idx; }
	UINT GetDiffuseSrvIndex() const { return m_nDiffuseSrvIndex; }
	bool NeedsLegacyBinding() const;

	UINT GetMaterialID() const { return m_nMaterialID; }
	void SetMaterialID(UINT id) { m_nMaterialID = id; }

private:
	UINT m_nMaterialID = UINT_MAX;

	std::string m_diffuseTextureName;
	std::string m_normalTextureName;
	std::string m_emissiveTextureName;
	std::string m_specularTextureName;

	UINT m_nNormalSrvIndex = UINT_MAX;
	UINT m_nEmissiveSrvIndex = UINT_MAX;
	UINT m_nSpecularSrvIndex = UINT_MAX;

	shared_ptr<CTexture> m_pEmissiveTexture;
	shared_ptr<CTexture> m_pSpecularTexture;

public:
	void SetDiffuseTextureName(const std::string& s) { m_diffuseTextureName = s; }
	void SetNormalTextureName(const std::string& s) { m_normalTextureName = s; }
	void SetEmissiveTextureName(const std::string& s) { m_emissiveTextureName = s; }
	void SetSpecularTextureName(const std::string& s) { m_specularTextureName = s; }

	const std::string& GetDiffuseTextureName() const { return m_diffuseTextureName; }
	const std::string& GetNormalTextureName() const { return m_normalTextureName; }
	const std::string& GetEmissiveTextureName() const { return m_emissiveTextureName; }
	const std::string& GetSpecularTextureName() const { return m_specularTextureName; }

	void SetNormalSrvIndex(UINT idx) { m_nNormalSrvIndex = idx; }
	UINT GetNormalSrvIndex() const { return m_nNormalSrvIndex; }

	void SetEmissiveSrvIndex(UINT idx) { m_nEmissiveSrvIndex = idx; }
	UINT GetEmissiveSrvIndex() const { return m_nEmissiveSrvIndex; }

	void SetSpecularSrvIndex(UINT idx) { m_nSpecularSrvIndex = idx; }
	UINT GetSpecularSrvIndex() const { return m_nSpecularSrvIndex; }
};
