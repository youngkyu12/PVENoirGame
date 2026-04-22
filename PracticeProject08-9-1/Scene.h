//-----------------------------------------------------------------------------
// File: Scene.h
//-----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <vector>
#include <array>
#include <cstdint>

#include "DescriptorHeap.h"
#include "NetworkQueue.h"

#define ROOT_PARAMETER_DRAW_OPTIONS 5
#define ROOT_PARAMETER_GLOBAL_SRV 6
#define ROOT_PARAMETER_FOG 9
#define ROOT_PARAMETER_SHADOW 10
constexpr UINT LEGACY_SRV_COUNT = 6; // t0(1) + t1~t5(5)

class CGameObject;
class CCamera;
class CAudioManager;

constexpr UINT GLOBAL_SRV_CAPACITY = 8192;

// ============================================================================
// Base Scene
// - 공통 인프라(루트시그니처/디스크립터힙/카메라) + 인터페이스만 제공
// - Game 로직/데이터는 CGameScene로 이동
// ============================================================================
class CScene
{
public:
    CScene();
    virtual ~CScene();

    // Lifecycle / Release
public:
    // Base는 "공통 리소스(카메라/루트시그니처)"만 정리한다.
    // 파생(Game/Menu)에서 자기 리소스를 정리한 뒤 CScene::ReleaseObjects() 호출.
    virtual void ReleaseObjects();

    // 파생에서 필요 시 구현
    virtual void ReleaseShaderVariables() {}
    virtual void ReleaseUploadBuffers() {}

    // Build
public:
    virtual void BuildObjects(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) = 0;
    virtual void BuildObjectsCollider() {}
	virtual void InitShadowMap() {};

protected:
    void CreateGraphicsRootSignature(ID3D12Device* dev);
	std::array<const D3D12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

    // 파생에서 카메라 생성(메뉴/게임 각자 방식)
    virtual void CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* target) = 0;

    // Frame / Render
public:
    virtual bool ProcessInput(UCHAR* /*pKeysBuffer*/) { return false; }
    virtual void AnimateObjects(float /*dt*/) {}
    virtual void CollisionObjects() {}

    // 공통: RS/Heap/SRVTable/Camera 세팅까지만 수행
    virtual void OnPrepareRender(ID3D12GraphicsCommandList* cmd, CCamera* camera);

    // 파생에서 실제 드로우
    virtual void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera = nullptr) = 0;

    // Input (messages)
public:
    virtual bool OnProcessingMouseMessage(HWND /*hWnd*/, UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/) { return false; }
    virtual bool OnProcessingKeyboardMessage(HWND /*hWnd*/, UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/) { return false; }

public:
    enum class ESceneRequest : uint8_t
    {
        None = 0,
        SwitchToGame,
    };

    virtual bool ConsumeSceneRequest(ESceneRequest& outReq)
    {
        outReq = ESceneRequest::None;
        return false;
    }

    // Common getters
public:
    ID3D12RootSignature* GetGraphicsRootSignature() const { return m_pd3dGraphicsRootSignature.Get(); }
    void SetGraphicsRootSignature(ID3D12GraphicsCommandList* cmd) { cmd->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get()); }

    CCamera* GetMainCamera() const { return m_pMainCamera; }
    CGameObject* GetMainCameraObject() const { return m_pMainCameraObject.get(); }

    // Framework 입력 코드 호환(메뉴에서는 nullptr 반환)
    virtual CGameObject* GetPlayer() const { return nullptr; }

    // Framework 숫자키 공격 요청 호환(메뉴에서는 noop)
    virtual void RequestPlayerAttackBySlot(int /*slot*/) {}
	void SetAudioManager(CAudioManager* audioManager) { m_pAudioManager = audioManager; }
	CAudioManager* GetAudioManager() const { return m_pAudioManager; }

public:
    static std::unique_ptr<CDescriptorHeap> m_pDescriptorHeap;

protected:
    ComPtr<ID3D12RootSignature> m_pd3dGraphicsRootSignature;

    std::unique_ptr<CGameObject> m_pMainCameraObject;
    CCamera* m_pMainCamera = nullptr;
	CAudioManager* m_pAudioManager = nullptr;

protected:
	NetworkMessage m_pendingNetworkMessage;
public:
    virtual void DequeueNetworkMessage(const NetworkMessageType& type);
    virtual void SetNetworkMessageType(NetworkMessageType type);
};