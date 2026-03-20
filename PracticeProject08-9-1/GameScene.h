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
class CFollowBoneComponent;
class CArrowComponent;
class CGameObject;
class CCollisionSystem;
class CTexture;

struct CB_GAMEOBJECT_INFO;
struct AttachmentBindSpec
{
    CGameObject* follower = nullptr;
    CGameObject* target = nullptr;
    std::string  boneName;
    XMFLOAT4X4   localOffset{};
};
struct StaticPlacementEntry
{
    std::string assetName;
    std::string objectName;

    XMFLOAT3 pos = XMFLOAT3(0.0f, 0.0f, 0.0f);
    XMFLOAT4 rot = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

    float yawDeg = 0.0f;
};

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
    void BuildObjectsCollider() override;

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

    void LinkSceneObjects();
    static XMFLOAT4X4 BuildAttachmentOffsetMatrix(
        const XMFLOAT3& pos,
        const XMFLOAT3& rotDeg,
        const XMFLOAT3& scale = XMFLOAT3(1.0f, 1.0f, 1.0f)
    );

    void UpdateShaderVariables(ID3D12GraphicsCommandList* cmd);

    // Frame / Render
public:
    bool ProcessInput(UCHAR* pKeysBuffer) override;
    void AnimateObjects(float dt) override;
    void CollisionObjects() override;

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
    void SetInactiveOverlayVisible(bool visible) { m_bInactiveOverlayVisible = visible; }

    CGameObject* GetDemoFighter(int index) const;
    void RequestDemoFighterAttack(int index);

    CGameObject* GetPlayerBySlot(int slot) const; // slot: 0..3
    bool IsLocalPlayer(const CGameObject* obj) const;

    void RequestFireArrow(CGameObject* shooter, float speed, float lifeSec = 3.0f, float yOffset = 0.0f);

private:
    // slot 0..3 플레이어 포인터(소유는 m_skinnedObjects가 함)
    std::array<CGameObject*, 4> m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };

    int m_localPlayerSlot;
    // ------------------------------------------------------------------------
    // Build counts (현재는 BuildObjects()에서 결정, 추후 서버 동기화 값으로 대체)
    // ------------------------------------------------------------------------
    UINT m_grassCount = 1;
    UINT m_groundCount = 1;
    UINT m_villagewallCount = 1;
	UINT m_dirtRoadCount = 1;

    UINT m_building1Count = 1;
    UINT m_building2Count = 1;
    UINT m_building3Count = 1;
    UINT m_building4Count = 1;
    UINT m_building5Count = 1;
    UINT m_building6Count = 1;
    UINT m_building7Count = 1;
    UINT m_building8Count = 1;
    UINT m_building9Count = 1;
	UINT m_towerCount = 1;

    UINT m_ghoulCount = 4;
    UINT m_swordManCount = 3;
    UINT m_bowManCount = 3;
    UINT m_axeManCount = 2;
    UINT m_bossCount = 1;

    UINT m_PlayerCount = 4;

    UINT m_helmetCount = 0;

	UINT m_PlayerSwordCount = 4;
	UINT m_PlayerBowCount = 4;
	UINT m_PlayerAxeCount = 4;
	UINT m_PlayerGunCount = 4;


    std::vector<std::unique_ptr<CGameObject>> m_staticObjects;
    std::vector<std::unique_ptr<CGameObject>> m_skinnedObjects;

    SCENE_STATIC_BATCH  m_staticBatch;
    SCENE_SKINNED_BATCH m_skinnedBatch;

    std::vector<CGameObject*> m_swordManRefs;
    std::vector<CGameObject*> m_bowManRefs;
    std::vector<CGameObject*> m_axeManRefs;

    std::vector<CGameObject*> m_helmetRefs;

    std::vector<CGameObject*> m_PlayerSwordRefs;
    std::vector<CGameObject*> m_PlayerBowRefs;
    std::vector<CGameObject*> m_PlayerAxeRefs;
    std::vector<CGameObject*> m_PlayerGunRefs;

    std::vector<CGameObject*> m_EnemySwordRefs;
    std::vector<CGameObject*> m_EnemyBowRefs;
    std::vector<CGameObject*> m_EnemyAxeRefs;

    std::vector<AttachmentBindSpec> m_attachmentBinds;

    static constexpr UINT kArrowPoolSize = 32;
    std::vector<CGameObject*> m_arrowRefs;
    std::array<CGameObject*, 4> m_preparedPlayerArrows = { nullptr, nullptr, nullptr, nullptr };
    std::array<bool, 4> m_prevBowReleasePhase = { false, false, false, false };
    int GetPlayerSlotFromObject(const CGameObject* obj) const;

    void RequestPrepareArrow(CGameObject* shooter, float pullBackDistance);
    void RequestReleasePreparedArrow(CGameObject* shooter, float speed, float lifeSec = 3.0f);
    void UpdatePreparedBowArrows();

    std::array<CGameObject*, 3> m_demoFighters = { nullptr, nullptr, nullptr };

    std::vector<std::unique_ptr<CGameObject>> m_lightObjects;
    CFollowTransformComponent* m_pPlayerSpotFollower = nullptr;

    // GPU / Shader Variables (Game 전용)
    ComPtr<ID3D12Resource> m_pd3dcbLights;
    LIGHTS* m_pcbMappedLights = nullptr;

    std::unique_ptr<MATERIALS> m_pMaterials;

    ComPtr<ID3D12Resource> m_pd3dcbMaterials;
    MATERIAL* m_pcbMappedMaterials = nullptr;

    unique_ptr<CCollisionSystem> m_Collision;

    //클라 맵 하드리딩용
private:
    bool LoadStaticPlacementFile(const std::string& filePath);
    void ResetStaticPlacementCounts();
    void ApplyStaticPlacementCounts();
    static float QuaternionToYawDegrees(const XMFLOAT4& q);

private:
    std::vector<StaticPlacementEntry>   m_staticPlacementEntries;
    
    std::shared_ptr<CRectUIShader>      m_inactiveOverlayShader;
    std::shared_ptr<CTexture>           m_inactiveOverlayTex;
    UINT                                m_inactiveOverlaySrvIndex = UINT_MAX;
    bool                                m_bInactiveOverlayVisible = false;
};