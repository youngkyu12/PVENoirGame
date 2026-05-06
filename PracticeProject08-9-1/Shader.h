//-----------------------------------------------------------------------------
// File: Shader.h
//-----------------------------------------------------------------------------

#pragma once

#include "Object.h"
#include "Camera.h"
#include "Texture.h"
#include "AnimatorData.h"
#include "SceneRenderTypes.h"

#define _WITH_SCENE_ROOT_SIGNATURE

class CMaterial;
class CGameObject;
struct CB_GAMEOBJECT_INFO;

class CShader
{
public:
	CShader();
	virtual ~CShader();

	// Release
public:
	virtual void ReleaseShaderVariables();
	virtual void ReleaseObjects() {}
	virtual void ReleaseUploadBuffers();

	// Util
public:
	D3D12_SHADER_BYTECODE CompileShaderFromFile(
		const WCHAR* pszFileName,
		LPCSTR pszShaderName,
		LPCSTR pszShaderProfile,
		ID3DBlob** ppd3dShaderBlob
	);

	// Build
public:
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_RASTERIZER_DESC CreateRasterizerState();
	virtual D3D12_BLEND_DESC CreateBlendState();
	virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState();

	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);

	virtual void CreateShader(
		ID3D12Device* pd3dDevice,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		UINT nRenderTargets,
		DXGI_FORMAT* pdxgiRtvFormats,
		DXGI_FORMAT dxgiDsvFormat
	);

	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

	virtual void BuildObjects(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		void* pContext = nullptr) 
	{
	}

	virtual void CreateGraphicsRootSignature(ID3D12Device* pd3dDevice) {}
	ID3D12RootSignature* GetGraphicsRootSignature() { return(m_pd3dGraphicsRootSignature.Get()); }

	// Render
public:
	virtual void AnimateObjects(float fTimeElapsed) {}
	virtual void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext = nullptr);

	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext);

protected:
	ComPtr<ID3D12PipelineState>				m_pd3dPipelineState;
	ComPtr<ID3D12RootSignature>				m_pd3dGraphicsRootSignature;
};

class CDiffusedShader : public CShader
{
public:
	CDiffusedShader() {};
	virtual ~CDiffusedShader() {};

public:
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();

	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);

	virtual D3D12_RASTERIZER_DESC CreateRasterizerState();
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CTexturedShader : public CShader
{
public:
	CTexturedShader();
	virtual ~CTexturedShader();

public:
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();

	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CIlluminatedTexturedShader : public CTexturedShader
{
public:
	CIlluminatedTexturedShader();
	virtual ~CIlluminatedTexturedShader();

public:
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();

	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
#define _WITH_BATCH_MATERIAL

class CStaticObjectsShader : public CIlluminatedTexturedShader
{
public:
	CStaticObjectsShader();
	virtual ~CStaticObjectsShader();

public:
	virtual void CreateShader(
		ID3D12Device* pd3dDevice,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		UINT nRenderTargets,
		DXGI_FORMAT* pdxgiRtvFormats,
		DXGI_FORMAT dxgiDsvFormat
	);

	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout() override;
	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob) override;
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext = nullptr);
}; 

class CTreeStaticObjectsShader final : public CStaticObjectsShader
{
public:
	CTreeStaticObjectsShader() = default;
	~CTreeStaticObjectsShader() override = default;

public:
	D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_RASTERIZER_DESC CreateRasterizerState() override;
};

class CSkinnedObjectsShader : public CIlluminatedTexturedShader
{
public:
	CSkinnedObjectsShader();
	virtual ~CSkinnedObjectsShader();

public:
	virtual void CreateShader(
		ID3D12Device* pd3dDevice,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		UINT nRenderTargets,
		DXGI_FORMAT* pdxgiRtvFormats,
		DXGI_FORMAT dxgiDsvFormat
	);

	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);

	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext = nullptr);
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

class CItemBillboardShader final : public CStaticObjectsShader
{
public:
	CItemBillboardShader() = default;
	~CItemBillboardShader() override = default;

public:
	D3D12_INPUT_LAYOUT_DESC CreateInputLayout() override;
	D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_RASTERIZER_DESC CreateRasterizerState() override;
};

class CTransparentItemBillboardShader final : public CStaticObjectsShader
{
public:
	CTransparentItemBillboardShader() = default;
	~CTransparentItemBillboardShader() override = default;

public:
	D3D12_INPUT_LAYOUT_DESC CreateInputLayout() override;
	D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob) override;

	D3D12_RASTERIZER_DESC CreateRasterizerState() override;
	D3D12_BLEND_DESC CreateBlendState() override;
	D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState() override;
};

class CMuzzleFlashBillboardShader final : public CStaticObjectsShader
{
public:
	CMuzzleFlashBillboardShader() = default;
	~CMuzzleFlashBillboardShader() override = default;

public:
	D3D12_INPUT_LAYOUT_DESC CreateInputLayout() override;
	D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob) override;

	D3D12_RASTERIZER_DESC CreateRasterizerState() override;
	D3D12_BLEND_DESC CreateBlendState() override;
	D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState() override;
};

class CSwordTrailShader final : public CStaticObjectsShader
{
public:
	CSwordTrailShader() = default;
	~CSwordTrailShader() override = default;

public:
	D3D12_INPUT_LAYOUT_DESC CreateInputLayout() override;
	D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob) override;

	D3D12_RASTERIZER_DESC CreateRasterizerState() override;
	D3D12_BLEND_DESC CreateBlendState() override;
	D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState() override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

class CShadowMapStaticShader : public CStaticObjectsShader
{
public:
	CShadowMapStaticShader() = default;
	~CShadowMapStaticShader() override = default;

public:
	D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_RASTERIZER_DESC CreateRasterizerState() override;
};

class CShadowMapAlphaClipStaticShader final : public CShadowMapStaticShader
{
public:
	CShadowMapAlphaClipStaticShader() = default;
	~CShadowMapAlphaClipStaticShader() override = default;

public:
	D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_RASTERIZER_DESC CreateRasterizerState() override;
};

class CShadowMapSkinnedShader : public CSkinnedObjectsShader
{
public:
	CShadowMapSkinnedShader() = default;
	~CShadowMapSkinnedShader() override = default;

public:
	D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_RASTERIZER_DESC CreateRasterizerState() override;
};

class CShadowMapAlphaClipSkinnedShader final : public CShadowMapSkinnedShader
{
public:
	CShadowMapAlphaClipSkinnedShader() = default;
	~CShadowMapAlphaClipSkinnedShader() override = default;

public:
	D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_RASTERIZER_DESC CreateRasterizerState() override;
};

class COcclusionStaticShader final : public CStaticObjectsShader
{
public:
	COcclusionStaticShader() = default;
	~COcclusionStaticShader() override = default;

public:
	D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob) override;
	D3D12_RASTERIZER_DESC CreateRasterizerState() override;
	D3D12_BLEND_DESC CreateBlendState() override;
	D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState() override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CUIShader : public CTexturedShader
{
public:
	CUIShader();
	virtual ~CUIShader();

public:
	virtual D3D12_RASTERIZER_DESC CreateRasterizerState() override;
	virtual D3D12_BLEND_DESC CreateBlendState() override;
	virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState() override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CPostProcessingShader : public CShader
{
public:
	CPostProcessingShader();
	virtual ~CPostProcessingShader();

	// Build
public:
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState();

	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);

	virtual void CreateGraphicsRootSignature(ID3D12Device* pd3dDevice);

	virtual void CreateShader(
		ID3D12Device* pd3dDevice,
		ID3D12RootSignature* pd3dGraphicsRootSignature,
		UINT nRenderTargets,
		DXGI_FORMAT* pdxgiRtvFormats,
		DXGI_FORMAT dxgiDsvFormat
	);

	virtual void CreateResourcesAndRtvsSrvs(
		ID3D12Device* pd3dDevice,
		ID3D12GraphicsCommandList* pd3dCommandList,
		UINT nRenderTargets,
		DXGI_FORMAT* pdxgiFormats,
		D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle
	);

	// RenderTarget
public:
	virtual void OnPrepareRenderTarget(
		ID3D12GraphicsCommandList* pd3dCommandList,
		int nRenderTargets,
		D3D12_CPU_DESCRIPTOR_HANDLE* pd3dRtvCPUHandles,
		D3D12_CPU_DESCRIPTOR_HANDLE* pd3dDsvCPUHandle
	);

	virtual void OnPrepareSceneRenderTargets(
		ID3D12GraphicsCommandList* pd3dCommandList,
		D3D12_CPU_DESCRIPTOR_HANDLE* pd3dDsvCPUHandle
	);

	virtual void OnPostRenderTarget(ID3D12GraphicsCommandList* pd3dCommandList);

	// Render
public:
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext);

	// Resource Getters
public:
	CTexture* GetTexture() { return(m_pTexture.get()); }
	ID3D12Resource* GetTextureResource(UINT nIndex) { return(m_pTexture->GetResource(nIndex)); }

	D3D12_CPU_DESCRIPTOR_HANDLE GetRtvCPUDescriptorHandle(UINT nIndex) { return(m_pd3dRtvCPUDescriptorHandles[nIndex]); }

protected:
	shared_ptr<CTexture>						m_pTexture;
	unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]>	m_pd3dRtvCPUDescriptorHandles;
};

struct PS_CB_DRAW_OPTIONS
{
	XMINT4  m_xmn4DrawOptions;     // x='T','L','N','D','Z'
	XMUINT4 m_xmu4PostSrvIdx0;     // x=T, y=L, z=N, w=D
	XMUINT4 m_xmu4PostSrvIdx1;     // x=Z

	// UI rect in pixels
	// x=centerX, y=centerY, z=width, w=height
	XMFLOAT4 m_xmf4UiRect;

	// viewport
	// x=width, y=height, z=1/width, w=1/height
	XMFLOAT4 m_xmf4Viewport;
};

class CTextureToFullScreenShader : public CPostProcessingShader
{
public:
	CTextureToFullScreenShader();
	virtual ~CTextureToFullScreenShader();

public:
	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);

	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext);
	virtual void ReleaseShaderVariables();

	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext = nullptr);

protected:
	static constexpr UINT			m_nMaxDrawOptionEntries = 64;

	ComPtr<ID3D12Resource>			m_pd3dcbDrawOptions;
	UINT8* m_pcbMappedDrawOptions = nullptr;

	UINT							m_nDrawOptionsStride = 0;
	UINT							m_nDrawOptionWriteIndex = 0;

public:
	void ResetDrawOptionWriteIndex() { m_nDrawOptionWriteIndex = 0; }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRectUIShader final : public CTextureToFullScreenShader
{
public:
	CRectUIShader() = default;
	~CRectUIShader() override = default;

public:
	// 중요: Scene이 이미 RootSig/DescriptorTable을 세팅하므로
	// Shader가 RootSig를 다시 Set하지 않게(= 파라미터 무효화 방지) CreateShader를 재정의한다.
	void CreateShader(
		ID3D12Device* dev,
		ID3D12RootSignature* sceneRootSig,
		UINT nRenderTargets,
		DXGI_FORMAT* rtvFormats,
		DXGI_FORMAT dsvFormat
	) override;

	D3D12_RASTERIZER_DESC CreateRasterizerState() override;
	D3D12_BLEND_DESC CreateBlendState() override;
	void UpdateShaderVariables(ID3D12GraphicsCommandList* cmd, void* pContext) override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class CShadowShader : public CShader
{
public:
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_RASTERIZER_DESC CreateRasterizerState();
	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CDepthFogShader final : public CTextureToFullScreenShader
{
public:
	CDepthFogShader() = default;
	~CDepthFogShader() override = default;

public:
	// 중요: Scene이 이미 RootSig/DescriptorTable을 세팅하므로
	// Shader가 RootSig를 다시 Set하지 않게 CreateShader를 재정의한다.
	void CreateShader(
		ID3D12Device* dev,
		ID3D12RootSignature* sceneRootSig,
		UINT nRenderTargets,
		DXGI_FORMAT* rtvFormats,
		DXGI_FORMAT dsvFormat
	) override;

	D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob) override;

	D3D12_RASTERIZER_DESC CreateRasterizerState() override;
	D3D12_BLEND_DESC CreateBlendState() override;
	D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState() override;

	void UpdateShaderVariables(ID3D12GraphicsCommandList* cmd, void* pContext) override;
};