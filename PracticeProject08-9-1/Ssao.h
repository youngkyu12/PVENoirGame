#pragma once

class CTexture;
class CShader;

struct SsaoCB
{
	DirectX::XMFLOAT4X4 Proj;
	DirectX::XMFLOAT4X4 InvProj;
	DirectX::XMFLOAT4X4 ProjTex;
	DirectX::XMFLOAT4   OffsetVectors[14];

	// For SsaoBlur.hlsl
	DirectX::XMFLOAT4 BlurWeights[3];

	DirectX::XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };

	// Coordinates given in view space.
	float OcclusionRadius  = 0.5f;
	float OcclusionFadeStart = 0.2f;
	float OcclusionFadeEnd = 2.0f;
	float SurfaceEpsilon = 0.05f;

	UINT NormalMapIndex = UINT_MAX;
	UINT DepthMapIndex = UINT_MAX;
	UINT RandomVecMapIndex = UINT_MAX;
	UINT InputMapIndex = UINT_MAX;
	UINT HorizontalBlur = 0;
	DirectX::XMUINT3 Padding = {};
};

class Ssao
{
public:
	Ssao(UINT width, UINT height);
	Ssao(const Ssao& rhs) = delete;
	Ssao& operator=(const Ssao& rhs) = delete;
	~Ssao() = default; 

	static const DXGI_FORMAT AmbientMapFormat = DXGI_FORMAT_R16_UNORM;
	static const DXGI_FORMAT NormalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	static const int MaxBlurRadius = 5;

	UINT SsaoMapWidth()const;
	UINT SsaoMapHeight()const;
	const D3D12_VIEWPORT& Viewport() const;
	const D3D12_RECT& ScissorRect() const;

	void GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);
	std::vector<float> CalcGaussWeights(float sigma);

	void OnResize(UINT newWidth, UINT newHeight);
	
	void SetDepthSrvIndex(UINT depthSrvIndex);
	UINT GetDepthSrvIndex() const { return mDepthMapSrvIndex; }

private:
	void BuildOffsetVectors();

private:
	UINT mRenderTargetWidth = 0;
	UINT mRenderTargetHeight = 0;

	DirectX::XMFLOAT4 mOffsets[14];

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;
	UINT mDepthMapSrvIndex = UINT_MAX;
};

