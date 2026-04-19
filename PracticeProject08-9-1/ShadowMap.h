#pragma once

#include "Shader.h"
#include "MathHelper.h"
#include "Timer.h"

class CLightComponent;
class CGameObject;

struct CB_SHADOW_PASS
{
	DirectX::XMFLOAT4X4 ViewProj = Matrix4x4::Identity();
	DirectX::XMFLOAT4X4 ShadowTransform = Matrix4x4::Identity();
	DirectX::XMFLOAT3 LightDirectionW = { 0.0f, -1.0f, 0.0f };
	float PadShadow0 = 0.0f;
	DirectX::XMUINT4 ShadowMeta = DirectX::XMUINT4(UINT_MAX, 0, 0, 0);
};

class ShadowMap
{
public:
	ShadowMap(ID3D12Device* device, CShader* shader,
		UINT width, UINT height);

	ShadowMap(const ShadowMap& rhs) = delete;
	ShadowMap& operator=(const ShadowMap& rhs) = delete;
	~ShadowMap();

	UINT Width()const;
	UINT Height()const;
	ID3D12Resource* Resource();
	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv()const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE Dsv()const;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> Heap() const;

	D3D12_VIEWPORT Viewport()const;
	D3D12_RECT ScissorRect()const;

	CB_SHADOW_PASS GetConstants();

	void SetLightComponent(CLightComponent* light) { m_pLight = light; }
	void SetTargetObject(CGameObject* target) { m_pTarget = target; }

	void OnCreate(D3D12_CPU_DESCRIPTOR_HANDLE dsvStart);
	void OnUpdate();
	void Render(const CGameTimer& gt, ID3D12GraphicsCommandList* commandList, std::vector<CGameObject*> components);

	void CreateShadowPassCB();
	void UpdateShadowPassCB();
	void BindShadowPassCB(ID3D12GraphicsCommandList* commandList) const;

	void BuildDescriptors(
		D3D12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		D3D12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		D3D12_CPU_DESCRIPTOR_HANDLE hCpuDsv,
		UINT shadowSrvIndex);

	void OnResize(UINT newWidth, UINT newHeight);
	UINT GetSrvIndex() const { return m_nShadowSrvIndex; }

private:
	void BuildDescriptors();
	void BuildResource();

private:

	ComPtr<ID3D12Device> mDevice;
	CShader* mShader;
	CLightComponent* m_pLight = nullptr;
	CGameObject* m_pTarget = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mFormat = DXGI_FORMAT_R24G8_TYPELESS;

	D3D12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
	D3D12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
	D3D12_CPU_DESCRIPTOR_HANDLE mhCpuDsv;

	ComPtr<ID3D12Resource> mShadowMap = nullptr;

	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	std::unique_ptr<CB_SHADOW_PASS> m_pShadow;
	ComPtr<ID3D12Resource> m_pd3dcbShadowPass;
	CB_SHADOW_PASS* m_pcbMappedShadowPass = nullptr;

	DirectX::XMFLOAT3 mLightPosW = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 mLightDirW = { 0.0f, -1.0f, 0.0f };
	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	DirectX::XMFLOAT4X4 mLightView = Matrix4x4::Identity();
	DirectX::XMFLOAT4X4 mLightProj = Matrix4x4::Identity();
	DirectX::XMFLOAT4X4 mShadowTransform = Matrix4x4::Identity();
	
	UINT m_nShadowSrvIndex = UINT_MAX;
};