//-----------------------------------------------------------------------------
// File: Scene.h
//-----------------------------------------------------------------------------

#pragma once

#include "Shader.h"
#include "Player.h"
#include "DescriptorHeap.h"

struct LIGHT
{
	XMFLOAT4				m_xmf4Ambient;
	XMFLOAT4				m_xmf4Diffuse;
	XMFLOAT4				m_xmf4Specular;
	XMFLOAT3				m_xmf3Position;
	float 					m_fFalloff;
	XMFLOAT3				m_xmf3Direction;
	float 					m_fTheta; //cos(m_fTheta)
	XMFLOAT3				m_xmf3Attenuation;
	float					m_fPhi; //cos(m_fPhi)
	bool					m_bEnable;
	int						m_nType;
	float					m_fRange;
	float					padding;
};

struct LIGHTS
{
	LIGHT					m_pLights[MAX_LIGHTS];
	XMFLOAT4				m_xmf4GlobalAmbient;
};

struct MATERIAL
{
	XMFLOAT4				m_xmf4Ambient;
	XMFLOAT4				m_xmf4Diffuse;
	XMFLOAT4				m_xmf4Specular; //(r,g,b,a=power)
	XMFLOAT4				m_xmf4Emissive;
};

struct MATERIALS
{
	MATERIAL				m_pReflections[MAX_MATERIALS];
};

class CScene
{
public:
    CScene();
    ~CScene();

	void ReleaseObjects();
	virtual void ReleaseShaderVariables();
	void ReleaseUploadBuffers();

// Build
public:
	void BuildObjects(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList);
	void BuildLightsAndMaterials();

	void CreateGraphicsRootSignature(ID3D12Device *pd3dDevice);
	virtual void CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList);

// Render
public:
	bool ProcessInput(UCHAR *pKeysBuffer);
    void AnimateObjects(float fTimeElapsed);

	void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);
    void Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera=NULL);

	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);

// Input
public:
	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

// Get & Set Method
public:
	ID3D12RootSignature* GetGraphicsRootSignature() { return(m_pd3dGraphicsRootSignature.Get()); }
	void SetGraphicsRootSignature(ID3D12GraphicsCommandList* pd3dCommandList) { pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get()); }

public:
	shared_ptr<CPlayer>		m_pPlayer;
	static unique_ptr<CDescriptorHeap> m_pDescriptorHeap;

protected:
	ComPtr<ID3D12RootSignature>	m_pd3dGraphicsRootSignature;

	vector<shared_ptr<CShader>>	m_ppShaders;
	int							m_nShaders = 0;

	unique_ptr<LIGHTS>			m_pLights;

	ComPtr<ID3D12Resource>		m_pd3dcbLights;
	LIGHTS						*m_pcbMappedLights = nullptr;

	unique_ptr<MATERIALS>		m_pMaterials;

	ComPtr<ID3D12Resource>		m_pd3dcbMaterials;
	MATERIAL					*m_pcbMappedMaterials = nullptr;
};
