//-----------------------------------------------------------------------------
// File: GameScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScene.h"

#include <cmath>

#include "AnimatorComponent.h"
#include "AnimController.h"
#include "Material.h"
#include "AssetManager.h"
#include "LightComponent.h"
#include "PlayerControllerComponent.h"
#include "Object.h"
#include "ActorTagComponent.h"
#include "ArrowComponent.h"
#include "Camera.h"

#include "ThreadManager.h"
#include "Service.h"
#include "Session.h"
#include "BufferReader.h"
#include "ServerPacketHandler.h"

CGameScene::CGameScene()
{
    m_staticBatch.capacity = 4 + kArrowPoolSize;
    m_staticBatch.count = 0;

    m_skinnedBatch.capacity = 10;
    m_skinnedBatch.count = 0;

    m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };
    m_localPlayerSlot = 0;

    m_arrowRefs.clear();
    m_arrowRefs.shrink_to_fit();
}

CGameScene::~CGameScene()
{
}

void CGameScene::ReleaseObjects()
{
    m_staticBatch.shader.reset();
    m_skinnedBatch.shader.reset();

    m_staticObjects.clear();
    m_skinnedObjects.clear();

    m_lightObjects.clear();
    m_pPlayerSpotFollower = nullptr;

    m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };

    m_staticBatch.objectRefs.clear();
    m_skinnedBatch.objectRefs.clear();

    ReleaseShaderVariables();

    CScene::ReleaseObjects();
}

void CGameScene::ReleaseUploadBuffers()
{
    for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
    {
        if (!m_staticObjects[j]) continue;
        m_staticObjects[j]->ReleaseUploadBuffers();
    }
    for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
    {
        if (!m_skinnedObjects[j]) continue;
        m_skinnedObjects[j]->ReleaseUploadBuffers();
    }

#ifdef _WITH_BATCH_MATERIAL
    if (m_staticBatch.material)  m_staticBatch.material->ReleaseUploadBuffers();
#endif
}

void CGameScene::ReleaseShaderVariables()
{
    // ---- Static batch CB ----
    if (m_staticBatch.cbGameObjects)
    {
        if (m_staticBatch.mappedGameObjects)
        {
            m_staticBatch.cbGameObjects->Unmap(0, NULL);
            m_staticBatch.mappedGameObjects = nullptr;
        }
        m_staticBatch.cbGameObjects.Reset();
    }

    // ---- Skinned batch CB ----
    if (m_skinnedBatch.cbGameObjects)
    {
        if (m_skinnedBatch.mappedGameObjects)
        {
            m_skinnedBatch.cbGameObjects->Unmap(0, NULL);
            m_skinnedBatch.mappedGameObjects = nullptr;
        }
        m_skinnedBatch.cbGameObjects.Reset();
    }

    if (m_pd3dcbLights)
    {
        m_pd3dcbLights->Unmap(0, NULL);
        m_pd3dcbLights.Reset();
    }
    m_pcbMappedLights = nullptr;

    if (m_pd3dcbMaterials)
    {
        m_pd3dcbMaterials->Unmap(0, NULL);
        m_pd3dcbMaterials.Reset();
    }
    m_pcbMappedMaterials = nullptr;
}

void CGameScene::BuildObjects(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
    // 지금은 하드코딩: 로컬 슬롯 = 2
    m_localPlayerSlot = 2;

    CreateGraphicsRootSignature(dev);

    constexpr int MAX_GLOBAL_SRVS = 1024;

    auto pStaticShader = std::make_shared<CStaticObjectsShader>();
    auto pSkinnedShader = std::make_shared<CSkinnedObjectsShader>();

    m_staticBatch.shader = pStaticShader;
    m_skinnedBatch.shader = pSkinnedShader;

    const UINT cbvTotal =
        m_staticBatch.capacity +
        m_skinnedBatch.capacity +
        1 /*Camera*/ +
        1 /*etc*/;

    m_pDescriptorHeap->CreateCbvSrvDescriptorHeaps(
        dev,
        cbvTotal,
        MAX_GLOBAL_SRVS
    );

    DXGI_FORMAT rtvFormats[5] =
    {
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R32_FLOAT
    };

    BuildLightsAndMaterials();

    for (auto& lo : m_lightObjects)
    {
        if (lo) lo->CreateComponents(dev, cmd);
    }

    constexpr UINT kRTCount = 5;
    const DXGI_FORMAT kDsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    BuildStaticBatch(dev, cmd, pStaticShader, kRTCount, rtvFormats, kDsvFormat);
    BuildSkinnedBatch(dev, cmd, pSkinnedShader, kRTCount, rtvFormats, kDsvFormat);

    CreateShaderVariables(dev, cmd);

    CGameObject* local = GetPlayer();
    if (!local) local = GetPlayerBySlot(0);

    CreateMainCamera(dev, cmd, local);
}

void CGameScene::BuildLightsAndMaterials()
{
    m_lightObjects.clear();
    m_lightObjects.reserve(4);
    m_pPlayerSpotFollower = nullptr;

    // [0] Point Light
    {
        auto obj = std::make_unique<CGameObject>(0);
        obj->SetPosition(1.0f, 0.0f, 0.0f);

        auto* lc = obj->AddComponent<CLightComponent>();
        lc->type = ELightType::Point;
        lc->range = 100.0f;
        lc->ambient = XMFLOAT4(0.1f, 0.0f, 0.0f, 1.0f);
        lc->diffuse = XMFLOAT4(0.8f, 0.0f, 0.0f, 1.0f);
        lc->specular = XMFLOAT4(0.1f, 0.1f, 0.1f, 0.0f);
        lc->attenuation = XMFLOAT3(1.0f, 0.001f, 0.0001f);

        m_lightObjects.push_back(std::move(obj));
    }

    // [1] Spot Light (player follow)
    {
        auto obj = std::make_unique<CGameObject>(0);

        auto* lc = obj->AddComponent<CLightComponent>();
        lc->type = ELightType::Spot;
        lc->range = 50.0f;
        lc->ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
        lc->diffuse = XMFLOAT4(0.4f, 0.4f, 0.4f, 1.0f);
        lc->specular = XMFLOAT4(0.1f, 0.1f, 0.1f, 0.0f);
        lc->attenuation = XMFLOAT3(1.0f, 0.01f, 0.0001f);
        lc->falloff = 8.0f;
        lc->cosPhi = (float)cos(XMConvertToRadians(40.0f));
        lc->cosTheta = (float)cos(XMConvertToRadians(20.0f));

        auto* follow = obj->AddComponent<CFollowTransformComponent>();
        m_pPlayerSpotFollower = follow;

        m_lightObjects.push_back(std::move(obj));
    }

    // [2] Directional Light
    {
        auto obj = std::make_unique<CGameObject>(0);
        if (auto* tr = obj->GetComponent<CTransformComponent>())
            tr->SetLookDirection(XMFLOAT3(1.0f, 0.0f, 0.0f));

        auto* lc = obj->AddComponent<CLightComponent>();
        lc->type = ELightType::Directional;
        lc->ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
        lc->diffuse = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
        lc->specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

        m_lightObjects.push_back(std::move(obj));
    }

    // [3] Spot Light
    {
        auto obj = std::make_unique<CGameObject>(0);
        obj->SetPosition(-150.0f, 30.0f, 30.0f);
        if (auto* tr = obj->GetComponent<CTransformComponent>())
            tr->SetLookDirection(XMFLOAT3(0.0f, 1.0f, 1.0f));

        auto* lc = obj->AddComponent<CLightComponent>();
        lc->type = ELightType::Spot;
        lc->range = 60.0f;
        lc->ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
        lc->diffuse = XMFLOAT4(0.5f, 0.0f, 0.0f, 1.0f);
        lc->specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        lc->attenuation = XMFLOAT3(1.0f, 0.01f, 0.0001f);
        lc->falloff = 8.0f;
        lc->cosPhi = (float)cos(XMConvertToRadians(90.0f));
        lc->cosTheta = (float)cos(XMConvertToRadians(30.0f));

        m_lightObjects.push_back(std::move(obj));
    }

    m_pMaterials = make_unique<MATERIALS>();
    ::ZeroMemory(m_pMaterials.get(), sizeof(MATERIALS));

    m_pMaterials->m_pReflections[0] = {
        XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 5.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[1] = {
        XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f),
        XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 10.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[2] = {
        XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 15.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[3] = {
        XMFLOAT4(0.5f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(0.0f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 20.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[4] = {
        XMFLOAT4(0.0f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(0.5f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 25.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[5] = {
        XMFLOAT4(0.0f, 0.5f, 0.5f, 1.0f),
        XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 30.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[6] = {
        XMFLOAT4(0.5f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(0.5f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 35.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    m_pMaterials->m_pReflections[7] = {
        XMFLOAT4(1.0f, 0.5f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f),
        XMFLOAT4(1.0f, 1.0f, 1.0f, 40.0f),
        XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)
    };

    for (int i = 0; i < MAX_MATERIALS; ++i)
        m_pMaterials->m_pReflections[i].m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);
}

void CGameScene::CreateShaderVariables(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
    UINT ncbElementBytes = ((sizeof(LIGHTS) + 255) & ~255);
    m_pd3dcbLights = ::CreateBufferResource(
        dev, cmd, nullptr,
        ncbElementBytes,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        nullptr
    );
    m_pd3dcbLights->Map(0, nullptr, (void**)&m_pcbMappedLights);

    UINT ncbMaterialBytes = ((sizeof(MATERIALS) + 255) & ~255);
    m_pd3dcbMaterials = ::CreateBufferResource(
        dev, cmd, nullptr,
        ncbMaterialBytes,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        nullptr
    );
    m_pd3dcbMaterials->Map(0, nullptr, (void**)&m_pcbMappedMaterials);
}

void CGameScene::BuildStaticBatch(
    ID3D12Device* dev,
    ID3D12GraphicsCommandList* cmd,
    const std::shared_ptr<CStaticObjectsShader>& pStaticShader,
    UINT nRenderTargets,
    DXGI_FORMAT* rtvFormats,
    DXGI_FORMAT dsvFormat
)
{
    auto* b = &m_staticBatch;
    if (!b) return;

    if (b->capacity < 4) b->capacity = 4;
    const UINT cap = b->capacity;
    if (cap == 0) return;

    pStaticShader->CreateShader(
        dev,
        m_pd3dGraphicsRootSignature.Get(),
        nRenderTargets,
        rtvFormats,
        dsvFormat
    );

    b->cbElementBytes = ((sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255);

    b->cbGameObjects = ::CreateBufferResource(
        dev, cmd, nullptr,
        b->cbElementBytes * cap,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        nullptr
    );

    b->cbGameObjects->Map(0, nullptr, (void**)&b->mappedGameObjects);

    b->baseCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
    b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

    m_pDescriptorHeap->CreateConstantBufferViews(
        dev,
        cap,
        b->cbGameObjects.Get(),
        b->cbElementBytes
    );

    m_staticObjects.clear();
    m_staticObjects.reserve(cap);

    b->objectRefs.clear();
    b->objectRefs.reserve(cap);

    b->count = 0;

    struct StaticAssetDesc
    {
        AssetType type;
        const char* meshBin;
        const char* texDir;
    };

    StaticAssetDesc descs[4] =
    {
        { AssetType::Plane, "Assets/GroundPlane/Mesh/Plane.bin", "Assets/GroundPlane/Texture" },
        { AssetType::House, "Assets/House/Mesh/Barn1.bin", "Assets/House/Texture" },
        { AssetType::House, "Assets/House/Mesh/Barn2.bin", "Assets/House/Texture" },
        { AssetType::House, "Assets/House/Mesh/Cabin1.bin", "Assets/House/Texture" },
    };
    XMFLOAT3 positions[4] =
    {
        XMFLOAT3(0.0f, 0.0f, 0.0f),
        XMFLOAT3(22.0f, 0.0f, 12.0f),
        XMFLOAT3(-20.0f, 0.0f, 0.0f),
        XMFLOAT3(0.0f, 0.0f, -20.0f),
    };

    for (UINT k = 0; k < 4; ++k)
    {
        if (b->objectRefs.size() >= b->capacity) break;

        const UINT i = (UINT)b->objectRefs.size();

        AssetBuildDesc Desc =
        {
            descs[k].type,
            descs[k].meshBin,
            descs[k].texDir
        };

        BuiltAsset asset = AssetManager::BuildAsset(
            dev, cmd,
            m_pMaterials.get(),
            Desc
        );

        auto obj = std::make_unique<CGameObject>(1);

        auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
        obj->SetMappedGameObjectCB(cb);

        obj->SetMesh(0, asset.mesh);
        obj->AddComponent<CStaticMeshRendererComponent>();

        obj->SetPosition(positions[k]);

        obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

        obj->CreateComponents(dev, cmd);

        CGameObject* raw = obj.get();
        m_staticObjects.push_back(std::move(obj));
        b->objectRefs.push_back(raw);
        b->count = (UINT)b->objectRefs.size();
    }

    // ------------------------------------------------------------------------
    // Arrow pool (Static)
    // ------------------------------------------------------------------------
    {
        AssetBuildDesc ArrowDesc =
        {
            AssetType::Arrow,
            "Assets/Arrow/Mesh/Arrow.bin",
            "Assets/Arrow/Texture"
        };

        BuiltAsset arrowAsset = AssetManager::BuildAsset(
            dev, cmd,
            m_pMaterials.get(),
            ArrowDesc
        );

        m_arrowRefs.clear();
        m_arrowRefs.reserve(kArrowPoolSize);

        for (UINT k = 0; k < kArrowPoolSize; ++k)
        {
            if (b->objectRefs.size() >= b->capacity) break;

            const UINT i = (UINT)b->objectRefs.size();

            auto obj = std::make_unique<CGameObject>(1);

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
            obj->SetMappedGameObjectCB(cb);

            obj->SetMesh(0, arrowAsset.mesh);
            obj->AddComponent<CStaticMeshRendererComponent>();

            auto* arrow = obj->AddComponent<CArrowComponent>();
            (void)arrow;

            obj->SetPosition(0.0f, -10000.0f, 0.0f);

            obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

            obj->CreateComponents(dev, cmd);

            CGameObject* raw = obj.get();
            m_staticObjects.push_back(std::move(obj));
            b->objectRefs.push_back(raw);
            b->count = (UINT)b->objectRefs.size();

            m_arrowRefs.push_back(raw);
        }
    }
}

void CGameScene::BuildSkinnedBatch(
    ID3D12Device* dev,
    ID3D12GraphicsCommandList* cmd,
    const std::shared_ptr<CSkinnedObjectsShader>& pSkinnedShader,
    UINT nRenderTargets,
    DXGI_FORMAT* rtvFormats,
    DXGI_FORMAT dsvFormat
)
{
    auto* b = &m_skinnedBatch;
    if (!b) return;

    const UINT cap = b->capacity;
    if (cap == 0) return;

    pSkinnedShader->CreateShader(
        dev,
        m_pd3dGraphicsRootSignature.Get(),
        nRenderTargets,
        rtvFormats,
        dsvFormat
    );

    b->cbElementBytes = ((sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255);

    b->cbGameObjects = ::CreateBufferResource(
        dev, cmd, nullptr,
        b->cbElementBytes * cap,
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        nullptr
    );

    b->cbGameObjects->Map(0, nullptr, (void**)&b->mappedGameObjects);

    b->baseCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
    b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

    m_pDescriptorHeap->CreateConstantBufferViews(
        dev,
        cap,
        b->cbGameObjects.Get(),
        b->cbElementBytes
    );

    m_skinnedObjects.clear();
    m_skinnedObjects.reserve(cap);

    b->objectRefs.clear();
    b->objectRefs.reserve(cap);

    b->count = 0;

    m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };

    const UINT fighterCount = 4;
    const UINT zombieCount = 3;

    const XMFLOAT3 playerBase(0.0f, 0.0f, 0.0f);

    // ------------------------------------------------------------------------
    // Zombie (NPC)
    // ------------------------------------------------------------------------
    {
        AssetBuildDesc ZombieDesc =
        {
            AssetType::Zombie,
            "Assets/Zombie/Mesh/Zombie.bin",
            "Assets/Zombie/Texture"
        };

        BuiltAsset asset = AssetManager::BuildAsset(dev, cmd, m_pMaterials.get(), ZombieDesc);

        for (UINT k = 0; k < zombieCount; ++k)
        {
            if (b->objectRefs.size() >= b->capacity) break;

            const UINT i = (UINT)b->objectRefs.size();

            auto obj = std::make_unique<CGameObject>(1);

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
            obj->SetMappedGameObjectCB(cb);

            obj->SetMesh(0, asset.mesh);
            obj->AddComponent<CSkinnedMeshRendererComponent>();

            {
                auto* tag = obj->AddComponent<CActorTagComponent>();
                tag->kind = EActorKind::NPC;
                tag->control = EPlayerControl::None;
                tag->playerSlot = -1;
            }

            const float x = playerBase.x + 2.0f * (float)k;
            const float z = playerBase.z + 2.0f;

            obj->SetPosition(x, playerBase.y, z);
            obj->Rotate(0.0f, 180.0f, 0.0f);

            obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

            if (asset.mesh && asset.mesh->IsSkinnedMesh())
            {
                obj->EnableSkinning(dev, asset.mesh->GetBoneCount());
            }

            AnimationClip idleClip;
            bool idleLoaded = false;

            auto mesh0 = obj->GetMeshShared(0);
            if (mesh0)
            {
                idleLoaded = mesh0->LoadAnimationFromBIN(
                    "Assets/Zombie/Animation/ZombieIdle.bin",
                    "Idle", idleClip, 1.0f
                );
            }

            if (idleLoaded)
            {
                idleClip.name = "Idle";

                CAnimator* anim = obj->EnsureAnimator();
                if (anim) anim->AddClip(idleClip);

                auto* ctrl = obj->EnsureAnimController();
                ctrl->SetIdleClip("Idle");
                ctrl->SetMoveClip("Idle");
                ctrl->SetSpeed(0.0f);
                ctrl->Update(0.0f);

                obj->Animate(0.0f);
            }

            obj->CreateComponents(dev, cmd);

            CGameObject* raw = obj.get();

            m_skinnedObjects.push_back(std::move(obj));
            b->objectRefs.push_back(raw);
            b->count = (UINT)b->objectRefs.size();
        }
    }

    // ------------------------------------------------------------------------
    // Fighter (Players slot 0..3) : 전원 m_skinnedObjects에 포함
    //  - m_localPlayerSlot(현재 2)만 Local + PlayerControllerComponent 부착
    // ------------------------------------------------------------------------
    {
        AssetBuildDesc FighterDesc =
        {
            AssetType::Fighter,
            "Assets/Fighter/Mesh/Fighter.bin",
            "Assets/Fighter/Texture"
        };

        BuiltAsset asset = AssetManager::BuildAsset(dev, cmd, m_pMaterials.get(), FighterDesc);

        for (UINT k = 0; k < fighterCount; ++k)
        {
            if (b->objectRefs.size() >= b->capacity) break;

            const UINT i = (UINT)b->objectRefs.size();

            const int slot = (int)k;
            const bool isLocal = (slot == m_localPlayerSlot);

            auto obj = std::make_unique<CGameObject>(1);

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
            obj->SetMappedGameObjectCB(cb);

            obj->SetMesh(0, asset.mesh);
            obj->AddComponent<CSkinnedMeshRendererComponent>();

            auto* animComp = obj->AddComponent<CAnimatorComponent>();

            {
                auto* tag = obj->AddComponent<CActorTagComponent>();
                tag->kind = EActorKind::Player;
                tag->control = isLocal ? EPlayerControl::Local : EPlayerControl::Remote;
                tag->playerSlot = slot;
            }

            if (isLocal)
            {
                obj->AddComponent<CPlayerControllerComponent>();
            }

            UINT matId = 0;
            if (asset.mesh)
            {
                for (auto& sm : asset.mesh->m_SubMeshes)
                {
                    if (sm.materialId == 0xFFFFFFFFu) continue;
                    matId = sm.materialId;
                    break;
                }
            }

            auto mat = std::make_shared<CMaterial>();
            mat->m_nReflection = matId;

            const float x = playerBase.x + 2.0f * (float)slot;
            obj->SetPosition(x, playerBase.y, playerBase.z);
            obj->Rotate(0.0f, 0.0f, 0.0f);

            obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

            if (asset.mesh && asset.mesh->IsSkinnedMesh())
            {
                obj->EnableSkinning(dev, asset.mesh->GetBoneCount());
            }

            auto mesh0 = obj->GetMeshShared(0);

            AnimationClip idleClip{};
            AnimationClip runClip{};
            AnimationClip atkClip{};
            bool idleLoaded = false;
            bool runLoaded = false;
            bool atkLoaded = false;

            if (mesh0)
            {
                idleLoaded = mesh0->LoadAnimationFromBIN(
                    "Assets/Fighter/Animation/FighterIdle.bin",
                    "Idle", idleClip, 1.0f
                );

                runLoaded = mesh0->LoadAnimationFromBIN(
                    "Assets/Fighter/Animation/FighterRun.bin",
                    "Run", runClip, 1.0f
                );

                atkLoaded = mesh0->LoadAnimationFromBIN(
                    "Assets/Fighter/Animation/FighterAttack.bin",
                    "Attack", atkClip, 1.0f
                );
            }

            if (animComp)
            {
                if (idleLoaded) { idleClip.name = "Idle";   animComp->AddClip(idleClip); }
                if (runLoaded) { runClip.name = "Run";    animComp->AddClip(runClip); }
                if (atkLoaded) { atkClip.name = "Attack"; animComp->AddClip(atkClip); }

                animComp->SetIdleClip("Idle");
                animComp->SetMoveClip(runLoaded ? "Run" : "Idle");

                auto* ctrl = animComp->EnsureController();
                if (ctrl)
                {
                    ctrl->SetAttackClip("Attack");
                    ctrl->SetSpeed(0.0f);
                    ctrl->Update(0.0f);
                }
            }

            obj->CreateComponents(dev, cmd);
            if (animComp) animComp->EvaluatePose(0.0f);

            CGameObject* raw = obj.get();

            if (slot >= 0 && slot <= 3)
                m_playersBySlot[(size_t)slot] = raw;

            m_skinnedObjects.push_back(std::move(obj));
            b->objectRefs.push_back(raw);
            b->count = (UINT)b->objectRefs.size();
        }
    }
}


void CGameScene::CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* target)
{
    m_pMainCameraObject = std::make_unique<CGameObject>(0);

    auto* cam = m_pMainCameraObject->AddComponent<CThirdPersonCamera>();
    m_pMainCamera = cam;

    cam->SetMode(THIRD_PERSON_CAMERA);
    cam->SetTarget(target);

    cam->SetTimeLag(0.25f);
    cam->SetOffset(XMFLOAT3(0.0f, 1.0f, -2.0f));
    cam->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
    cam->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
    cam->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);

    m_pMainCameraObject->CreateComponents(dev, cmd);

    if (target)
    {
        XMFLOAT3 pos = target->GetPosition();
        cam->SetPosition(Vector3::Add(pos, cam->GetOffset()));
        cam->Update(pos, 0.0f);
        cam->SetLookAt(pos);
        cam->RegenerateViewMatrix();
    }
}

void CGameScene::SetMaterialDiffuseSrvIndex(int materialId, UINT srvIndex)
{
    if (!m_pMaterials) return;
    if (materialId < 0 || materialId >= MAX_MATERIALS) return;
    m_pMaterials->m_pReflections[materialId].m_xmn4TextureIndices.x = srvIndex;
}

CGameObject* CGameScene::GetDemoFighter(int index) const
{
    if (index < 0 || index >= 3) return nullptr;
    return GetPlayerBySlot(index + 1);
}

void CGameScene::RequestDemoFighterAttack(int index)
{
    if (index < 0 || index >= 3) return;
    RequestPlayerAttackBySlot(index + 1);
}

void CGameScene::RequestPlayerAttackBySlot(int slot)
{
    CGameObject* obj = GetPlayerBySlot(slot);
    if (!obj) return;

    const float kArrowSpeed = 3.0f;
    const float kArrowLife = 6.0f;
    const float kArrowYOffset = 1.0f;

    if (auto* animComp = obj->GetComponent<CAnimatorComponent>())
    {
        if (auto* ctrl = animComp->EnsureController())
        {
            ctrl->RequestAttack();
            RequestFireArrow(obj, kArrowSpeed, kArrowLife, kArrowYOffset);
            return;
        }
    }

    if (auto* ctrl = obj->GetAnimController())
    {
        ctrl->RequestAttack();
        RequestFireArrow(obj, kArrowSpeed, kArrowLife, kArrowYOffset);
        return;
    }
}

CGameObject* CGameScene::GetPlayerBySlot(int slot) const
{
    if (slot < 0 || slot > 3) return nullptr;
    return m_playersBySlot[(size_t)slot];
}

bool CGameScene::IsLocalPlayer(const CGameObject* obj) const
{
    if (!obj) return false;
    auto* tag = obj->GetComponent<CActorTagComponent>();
    return tag && tag->kind == EActorKind::Player && tag->control == EPlayerControl::Local;
}

bool CGameScene::OnProcessingMouseMessage(HWND /*hWnd*/, UINT msg, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    if (msg == WM_LBUTTONDOWN)
    {
        RequestPlayerAttackBySlot(m_localPlayerSlot);
        return true;
    }
    return false;
}

bool CGameScene::OnProcessingKeyboardMessage(HWND /*hWnd*/, UINT /*msg*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    return false;
}

void CGameScene::RequestFireArrow(CGameObject* shooter, float speed, float lifeSec, float yOffset)
{
    if (!shooter) return;

    const XMFLOAT4X4& W = shooter->GetWorldMatrix();
    XMFLOAT3 dir = { W._31, W._32, W._33 };

    XMVECTOR dirV = XMLoadFloat3(&dir);
    const float lenSq = XMVectorGetX(XMVector3LengthSq(dirV));
    if (lenSq < 1e-8f)
    {
        dir = XMFLOAT3(0.0f, 0.0f, 1.0f);
        dirV = XMLoadFloat3(&dir);
    }

    dirV = XMVector3Normalize(dirV);

    XMFLOAT3 dirN{};
    XMStoreFloat3(&dirN, dirV);

    XMFLOAT3 startPos = shooter->GetPosition();
    startPos.y += yOffset;

    const XMFLOAT3 vel =
    {
        dirN.x * speed,
        dirN.y * speed,
        dirN.z * speed
    };

    for (CGameObject* arrowObj : m_arrowRefs)
    {
        if (!arrowObj) continue;

        auto* arrow = arrowObj->GetComponent<CArrowComponent>();
        if (!arrow) continue;

        if (arrow->IsActive()) continue;

        if (auto* tr = arrowObj->GetComponent<CTransformComponent>())
        {
            tr->SetLookDirection(dirN);
        }

        arrow->Activate(startPos, vel, lifeSec);
        return;
    }
}

bool CGameScene::ProcessInput(UCHAR* /*pKeysBuffer*/)
{
    return false;
}

void CGameScene::AnimateObjects(float dt)
{
    for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
    {
        if (!m_staticObjects[j]) continue;
        m_staticObjects[j]->Animate(dt);
    }

    for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
    {
        if (!m_skinnedObjects[j]) continue;
        m_skinnedObjects[j]->Animate(dt);
    }

    CGameObject* local = GetPlayer();
    if (local && m_pPlayerSpotFollower && (m_pPlayerSpotFollower->GetTarget() == nullptr))
    {
        m_pPlayerSpotFollower->SetTarget(local);
    }

    for (UINT j = 0; j < (UINT)m_lightObjects.size(); ++j)
    {
        if (!m_lightObjects[j]) continue;
        m_lightObjects[j]->Animate(dt);
    }
}

void CGameScene::UpdateShaderVariables(ID3D12GraphicsCommandList* /*cmd*/)
{
    if (m_pcbMappedLights)
    {
        ::ZeroMemory(m_pcbMappedLights, sizeof(LIGHTS));
        m_pcbMappedLights->m_xmf4GlobalAmbient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);

        UINT li = 0;
        for (auto& obj : m_lightObjects)
        {
            if (!obj) continue;

            auto* lc = obj->GetComponent<CLightComponent>();
            if (!lc) continue;
            if (!lc->IsEnabled()) continue;
            if (li >= MAX_LIGHTS) break;

            lc->Fill(m_pcbMappedLights->m_pLights[li]);
            ++li;
        }
    }

    if (m_pcbMappedMaterials && m_pMaterials)
        ::memcpy(m_pcbMappedMaterials, m_pMaterials.get(), sizeof(MATERIALS));

    if (m_staticBatch.mappedGameObjects && !m_staticBatch.objectRefs.empty())
    {
        const UINT ncb = m_staticBatch.cbElementBytes;

        for (UINT j = 0; j < (UINT)m_staticBatch.objectRefs.size(); ++j)
        {
            auto* obj = m_staticBatch.objectRefs[j];
            if (!obj) continue;

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)m_staticBatch.mappedGameObjects + j * ncb);

            const XMFLOAT4X4& W = obj->GetWorldMatrix();

            XMStoreFloat4x4(
                &cb->m_xmf4x4World,
                XMMatrixTranspose(XMLoadFloat4x4(&W))
            );

            cb->m_nObjectID = j;
        }
    }

    if (m_skinnedBatch.mappedGameObjects && !m_skinnedBatch.objectRefs.empty())
    {
        const UINT ncb = m_skinnedBatch.cbElementBytes;

        for (UINT j = 0; j < (UINT)m_skinnedBatch.objectRefs.size(); ++j)
        {
            auto* obj = m_skinnedBatch.objectRefs[j];
            if (!obj) continue;

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)m_skinnedBatch.mappedGameObjects + j * ncb);

            const XMFLOAT4X4& W = obj->GetWorldMatrix();

            XMStoreFloat4x4(
                &cb->m_xmf4x4World,
                XMMatrixTranspose(XMLoadFloat4x4(&W))
            );

            cb->m_nObjectID = j;
        }
    }
}

void CGameScene::OnPrepareRender(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
    CScene::OnPrepareRender(cmd, camera);

    UpdateShaderVariables(cmd);

    if (m_pd3dcbLights)
    {
        D3D12_GPU_VIRTUAL_ADDRESS lightsGpu = m_pd3dcbLights->GetGPUVirtualAddress();
        cmd->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_LIGHT, lightsGpu);
    }

    if (m_pd3dcbMaterials)
    {
        D3D12_GPU_VIRTUAL_ADDRESS matsGpu = m_pd3dcbMaterials->GetGPUVirtualAddress();
        cmd->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_MATERIAL, matsGpu);
    }
}

void CGameScene::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
    if (m_staticBatch.shader)
    {
        m_staticBatch.shader->Render(cmd, camera, &m_staticBatch);
        for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
        {
            if (!m_staticObjects[j]) continue;
            m_staticObjects[j]->Render(cmd, camera);
        }
    }

    if (m_skinnedBatch.shader)
    {
        m_skinnedBatch.shader->Render(cmd, camera, &m_skinnedBatch);
        for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
        {
            if (!m_skinnedObjects[j]) continue;
            m_skinnedObjects[j]->Render(cmd, camera);
        }
    }
}