#pragma once

class HeightMapImage;

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

	float GetHeight(float localX, float loaclZ) const;
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
	HeightMapImage* GetHeightMapImage() const;

private:
	HeightMapImage* m_heightMapImage = nullptr;

	int m_nWidth = 0;
	int m_nLength = 0;

	int m_nBlockWidth = 0;
	int m_nBlockLength = 0;

	XMFLOAT3 m_xmf3Scale;
};