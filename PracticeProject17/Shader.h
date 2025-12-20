#pragma once
#include "Object.h"

//객체를 렌더링할 때 적용하는 상수 버퍼 데이터
struct CB_GAMEOBJECT_INFO
{
	XMFLOAT4X4 m_xmf4x4World;
	UINT m_nMaterial;
};

//플레이어 객체를 렌더링할 때 적용하는 상수 버퍼 데이터
struct CB_PLAYER_INFO
{
	XMFLOAT4X4  m_xmf4x4World;
};

class CShader {
public:
	CShader() {}
	virtual ~CShader() {}

	virtual void ReleaseObjects() {}
	virtual void ReleaseUploadBuffers() {}
	virtual void ReleaseShaderVariables() {}

	virtual D3D12_SHADER_BYTECODE CompileShaderFromFile(
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
	
	virtual void CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dRootSignature);

	virtual void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) {}

	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) {}

// Render
public:
	virtual void AnimateObjects(float fTimeElapsed) {}
	virtual void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);

	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList) {}
	virtual void UpdateShaderVariable(ID3D12GraphicsCommandList* pd3dCommandList, XMFLOAT4X4* pxmf4x4World) {}
	virtual void UpdateShaderVariable(ID3D12GraphicsCommandList* pd3dCommandList, MATERIAL* pMaterial) {}

// protected member variables
protected:
	vector<ComPtr<ID3D12PipelineState>> m_ppd3dPipelineStates;
	int m_nd3dPipelineState = 0;
};

class CObjectsShader : public CShader {
public:
	CObjectsShader() {}
	virtual ~CObjectsShader() {}

	virtual void ReleaseObjects();
	virtual void ReleaseUploadBuffers();
	virtual void ReleaseShaderVariables();

// Build
public:
	virtual void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
	virtual void CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature);

	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

// Render
public:
	virtual void AnimateObjects(float fTimeElapsed);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);

protected:
	vector<shared_ptr<CGameObject>> m_ppObjects;
	int m_nObjects = 0;

	ComPtr<ID3D12Resource> m_pd3dcbGameObjects;
	UINT8* m_pcbMappedGameObjects = nullptr;
		
	
};

class CPlayerShader : public CShader {
public:
	CPlayerShader() {}
	virtual ~CPlayerShader() {}

	virtual void ReleaseObjects();
	virtual void ReleaseShaderVariables();

public:
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
	virtual void CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature);

	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

public:
	virtual void UpdateShaderVariable(ID3D12GraphicsCommandList* pd3dCommandList, XMFLOAT4X4* pxmf4x4World);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);

protected:
	//플레이어 객체에 대한 리소스와 리소스 포인터
	ComPtr<ID3D12Resource> m_pd3dcbPlayer;
	CB_PLAYER_INFO* m_pcbMappedPlayer = nullptr;
};
