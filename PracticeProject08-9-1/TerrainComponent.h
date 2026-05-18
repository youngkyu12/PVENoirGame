#pragma once

#include "Mesh.h"
#include "Component.h"

class CModelComponent;

class TerrainComponent final : public CComponentT<TerrainComponent>
{
public:
	TerrainComponent(
		CGameObject* owner,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		LPCTSTR pFileName,
		int nWidth,
		int nLength,
		int nBlockWidth,
		int nBlockLength,
		XMFLOAT3 xmf3Scale,
		XMFLOAT4 xmf4Color);

	void OnCreate(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) override;

public:
	float GetHeight(float x, float z) const;
	XMFLOAT3 GetNormal(float x, float z) const;

	int GetHeightMapWidth() const { return m_nWidth; }
	int GetHeightMapLength() const { return m_nLength; }
	XMFLOAT3 GetScale() const { return m_xmf3Scale; }

	float GetWidth() const { return m_nWidth * m_xmf3Scale.x; }
	float GetLength() const { return m_nLength * m_xmf3Scale.z; }

	float SampleHeightMap(int x, int z) const;
	XMFLOAT3 SampleHeightMapNormal(int x, int z) const;

private:
	bool LoadHeightMapRaw(LPCTSTR pFileName, int nWidth, int nLength);
	float SampleHeightMapRaw01(int x, int z) const;
	int ClampX(int x) const;
	int ClampZ(int z) const;

private:
	CModelComponent* mModel = nullptr;
	ID3D12RootSignature* m_pd3dGraphicsRootSignature = nullptr;

	std::vector<BYTE> m_heightMapPixels;
	std::wstring m_heightMapPath;

	int m_nWidth = 0;
	int m_nLength = 0;
	int m_nBlockWidth = 0;
	int m_nBlockLength = 0;

	XMFLOAT3 m_xmf3Scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
	XMFLOAT4 m_xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
};