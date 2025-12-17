#pragma once
#include "Shader.h"

struct LIGHT
{
	XMFLOAT4	m_xmf4Ambient;
	XMFLOAT4	m_xmf4Diffuse;
	XMFLOAT4	m_xmf4Specular;
	XMFLOAT3	m_xmf3Position;
	float		m_fFalloff;
	XMFLOAT3	m_xmf3Direction;
	float		m_fTheta; //cos(m_fTheta)
	XMFLOAT3	m_xmf3Attenuation;
	float		m_fPhi; //cos(m_fPhi)
	bool		m_bEnable;
	int			m_nType;
	float		m_fRange;
	float		padding;
};

struct LIGHTS
{
	LIGHT  m_pLights[MAX_LIGHTS];
	XMFLOAT4  m_xmf4GlobalAmbient;
};

struct MATERIALS
{
	MATERIAL m_pReflections[MAX_MATERIALS];
};

class CScene {
public:
	CScene() {}
	virtual ~CScene() {}

	void ReleaseObjects();
	void ReleaseUploadBuffers();
	virtual void ReleaseShaderVariables();

//Build
public:
	void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

	void CreateGraphicsRootSignature(ID3D12Device* pd3dDevice);
	ComPtr<ID3D12RootSignature> GetGraphicsRootSignature();

	//씬의 모든 조명과 재질을 생성
	void BuildLightsAndMaterials();

	//씬의 모든 조명과 재질을 위한 리소스를 생성하고 갱신
	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

// Render
public:
	bool ProcessInput(UCHAR* pKeysBuffer);
	void AnimateObjects(float fTimeElapsed);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);

	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);

// Input
public:
	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);

public:
	shared_ptr<CPlayer> m_pPlayer;

// protected member variables
protected:
	unique_ptr<LIGHTS> m_pLights = nullptr;

	ComPtr<ID3D12Resource> m_pd3dcbLights;
	LIGHTS* m_pcbMappedLights = nullptr;

	unique_ptr<MATERIALS> m_pMaterials = nullptr;

	ComPtr<ID3D12Resource> m_pd3dcbMaterials = nullptr;
	MATERIAL* m_pcbMappedMaterials = nullptr;

	vector<shared_ptr<CObjectsShader>> m_ppShaders;
	int m_nShaders = 0;

	ComPtr<ID3D12RootSignature> m_pd3dGraphicsRootSignature;
};

