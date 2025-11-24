#pragma once
#include "Object.h"
#include "Camera.h"

class CShader {
public:
	CShader();
	virtual ~CShader();

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
	void AnimateObjects(float fTimeElapsed);
	void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);

	void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList) {}
	virtual void UpdateShaderVariable(ID3D12GraphicsCommandList* pd3dCommandList, XMFLOAT4X4* pxmf4x4World);

// protected member variables
protected:
	vector<ComPtr<ID3D12PipelineState>> m_ppd3dPipelineStates;
	int m_nd3dPipelineState = 0;
};

class CDiffusedShader : public CShader {
public:
	CDiffusedShader();
	virtual ~CDiffusedShader();

// Build
public:
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
	virtual void CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature);
};