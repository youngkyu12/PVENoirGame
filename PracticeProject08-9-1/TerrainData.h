//-----------------------------------------------------------------------------
// File: TerrainData.h
//-----------------------------------------------------------------------------
#pragma once
#include <memory>

class HeightMapImage;
class CTexture;

class TerrainData
{
public:
	TerrainData() = default;

	TerrainData(
		LPCTSTR pFileName, int	nWidth, int nLength, int nBlockWidth, int nBlockLength, 
		XMFLOAT3 xmf3Scale, XMFLOAT4 xmf4Color);

	~TerrainData() = default;

	void SetHeightMapImage(HeightMapImage* heightMapImage);
	void SetSize(int width, int length);
	void SetScale(const XMFLOAT3& scale);
	void SetWorldPosition(const XMFLOAT3& position);
	XMFLOAT3 GetWorldPosition() const;

	float GetHeight(float localX, float loaclZ, bool bReverseQuad = false) const; 
	XMFLOAT3 GetNormal(float localX, float loaclZ) const;
	int GetHeightMapWidth() const;
	int GetHeightMapLength() const;
	XMFLOAT3 GetScale() const;
	int GetWidthCount() const;
	int GetLengthCount() const;
	float GetWorldWidth() const;
	float GetWorldLength() const;
	int GetnBlockWidth() const;
	int GetnBlockLength() const;
	void SetHeightMapTexture(std::shared_ptr<CTexture> texture);
	void SetHeightMapSrvIndex(UINT srvIndex);

	UINT GetsrvIndex() const;
	HeightMapImage* GetHeightMapImage() const;

	void SetGrassDiffuseTexture(std::shared_ptr<CTexture> texture);
	void SetGroundDiffuseTexture(std::shared_ptr<CTexture> texture);
	void SetDirtDiffuseTexture(std::shared_ptr<CTexture> texture);

	void SetGrassNormalTexture(std::shared_ptr<CTexture> texture);
	void SetGroundNormalTexture(std::shared_ptr<CTexture> texture);
	void SetDirtNormalTexture(std::shared_ptr<CTexture> texture);

	UINT GetGrassDiffuseSrvIndex() const;
	UINT GetGroundDiffuseSrvIndex() const;
	UINT GetDirtDiffuseSrvIndex() const;

	UINT GetGrassNormalSrvIndex() const;
	UINT GetGroundNormalSrvIndex() const;
	UINT GetDirtNormalSrvIndex() const;

private:
	HeightMapImage* m_heightMapImage = nullptr;

	int m_nWidth = 0;
	int m_nLength = 0;

	int m_nBlockWidth = 0;
	int m_nBlockLength = 0;

	XMFLOAT3 m_xmf3Scale;
	XMFLOAT3 m_xmf3WorldPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);

	std::shared_ptr<CTexture> m_heightMapTexture;
	UINT srvHeightMapIndex = UINT_MAX;

	std::shared_ptr<CTexture> m_grassDiffuseTexture;
	std::shared_ptr<CTexture> m_groundDiffuseTexture;
	std::shared_ptr<CTexture> m_dirtDiffuseTexture;

	std::shared_ptr<CTexture> m_grassNormalTexture;
	std::shared_ptr<CTexture> m_groundNormalTexture;
	std::shared_ptr<CTexture> m_dirtNormalTexture;

	UINT srvGrassDiffuseIndex = UINT_MAX;
	UINT srvGroundDiffuseIndex = UINT_MAX;
	UINT srvDirtDiffuseIndex = UINT_MAX;

	UINT srvGrassNormalIndex = UINT_MAX;
	UINT srvGroundNormalIndex = UINT_MAX;
	UINT srvDirtNormalIndex = UINT_MAX;
};
