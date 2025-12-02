#pragma once
#include "Object.h"

class CShader {
public:
	CShader() {}
	virtual ~CShader() {}

	virtual void ReleaseObjects() {}
	virtual void ReleaseUploadBuffers() {}

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

	void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) {}

// Render
public:
	virtual void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);

	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList) {}
	virtual void UpdateShaderVariable(ID3D12GraphicsCommandList* pd3dCommandList, XMFLOAT4X4* pxmf4x4World) {}

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

// Build
public:
	virtual void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
	virtual void CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature);

// Render
public:
	virtual void AnimateObjects(float fTimeElapsed);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);

protected:
	vector<shared_ptr<CGameObject>> m_ppObjects;
	int m_nObjects = 0;
};

class CPlayerShader : public CShader {
public:
	CPlayerShader() {}
	virtual ~CPlayerShader() {}

	virtual void ReleaseObjects();

	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
	virtual void CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature);
};
