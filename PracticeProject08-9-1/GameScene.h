//-----------------------------------------------------------------------------
// File: GameScene.h
//-----------------------------------------------------------------------------

#pragma once

#include "Scene.h"
#include "Shader.h"
#include "LightTypes.h"
#include "SceneRenderTypes.h"

class CMaterial;
class CFollowTransformComponent;
class CArrowComponent;

struct CB_GAMEOBJECT_INFO;

// ============================================================================
// GameScene
// ============================================================================
class CGameScene final : public CScene
{
public:
    CGameScene();
    ~CGameScene() override;

    // Lifecycle / Release
public:
    void ReleaseObjects() override;
    void ReleaseShaderVariables() override;
    void ReleaseUploadBuffers() override;

    // Build
public:
    void BuildObjects(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;

protected:
    void CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* target) override;

private:
    void BuildLightsAndMaterials();
    void CreateShaderVariables(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd);

    void BuildStaticBatch(
        ID3D12Device* dev,
        ID3D12GraphicsCommandList* cmd,
        const std::shared_ptr<CStaticObjectsShader>& shader,
        UINT rtCount,
        DXGI_FORMAT* rtvFormats,
        DXGI_FORMAT dsvFormat
    );

    void BuildSkinnedBatch(
        ID3D12Device* dev,
        ID3D12GraphicsCommandList* cmd,
        const std::shared_ptr<CSkinnedObjectsShader>& shader,
        UINT rtCount,
        DXGI_FORMAT* rtvFormats,
        DXGI_FORMAT dsvFormat
    );

    void UpdateShaderVariables(ID3D12GraphicsCommandList* cmd);

    // Frame / Render
public:
    bool ProcessInput(UCHAR* pKeysBuffer) override;
    void AnimateObjects(float dt) override;

    void OnPrepareRender(ID3D12GraphicsCommandList* cmd, CCamera* camera) override;
    void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera = nullptr) override;

    // Input (messages) : 게임에서는 좌클릭 공격
public:
    bool OnProcessingMouseMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
    bool OnProcessingKeyboardMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) override;

    // Framework 호환: 플레이어 제공
public:
    CGameObject* GetPlayer() const override { return GetPlayerBySlot(m_localPlayerSlot); }
    int GetLocalPlayerSlot() const { return m_localPlayerSlot; }

    // Framework 숫자키 공격(슬롯 0..3)
public:
    void RequestPlayerAttackBySlot(int slot) override;

    // Game-only API
public:
    void SetMaterialDiffuseSrvIndex(int materialId, UINT srvIndex);

    CGameObject* GetDemoFighter(int index) const;
    void RequestDemoFighterAttack(int index);

    CGameObject* GetPlayerBySlot(int slot) const; // slot: 0..3
    bool IsLocalPlayer(const CGameObject* obj) const;

    void RequestFireArrow(CGameObject* shooter, float speed, float lifeSec = 3.0f, float yOffset = 0.0f);

private:
    // slot 0..3 플레이어 포인터(소유는 m_skinnedObjects가 함)
    std::array<CGameObject*, 4> m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };

    int m_localPlayerSlot;

    SCENE_STATIC_BATCH  m_staticBatch;
    SCENE_SKINNED_BATCH m_skinnedBatch;

    std::vector<std::unique_ptr<CGameObject>> m_staticObjects;
    std::vector<std::unique_ptr<CGameObject>> m_skinnedObjects;

    static constexpr UINT kArrowPoolSize = 32;
    std::vector<CGameObject*> m_arrowRefs; // raw pointers owned by m_staticObjects

    std::array<CGameObject*, 3> m_demoFighters = { nullptr, nullptr, nullptr };

    std::vector<std::unique_ptr<CGameObject>> m_lightObjects;
    CFollowTransformComponent* m_pPlayerSpotFollower = nullptr;

    // GPU / Shader Variables (Game 전용)
    ComPtr<ID3D12Resource> m_pd3dcbLights;
    LIGHTS* m_pcbMappedLights = nullptr;

    std::unique_ptr<MATERIALS> m_pMaterials;

    ComPtr<ID3D12Resource> m_pd3dcbMaterials;
    MATERIAL* m_pcbMappedMaterials = nullptr;
};