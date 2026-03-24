//-----------------------------------------------------------------------------
// File: GameScene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScene.h"

#include <cmath>
#include <fstream>
#include <cstdio>

#include "AnimatorComponent.h"
#include "AnimController.h"
#include "MonsterAnimController.h"
#include "MonsterAnimTypes.h"
#include "Material.h"
#include "AssetManager.h"
#include "Texture.h"
#include "LightComponent.h"
#include "PlayerControllerComponent.h"
#include "Object.h"
#include "ActorTagComponent.h"
#include "ArrowComponent.h"
#include "BulletComponent.h"
#include "Camera.h"
#include "FollowBoneComponent.h"
#include "PlayerEquipmentComponent.h"
#include "CollisionSystem.h"
#include "ColliderComponent.h"

#include "ThreadManager.h"
#include "Service.h"
#include "Session.h"
#include "BufferReader.h"
#include "ServerPacketHandler.h"

#include "GlobalValues.h"

namespace
{
    bool ParsePlacementEntryLine(const std::string& line, StaticPlacementEntry& outEntry)
    {
        char asset[64] = {};
        char objectName[128] = {};

        float px = 0.0f, py = 0.0f, pz = 0.0f;
        float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f;

        const int matched = sscanf_s(
            line.c_str(),
            "ENTRY|asset=\"%63[^\"]\"|object=\"%127[^\"]\"|pos=(%f,%f,%f)|rot=(%f,%f,%f,%f)",
            asset, (unsigned)_countof(asset),
            objectName, (unsigned)_countof(objectName),
            &px, &py, &pz,
            &qx, &qy, &qz, &qw
        );

        if (matched != 9) return false;

        outEntry.assetName = asset;
        outEntry.objectName = objectName;
        outEntry.pos = XMFLOAT3(px, py, pz);
        outEntry.rot = XMFLOAT4(qx, qy, qz, qw);
        return true;
    }

    bool ResolveStaticAssetDesc(const std::string& assetName, AssetBuildDesc& outDesc)
    {
        if (assetName == "Grass")
        {
            outDesc = { AssetType::Grass, "Assets/GroundPlane/Mesh/Grass.bin", "Assets/GroundPlane/Texture" };
            return true;
        }
        if (assetName == "Ground")
        {
            outDesc = { AssetType::Ground, "Assets/GroundPlane/Mesh/Ground.bin", "Assets/GroundPlane/Texture" };
            return true;
        }
        if (assetName == "VillageWall")
        {
            outDesc = { AssetType::VillageWall, "Assets/VillageWall/Mesh/VillageWall.bin", "Assets/VillageWall/Texture" };
            return true;
        }
        if (assetName == "DirtRoad")
        {
            outDesc = { AssetType::DirtRoad, "Assets/GroundPlane/Mesh/DirtRoad.bin", "Assets/GroundPlane/Texture" };
            return true;
        }
        if (assetName == "Building1")
        {
            outDesc = { AssetType::House, "Assets/House/Mesh/Building1.bin", "Assets/House/Texture" };
            return true;
        }
        if (assetName == "Building2")
        {
            outDesc = { AssetType::House, "Assets/House/Mesh/Building2.bin", "Assets/House/Texture" };
            return true;
        }
        if (assetName == "Building3")
        {
            outDesc = { AssetType::House, "Assets/House/Mesh/Building3.bin", "Assets/House/Texture" };
            return true;
        }
        if (assetName == "Building4")
        {
            outDesc = { AssetType::House, "Assets/House/Mesh/Building4.bin", "Assets/House/Texture" };
            return true;
        }
        if (assetName == "Building5")
        {
            outDesc = { AssetType::House, "Assets/House/Mesh/Building5.bin", "Assets/House/Texture" };
            return true;
        }
        if (assetName == "Building6")
        {
            outDesc = { AssetType::House, "Assets/House/Mesh/Building6.bin", "Assets/House/Texture" };
            return true;
        }
        if (assetName == "Building7")
        {
            outDesc = { AssetType::House, "Assets/House/Mesh/Building7.bin", "Assets/House/Texture" };
            return true;
        }
        if (assetName == "Building8")
        {
            outDesc = { AssetType::House, "Assets/House/Mesh/Building8.bin", "Assets/House/Texture" };
            return true;
        }
        if (assetName == "Building9")
        {
            outDesc = { AssetType::House, "Assets/House/Mesh/Building9.bin", "Assets/House/Texture" };
            return true;
        }
        if (assetName == "Tower")
        {
            outDesc = { AssetType::Tower, "Assets/Tower/Mesh/Tower.bin", "Assets/Tower/Texture" };
            return true;
        }

        return false;
    }

	void TriggerMonsterTestCommand(CGameObject* obj, EMonsterAnimCommand cmd, EMonsterAnimState locomotion = EMonsterAnimState::Idle)
	{
		if ( !obj ) return;

		auto* animComp = obj->GetComponent<CAnimatorComponent>();
		if ( !animComp ) return;

		auto* ctrl = animComp->EnsureMonsterController();
		if ( !ctrl ) return;

		ctrl->SetLocomotionState(locomotion);
		ctrl->RequestCommand(cmd);
	}
}

CGameScene::CGameScene()
{
    m_playersBySlot = { nullptr, nullptr, nullptr, nullptr };
    m_localPlayerSlot = 0;

	m_grassCount = 1;
    m_groundCount = 1;
    m_villagewallCount = 1;
	m_dirtRoadCount = 1;

    m_building1Count = 1;
    m_building2Count = 1;
    m_building3Count = 1;
    m_building4Count = 1;
    m_building5Count = 1;
    m_building6Count = 1;
    m_building7Count = 1;
    m_building8Count = 1;
    m_building9Count = 1;
    m_towerCount = 1;

    m_ghoulCount = 4;
    m_swordManCount = 3;
    m_bowManCount = 3;
    m_MutantCount = 2;
    m_bossCount = 1;

    m_PlayerCount = 4;

    m_staticBatch.capacity = 0;
    m_staticBatch.count = 0;

    m_skinnedBatch.capacity = 0;
    m_skinnedBatch.count = 0;

    m_arrowRefs.clear();
    m_arrowRefs.shrink_to_fit();

	m_bulletRefs.clear();
	m_bulletRefs.shrink_to_fit();
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

    m_swordManRefs.clear();
    m_bowManRefs.clear();
    m_MutantRefs.clear();

	m_helmetRefs.clear();
	m_arrowRefs.clear();
	m_bulletRefs.clear();
	m_attachmentBinds.clear();

    m_PlayerSwordRefs.clear();
    m_PlayerBowRefs.clear();
    m_PlayerAxeRefs.clear();
    m_PlayerGunRefs.clear();

    m_EnemySwordRefs.clear();
    m_EnemyBowRefs.clear();

    m_preparedPlayerArrows = { nullptr, nullptr, nullptr, nullptr };
    m_prevBowReleasePhase = { false, false, false, false };
	m_preparedBowmanArrows.clear();
	m_prevEnemyBowReleasePhase.clear();

    m_inactiveOverlayShader.reset();
    m_inactiveOverlayTex.reset();
    m_inactiveOverlaySrvIndex = UINT_MAX;
    m_bInactiveOverlayVisible = false;

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
#ifdef USING_NETWORK
    while (false == g_GameStarted);
    DequeueNetworkMessage(NetworkMessageType::GameStart);
    m_localPlayerSlot = g_myPlayerId;
#else
    m_localPlayerSlot = 1;
#endif

    const std::string placementFilePath = "Assets/placement_export_st1.txt";

    if (!LoadStaticPlacementFile(placementFilePath))
    {
        assert(false && "Failed to load placement_export");
        return;
    }

    m_PlayerCount = 4;

    m_PlayerSwordCount = m_PlayerCount;
    m_PlayerBowCount = m_PlayerCount;
    m_PlayerAxeCount = m_PlayerCount;
    m_PlayerGunCount = m_PlayerCount;

    m_helmetCount = m_MutantCount;

    const UINT worldStaticCount = static_cast<UINT>(m_staticPlacementEntries.size());

	m_staticBatch.capacity =
		worldStaticCount +
		kArrowPoolSize +
		kBulletPoolSize +
		m_helmetCount +
		m_PlayerSwordCount +
		m_PlayerAxeCount +
		m_PlayerGunCount +
		m_swordManCount;

    m_skinnedBatch.capacity =
        m_ghoulCount +
        m_swordManCount +
        m_bowManCount +
        m_MutantCount +
        m_bossCount +
        m_PlayerCount +
        m_PlayerBowCount +
        m_bowManCount;

    m_staticBatch.count = 0;
    m_skinnedBatch.count = 0;

    CreateGraphicsRootSignature(dev);

    auto pStaticShader = std::make_shared<CStaticObjectsShader>();
    auto pSkinnedShader = std::make_shared<CSkinnedObjectsShader>();

    m_staticBatch.shader = pStaticShader;
    m_skinnedBatch.shader = pSkinnedShader;

    DXGI_FORMAT rtvFormats[5] =
    {
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R32_FLOAT
    };

    BuildLightsAndMaterials();

    {
        constexpr const wchar_t* kInactiveOverlayDDS = L"Assets/UI/Pause.dds";

        m_inactiveOverlayTex = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 0);
        m_inactiveOverlayTex->LoadTextureFromFile(dev, cmd, kInactiveOverlayDDS, RESOURCE_TEXTURE2D, 0);

        CScene::m_pDescriptorHeap->CreateShaderResourceViewsOther(
            dev,
            m_inactiveOverlayTex.get(),
            ROOT_PARAMETER_GLOBAL_SRV
        );

        m_inactiveOverlaySrvIndex = m_inactiveOverlayTex->GetSrvIndex(0);

        m_inactiveOverlayShader = std::make_shared<CRectUIShader>();

        DXGI_FORMAT overlayRtv = DXGI_FORMAT_R8G8B8A8_UNORM;
        DXGI_FORMAT overlayDsv = DXGI_FORMAT_UNKNOWN;

        m_inactiveOverlayShader->CreateShader(
            dev,
            GetGraphicsRootSignature(),
            1,
            &overlayRtv,
            overlayDsv
        );
        m_inactiveOverlayShader->CreateShaderVariables(dev, cmd);
    }

    for (auto& lo : m_lightObjects)
    {
        if (lo) lo->CreateComponents(dev, cmd);
    }

    constexpr UINT kRTCount = 5;
    const DXGI_FORMAT kDsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    BuildStaticBatch(dev, cmd, pStaticShader, kRTCount, rtvFormats, kDsvFormat);
    BuildSkinnedBatch(dev, cmd, pSkinnedShader, kRTCount, rtvFormats, kDsvFormat);
    BuildObjectsCollider();

    LinkSceneObjects();

    CreateShaderVariables(dev, cmd);

    CGameObject* local = GetPlayer();
    if (!local) local = GetPlayerBySlot(0);

    CreateMainCamera(dev, cmd, local);
}

float CGameScene::QuaternionToYawDegrees(const XMFLOAT4& q)
{
    // yaw(heading) only
    const float siny_cosp = 2.0f * (q.w * q.y + q.x * q.z);
    const float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);

    const float yawRad = std::atan2(siny_cosp, cosy_cosp);
    return XMConvertToDegrees(yawRad);
}

void CGameScene::ResetStaticPlacementCounts()
{
    m_grassCount = 0;
    m_groundCount = 0;
    m_villagewallCount = 0;
    m_dirtRoadCount = 0;

    m_building1Count = 0;
    m_building2Count = 0;
    m_building3Count = 0;
    m_building4Count = 0;
    m_building5Count = 0;
    m_building6Count = 0;
    m_building7Count = 0;
    m_building8Count = 0;
    m_building9Count = 0;
    m_towerCount = 0;
}

void CGameScene::ApplyStaticPlacementCounts()
{
    ResetStaticPlacementCounts();

    for (const auto& e : m_staticPlacementEntries)
    {
        if (e.assetName == "Grass")       ++m_grassCount;
        else if (e.assetName == "Ground")      ++m_groundCount;
        else if (e.assetName == "VillageWall") ++m_villagewallCount;
        else if (e.assetName == "DirtRoad")    ++m_dirtRoadCount;
        else if (e.assetName == "Building1")   ++m_building1Count;
        else if (e.assetName == "Building2")   ++m_building2Count;
        else if (e.assetName == "Building3")   ++m_building3Count;
        else if (e.assetName == "Building4")   ++m_building4Count;
        else if (e.assetName == "Building5")   ++m_building5Count;
        else if (e.assetName == "Building6")   ++m_building6Count;
        else if (e.assetName == "Building7")   ++m_building7Count;
        else if (e.assetName == "Building8")   ++m_building8Count;
        else if (e.assetName == "Building9")   ++m_building9Count;
        else if (e.assetName == "Tower")       ++m_towerCount;
    }
}

bool CGameScene::LoadStaticPlacementFile(const std::string& filePath)
{
    m_staticPlacementEntries.clear();
    ResetStaticPlacementCounts();

    std::ifstream fin(filePath);
    if (!fin.is_open())
        return false;

    std::string line;
    while (std::getline(fin, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        if (line.rfind("ENTRY|", 0) != 0)
            continue;

        StaticPlacementEntry entry{};
        if (!ParsePlacementEntryLine(line, entry))
            continue;

        entry.yawDeg = QuaternionToYawDegrees(entry.rot);
        m_staticPlacementEntries.push_back(std::move(entry));
    }

    ApplyStaticPlacementCounts();
    return !m_staticPlacementEntries.empty();
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
        {
            // 태양광 방향
            tr->SetLookDirection(XMFLOAT3(1.0f, 0.0f, 0.0f));
        }

        auto* lc = obj->AddComponent<CLightComponent>();
        lc->type = ELightType::Directional;

        lc->ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
        lc->diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
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
		/*XMFLOAT4(1,1,1,1),
	XMFLOAT4(1,1,1,1),
	XMFLOAT4(1,1,1,16),
	XMFLOAT4(0,0,0,1)*/
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

    // ------------------------------------------------------------------------
    // Static world objects from placement file
    // ------------------------------------------------------------------------
    for (UINT k = 0; k < (UINT)m_staticPlacementEntries.size(); ++k)
    {
        if (b->objectRefs.size() >= b->capacity) break;

        const UINT i = (UINT)b->objectRefs.size();
        const StaticPlacementEntry& placement = m_staticPlacementEntries[k];

        AssetBuildDesc desc{};
        if (!ResolveStaticAssetDesc(placement.assetName, desc))
            continue;

        BuiltAsset asset = AssetManager::BuildAsset(
            dev, cmd,
            m_pMaterials.get(),
            desc
        );

        auto obj = std::make_unique<CGameObject>(1);

        auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
        obj->SetMappedGameObjectCB(cb);

        obj->SetMesh(0, asset.mesh);
        obj->AddComponent<CStaticMeshRendererComponent>();

        obj->SetPosition(placement.pos);
        obj->Rotate(0.0f, placement.yawDeg, 0.0f);

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
            "Assets/Weapon/Arrow/Mesh/Arrow_Mesh.bin",
            "Assets/Weapon/Arrow/Texture"
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
	// ------------------------------------------------------------------------
// Bullet pool (Static)
// ------------------------------------------------------------------------
	{
		AssetBuildDesc BulletDesc =
		{
			AssetType::Bullet, // 별도 타입이 없으면 일단 임시로 동일 enum 사용
			"Assets/Weapon/Bullet/Mesh/Bullet_Mesh.bin",
			"Assets/Weapon/Bullet/Texture"
		};

		BuiltAsset bulletAsset = AssetManager::BuildAsset(
			dev, cmd,
			m_pMaterials.get(),
			BulletDesc
		);

		m_bulletRefs.clear();
		m_bulletRefs.reserve(kBulletPoolSize);

		for ( UINT k = 0; k < kBulletPoolSize; ++k )
		{
			if ( b->objectRefs.size() >= b->capacity ) break;

			const UINT i = ( UINT ) b->objectRefs.size();

			auto obj = std::make_unique<CGameObject>(1);

			auto* cb = ( CB_GAMEOBJECT_INFO* ) ( ( UINT8* ) b->mappedGameObjects + i * b->cbElementBytes );
			obj->SetMappedGameObjectCB(cb);

			obj->SetMesh(0, bulletAsset.mesh);
			obj->AddComponent<CStaticMeshRendererComponent>();

			auto* bullet = obj->AddComponent<CBulletComponent>();
			( void ) bullet;

			obj->SetPosition(0.0f, -10000.0f, 0.0f);

			obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + ( UINT64 ) i * b->cbvInc);

			obj->CreateComponents(dev, cmd);

			CGameObject* raw = obj.get();
			m_staticObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = ( UINT ) b->objectRefs.size();

			m_bulletRefs.push_back(raw);
		}
	}
	// ------------------------------------------------------------------------
    // Helmet pool (Static attachment)
    // ------------------------------------------------------------------------
    {
        AssetBuildDesc HelmetDesc =
        {
            AssetType::Helmet,
            "Assets/Helmet/Mesh/Helmet.bin",
            "Assets/Helmet/Texture"
        };

        BuiltAsset helmetAsset = AssetManager::BuildAsset(
            dev, cmd,
            m_pMaterials.get(),
            HelmetDesc
        );

        m_helmetRefs.clear();
        m_helmetRefs.reserve(m_helmetCount);

        for (UINT k = 0; k < m_helmetCount; ++k)
        {
            if (b->objectRefs.size() >= b->capacity) break;

            const UINT i = (UINT)b->objectRefs.size();

            auto obj = std::make_unique<CGameObject>(1);

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
            obj->SetMappedGameObjectCB(cb);

            obj->SetMesh(0, helmetAsset.mesh);
            obj->AddComponent<CStaticMeshRendererComponent>();

            // 링크 전까지는 화면 밖에 둠
            obj->SetPosition(0.0f, -10000.0f, 0.0f);

            obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

            obj->CreateComponents(dev, cmd);

            CGameObject* raw = obj.get();
            m_staticObjects.push_back(std::move(obj));
            b->objectRefs.push_back(raw);
            b->count = (UINT)b->objectRefs.size();

            m_helmetRefs.push_back(raw);
        }
    }
    // ------------------------------------------------------------------------
    // PlayerSword pool
    // ------------------------------------------------------------------------
    {
        AssetBuildDesc SwordDesc =
        {
            AssetType::Sword,
            "Assets/Weapon/SwordP/Mesh/Sword_Mesh.bin",
            "Assets/Weapon/SwordP/Texture"
        };

        BuiltAsset SwordAsset = AssetManager::BuildAsset(
            dev, cmd,
            m_pMaterials.get(),
            SwordDesc
        );

        m_PlayerSwordRefs.clear();
        m_PlayerSwordRefs.reserve(m_PlayerSwordCount);

        for (UINT k = 0; k < m_PlayerSwordCount; ++k)
        {
            if (b->objectRefs.size() >= b->capacity) break;

            const UINT i = (UINT)b->objectRefs.size();

            auto obj = std::make_unique<CGameObject>(1);

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
            obj->SetMappedGameObjectCB(cb);

            obj->SetMesh(0, SwordAsset.mesh);
            obj->AddComponent<CStaticMeshRendererComponent>();

            // 링크 전까지는 화면 밖에 둠
            obj->SetPosition(0.0f, -10000.0f, 0.0f);

            obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

            obj->CreateComponents(dev, cmd);

            CGameObject* raw = obj.get();
            m_staticObjects.push_back(std::move(obj));
            b->objectRefs.push_back(raw);
            b->count = (UINT)b->objectRefs.size();

            m_PlayerSwordRefs.push_back(raw);
        }
    }
    // ------------------------------------------------------------------------
    // PlayerAxe pool
    // ------------------------------------------------------------------------
    {
        AssetBuildDesc AxeDesc =
        {
            AssetType::Axe,
            "Assets/Weapon/Axe/Mesh/Axe_Mesh.bin",
            "Assets/Weapon/Axe/Texture"
        };

        BuiltAsset AxeAsset = AssetManager::BuildAsset(
            dev, cmd,
            m_pMaterials.get(),
            AxeDesc
        );

        m_PlayerAxeRefs.clear();
        m_PlayerAxeRefs.reserve(m_PlayerAxeCount);

        for (UINT k = 0; k < m_PlayerAxeCount; ++k)
        {
            if (b->objectRefs.size() >= b->capacity) break;

            const UINT i = (UINT)b->objectRefs.size();

            auto obj = std::make_unique<CGameObject>(1);

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
            obj->SetMappedGameObjectCB(cb);

            obj->SetMesh(0, AxeAsset.mesh);
            obj->AddComponent<CStaticMeshRendererComponent>();

            // 링크 전까지는 화면 밖에 둠
            obj->SetPosition(0.0f, -10000.0f, 0.0f);

            obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

            obj->CreateComponents(dev, cmd);

            CGameObject* raw = obj.get();
            m_staticObjects.push_back(std::move(obj));
            b->objectRefs.push_back(raw);
            b->count = (UINT)b->objectRefs.size();

            m_PlayerAxeRefs.push_back(raw);
        }
    }
    // ------------------------------------------------------------------------
    // PlayerGun pool
    // ------------------------------------------------------------------------
    {
        AssetBuildDesc GunDesc =
        {
            AssetType::Gun,
            "Assets/Weapon/Gun/Mesh/Gun_Mesh.bin",
            "Assets/Weapon/Gun/Texture"
        };

        BuiltAsset GunAsset = AssetManager::BuildAsset(
            dev, cmd,
            m_pMaterials.get(),
            GunDesc
        );

        m_PlayerGunRefs.clear();
        m_PlayerGunRefs.reserve(m_PlayerGunCount);

        for (UINT k = 0; k < m_PlayerGunCount; ++k)
        {
            if (b->objectRefs.size() >= b->capacity) break;

            const UINT i = (UINT)b->objectRefs.size();

            auto obj = std::make_unique<CGameObject>(1);

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
            obj->SetMappedGameObjectCB(cb);

            obj->SetMesh(0, GunAsset.mesh);
            obj->AddComponent<CStaticMeshRendererComponent>();

            // 링크 전까지는 화면 밖에 둠
            obj->SetPosition(0.0f, -10000.0f, 0.0f);

            obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

            obj->CreateComponents(dev, cmd);

            CGameObject* raw = obj.get();
            m_staticObjects.push_back(std::move(obj));
            b->objectRefs.push_back(raw);
            b->count = (UINT)b->objectRefs.size();

            m_PlayerGunRefs.push_back(raw);
        }
    }
    // ------------------------------------------------------------------------
    // EnemySword pool
    // ------------------------------------------------------------------------
    {
        AssetBuildDesc SwordDesc =
        {
            AssetType::Sword,
            "Assets/Weapon/SwordE/Mesh/Sword_Mesh.bin",
            "Assets/Weapon/SwordE/Texture"
        };

        BuiltAsset swordAsset = AssetManager::BuildAsset(
            dev, cmd,
            m_pMaterials.get(),
            SwordDesc
        );

        m_EnemySwordRefs.clear();
        m_EnemySwordRefs.reserve(m_swordManCount);

        for (UINT k = 0; k < m_swordManCount; ++k)
        {
            if (b->objectRefs.size() >= b->capacity) break;

            const UINT i = (UINT)b->objectRefs.size();

            auto obj = std::make_unique<CGameObject>(1);

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
            obj->SetMappedGameObjectCB(cb);

            obj->SetMesh(0, swordAsset.mesh);
            obj->AddComponent<CStaticMeshRendererComponent>();

            obj->SetPosition(0.0f, -10000.0f, 0.0f);
            obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

            obj->CreateComponents(dev, cmd);

            CGameObject* raw = obj.get();
            m_staticObjects.push_back(std::move(obj));
            b->objectRefs.push_back(raw);
            b->count = (UINT)b->objectRefs.size();

            m_EnemySwordRefs.push_back(raw);
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

    const UINT fighterCount = m_PlayerCount;

    const XMFLOAT3 playerBase(0.0f, 0.0f, 0.0f);

    m_swordManRefs.clear();
    m_swordManRefs.reserve(m_swordManCount);

    m_bowManRefs.clear();
    m_bowManRefs.reserve(m_bowManCount);

    m_MutantRefs.clear();
    m_MutantRefs.reserve(m_MutantCount);

    // ------------------------------------------------------------------------
    // GameStartData에서 초기 좌표 추출
    // ------------------------------------------------------------------------
    GameStartData gameStartData{};
    if (std::holds_alternative<GameStartData>(m_pendingNetworkMessage.data))
    {
        gameStartData = std::get<GameStartData>(m_pendingNetworkMessage.data);
    }

    // enemy 인덱스 카운터 (모든 적 타입에 걸쳐 순차 증가)
    UINT enemyIndex = 0;

    // ------------------------------------------------------------------------
    // Enemies
    // ------------------------------------------------------------------------
    {
        const XMFLOAT3 enemyBase = XMFLOAT3(playerBase.x, playerBase.y, playerBase.z + 2.0f);

        // ----------------------------
        // Enemy Type: Ghoul
        // ----------------------------
        {
            const UINT countW = m_ghoulCount;

            AssetBuildDesc EnemyWDesc =
            {
                AssetType::Ghoul,
                "Assets/Ghoul/Mesh/Ghoul_Mesh.bin",
                "Assets/Ghoul/Texture"
            };

            BuiltAsset assetW = AssetManager::BuildAsset(dev, cmd, m_pMaterials.get(), EnemyWDesc);

            for (UINT k = 0; k < countW; ++k)
            {
                if (b->objectRefs.size() >= b->capacity) break;

                const UINT i = (UINT)b->objectRefs.size();

                auto obj = std::make_unique<CGameObject>(1);

                auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
                obj->SetMappedGameObjectCB(cb);

                obj->SetMesh(0, assetW.mesh);
                obj->AddComponent<CSkinnedMeshRendererComponent>();
                obj->AddComponent<CColliderComponent>(EColliderType::BCapsule);
				auto* animComp = obj->AddComponent<CAnimatorComponent>();

                {
                    auto* tag = obj->AddComponent<CActorTagComponent>();
                    tag->kind = EActorKind::NPC;
                    tag->control = EPlayerControl::None;
                    tag->playerSlot = -1;
                }

                // GameStartData에서 좌표 가져오기
                XMFLOAT3 pos;
                float yaw = 180.0f;
                if (enemyIndex < (UINT)gameStartData.enemies.size())
                {
                    const auto& state = gameStartData.enemies[enemyIndex];
                    pos = state.position;
                    yaw = state.yaw;
                }
                else
                {
                    // fallback: 기존 하드코딩 좌표
                    pos.x = enemyBase.x + 2.0f * (float)k;
                    pos.y = enemyBase.y;
                    pos.z = enemyBase.z + 0.0f;
                }
                obj->SetPosition(pos.x, pos.y, pos.z);
                //obj->Rotate(-90.0f, yaw, 0.0f); // Ghoul은 -90도 보정 필요

                ++enemyIndex;

                obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

                if (assetW.mesh && assetW.mesh->IsSkinnedMesh())
                    obj->EnableSkinning(dev, assetW.mesh->GetBoneCount());

				auto mesh0 = obj->GetMeshShared(0);
				if ( mesh0 && animComp )
				{
					auto LoadAndAddClip = [ & ] (const char* filePath, const char* clipName)
						{
							AnimationClip clip{};
							if ( mesh0->LoadAnimationFromBIN(filePath, clipName, clip, 1.0f) )
							{
								clip.name = clipName;
								animComp->AddClip(clip);
							}
						};

					LoadAndAddClip("Assets/Ghoul/Animation/Ghoul_Anim_Idle.bin", "Idle");
					LoadAndAddClip("Assets/Ghoul/Animation/Ghoul_Anim_Walk.bin", "Walk");
					LoadAndAddClip("Assets/Ghoul/Animation/Ghoul_Anim_Run.bin", "Run");
					LoadAndAddClip("Assets/Ghoul/Animation/Ghoul_Anim_Hit.bin", "Hit");
					LoadAndAddClip("Assets/Ghoul/Animation/Ghoul_Anim_Attack.bin", "Attack");
					LoadAndAddClip("Assets/Ghoul/Animation/Ghoul_Anim_Death.bin", "Death");
				}

				if ( animComp )
				{
					auto* ctrl = animComp->EnsureMonsterController();
					if ( ctrl )
					{
						MonsterAnimProfile p{};
						p.idleClip = "Idle";
						p.moveClip = "Walk";
						p.hitClip = "Hit";
						p.attackClip = "Attack";
						p.deathClip = "Death";

						ctrl->SetProfile(p);
						ctrl->SetLocomotionState(EMonsterAnimState::Idle);
						ctrl->Update(0.0f);
					}
				}

				obj->Animate(0.0f);

				obj->CreateComponents(dev, cmd);
				if ( animComp ) animComp->EvaluatePose(0.0f);

                CGameObject* raw = obj.get();
                m_skinnedObjects.push_back(std::move(obj));
                b->objectRefs.push_back(raw);
                b->count = (UINT)b->objectRefs.size();
            }
        }

        // ----------------------------
        // Enemy Type: SwordMan
        // ----------------------------
        {
            const UINT countX = m_swordManCount;

            AssetBuildDesc EnemyXDesc =
            {
                AssetType::SwordMan,
                "Assets/Enemy/Mesh/Enemy_Mesh1.bin",
                "Assets/Enemy/Texture"
            };

            BuiltAsset assetX = AssetManager::BuildAsset(dev, cmd, m_pMaterials.get(), EnemyXDesc);

            for (UINT k = 0; k < countX; ++k)
            {
                if (b->objectRefs.size() >= b->capacity) break;

                const UINT i = (UINT)b->objectRefs.size();

                auto obj = std::make_unique<CGameObject>(1);

                auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
                obj->SetMappedGameObjectCB(cb);

				obj->SetMesh(0, assetX.mesh);
				obj->AddComponent<CSkinnedMeshRendererComponent>();
				obj->AddComponent<CColliderComponent>(EColliderType::BCapsule);
				auto* animComp = obj->AddComponent<CAnimatorComponent>();

                {
                    auto* tag = obj->AddComponent<CActorTagComponent>();
                    tag->kind = EActorKind::NPC;
                    tag->control = EPlayerControl::None;
                    tag->playerSlot = -1;
                }

                // GameStartData에서 좌표 가져오기
                XMFLOAT3 pos;
                float yaw = 180.0f;
                if (enemyIndex < (UINT)gameStartData.enemies.size())
                {
                    const auto& state = gameStartData.enemies[enemyIndex];
                    pos = state.position;
                    yaw = state.yaw;
                }
                else
                {
                    pos.x = enemyBase.x + 2.0f * (float)k;
                    pos.y = enemyBase.y;
                    pos.z = enemyBase.z + 3.0f;
                }
                obj->SetPosition(pos.x, pos.y, pos.z);
                obj->Rotate(0.0f, yaw, 0.0f);

                ++enemyIndex;

                obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

                if (assetX.mesh && assetX.mesh->IsSkinnedMesh())
                    obj->EnableSkinning(dev, assetX.mesh->GetBoneCount());

				auto mesh0 = obj->GetMeshShared(0);
				if ( mesh0 && animComp )
				{
					auto LoadAndAddClip = [ & ] (const char* filePath, const char* clipName)
						{
							AnimationClip clip{};
							if ( mesh0->LoadAnimationFromBIN(filePath, clipName, clip, 1.0f) )
							{
								clip.name = clipName;
								animComp->AddClip(clip);
							}
						};

					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Normal_Idle.bin", "Idle_Common");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Sword_Idle.bin", "Idle");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Walk_F.bin", "Walk");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Run_F.bin", "Run");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Normal_Hit.bin", "Hit_Common");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Sword_Hit.bin", "Hit");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Sword_Attack.bin", "Attack");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Death.bin", "Death");
				}

				if ( animComp )
				{
					auto* ctrl = animComp->EnsureMonsterController();
					if ( ctrl )
					{
						MonsterAnimProfile p{};
						p.idleClip = "Idle";
						p.moveClip = "Walk";
						p.hitClip = "Hit";
						p.attackClip = "Attack";
						p.deathClip = "Death";

						ctrl->SetProfile(p);
						ctrl->SetLocomotionState(EMonsterAnimState::Idle);
						ctrl->Update(0.0f);
					}
				}

				obj->CreateComponents(dev, cmd);
				if ( animComp ) animComp->EvaluatePose(0.0f);

                CGameObject* raw = obj.get();
                m_skinnedObjects.push_back(std::move(obj));
                b->objectRefs.push_back(raw);
                b->count = (UINT)b->objectRefs.size();

                m_swordManRefs.push_back(raw);
            }
        }

        // ----------------------------
        // Enemy Type BowMan
        // ----------------------------
        {
            const UINT countY = m_bowManCount;

            AssetBuildDesc EnemyYDesc =
            {
                AssetType::BowMan,
                "Assets/Enemy/Mesh/Enemy_Mesh2.bin",
                "Assets/Enemy/Texture"
            };

            BuiltAsset assetY = AssetManager::BuildAsset(dev, cmd, m_pMaterials.get(), EnemyYDesc);

            for (UINT k = 0; k < countY; ++k)
            {
                if (b->objectRefs.size() >= b->capacity) break;

                const UINT i = (UINT)b->objectRefs.size();

                auto obj = std::make_unique<CGameObject>(1);

                auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
                obj->SetMappedGameObjectCB(cb);

				obj->SetMesh(0, assetY.mesh);
				obj->AddComponent<CSkinnedMeshRendererComponent>();
				obj->AddComponent<CColliderComponent>(EColliderType::BCapsule);
				auto* animComp = obj->AddComponent<CAnimatorComponent>();

                {
                    auto* tag = obj->AddComponent<CActorTagComponent>();
                    tag->kind = EActorKind::NPC;
                    tag->control = EPlayerControl::None;
                    tag->playerSlot = -1;
                }

                // GameStartData에서 좌표 가져오기
                XMFLOAT3 pos;
                float yaw = 180.0f;
                if (enemyIndex < (UINT)gameStartData.enemies.size())
                {
                    const auto& state = gameStartData.enemies[enemyIndex];
                    pos = state.position;
                    yaw = state.yaw;
                }
                else
                {
                    pos.x = enemyBase.x + 2.0f * (float)k;
                    pos.y = enemyBase.y;
                    pos.z = enemyBase.z + 6.0f;
                }
                obj->SetPosition(pos.x, pos.y, pos.z);
                obj->Rotate(0.0f, yaw, 0.0f);

                ++enemyIndex;

                obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

                if (assetY.mesh && assetY.mesh->IsSkinnedMesh())
                    obj->EnableSkinning(dev, assetY.mesh->GetBoneCount());

				auto mesh0 = obj->GetMeshShared(0);
				if ( mesh0 && animComp )
				{
					auto LoadAndAddClip = [ & ] (const char* filePath, const char* clipName)
						{
							AnimationClip clip{};
							if ( mesh0->LoadAnimationFromBIN(filePath, clipName, clip, 1.0f) )
							{
								clip.name = clipName;
								animComp->AddClip(clip);
							}
						};

					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Normal_Idle.bin", "Idle_Common");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Bow_Idle.bin", "Idle");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Walk_F.bin", "Walk");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Run_F.bin", "Run");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Normal_Hit.bin", "Hit");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Bow_Hold.bin", "Bow_Hold");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Bow_Load.bin", "Bow_Load");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Bow_Release.bin", "Bow_Release");
					LoadAndAddClip("Assets/Enemy/Animation/Enemy_Death.bin", "Death");
				}

				if ( animComp )
				{
					auto* ctrl = animComp->EnsureMonsterController();
					if ( ctrl )
					{
						MonsterAnimProfile p{};
						p.idleClip = "Idle";
						p.moveClip = "Walk";
						p.hitClip = "Hit";
						p.deathClip = "Death";

						p.attackClip = "Bow_Load";
						p.attackNextClip = "Bow_Release";
						p.attackHasChain = true;

						ctrl->SetProfile(p);
						ctrl->SetLocomotionState(EMonsterAnimState::Idle);
						ctrl->Update(0.0f);
					}
				}

				obj->CreateComponents(dev, cmd);
				if ( animComp ) animComp->EvaluatePose(0.0f);

                CGameObject* raw = obj.get();
                m_skinnedObjects.push_back(std::move(obj));
                b->objectRefs.push_back(raw);
                b->count = (UINT)b->objectRefs.size();

                m_bowManRefs.push_back(raw);
            }
        }

        // ----------------------------
        // Enemy Type: Mutant
        // ----------------------------
        {
            const UINT countZ = m_MutantCount;

            AssetBuildDesc EnemyZDesc =
            {
                AssetType::Mutant,
                "Assets/Mutant/Mesh/Mutant_Mesh.bin",
                "Assets/Mutant/Texture"
            };

            BuiltAsset assetZ = AssetManager::BuildAsset(dev, cmd, m_pMaterials.get(), EnemyZDesc);

            for (UINT k = 0; k < countZ; ++k)
            {
                if (b->objectRefs.size() >= b->capacity) break;

                const UINT i = (UINT)b->objectRefs.size();

                auto obj = std::make_unique<CGameObject>(1);

                auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
                obj->SetMappedGameObjectCB(cb);

				obj->SetMesh(0, assetZ.mesh);
				obj->AddComponent<CSkinnedMeshRendererComponent>();
				obj->AddComponent<CColliderComponent>(EColliderType::BCapsule);
				auto* animComp = obj->AddComponent<CAnimatorComponent>();

                {
                    auto* tag = obj->AddComponent<CActorTagComponent>();
                    tag->kind = EActorKind::NPC;
                    tag->control = EPlayerControl::None;
                    tag->playerSlot = -1;
                }

                // GameStartData에서 좌표 가져오기
                XMFLOAT3 pos;
                float yaw = 180.0f;
                if (enemyIndex < (UINT)gameStartData.enemies.size())
                {
                    const auto& state = gameStartData.enemies[enemyIndex];
                    pos = state.position;
                    yaw = state.yaw;
                }
                else
                {
                    pos.x = enemyBase.x + 2.0f * (float)k;
                    pos.y = enemyBase.y;
                    pos.z = enemyBase.z + 9.0f;
                }
                obj->SetPosition(pos.x, pos.y, pos.z);
                obj->Rotate(0.0f, yaw, 0.0f);

                ++enemyIndex;

                obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

                if (assetZ.mesh && assetZ.mesh->IsSkinnedMesh())
                    obj->EnableSkinning(dev, assetZ.mesh->GetBoneCount());

				auto mesh0 = obj->GetMeshShared(0);
				if ( mesh0 && animComp )
				{
					auto LoadAndAddClip = [ & ] (const char* filePath, const char* clipName)
						{
							AnimationClip clip{};
							if ( mesh0->LoadAnimationFromBIN(filePath, clipName, clip, 1.0f) )
							{
								clip.name = clipName;
								animComp->AddClip(clip);
							}
						};

					LoadAndAddClip("Assets/Mutant/Animation/Mutant_Anim_Idle.bin", "Idle");
					LoadAndAddClip("Assets/Mutant/Animation/Mutant_Anim_Walk.bin", "Walk");
					LoadAndAddClip("Assets/Mutant/Animation/Mutant_Anim_Run.bin", "Run");
					LoadAndAddClip("Assets/Mutant/Animation/Mutant_Anim_Hit.bin", "Hit");
					LoadAndAddClip("Assets/Mutant/Animation/Mutant_Anim_Attack.bin", "Attack");
					LoadAndAddClip("Assets/Mutant/Animation/Mutant_Anim_Dead.bin", "Death");
				}

				if ( animComp )
				{
					auto* ctrl = animComp->EnsureMonsterController();
					if ( ctrl )
					{
						MonsterAnimProfile p{};
						p.idleClip = "Idle";
						p.moveClip = "Walk";
						p.hitClip = "Hit";
						p.attackClip = "Attack";
						p.deathClip = "Death";

						ctrl->SetProfile(p);
						ctrl->SetLocomotionState(EMonsterAnimState::Idle);
						ctrl->Update(0.0f);
					}
				}

				obj->CreateComponents(dev, cmd);
				if ( animComp ) animComp->EvaluatePose(0.0f);

                CGameObject* raw = obj.get();
                m_skinnedObjects.push_back(std::move(obj));
                b->objectRefs.push_back(raw);
                b->count = (UINT)b->objectRefs.size();

                m_MutantRefs.push_back(raw);
            }
        }

        // ----------------------------
        // Enemy Type: Boss
        // ----------------------------
        {
            const UINT countOne = m_bossCount;

            AssetBuildDesc EnemyOneDesc =
            {
                AssetType::Boss,
                "Assets/Boss/Mesh/Boss_Mesh.bin",
                "Assets/Boss/Texture"
            };

            BuiltAsset assetOne = AssetManager::BuildAsset(dev, cmd, m_pMaterials.get(), EnemyOneDesc);

            for (UINT k = 0; k < countOne; ++k)
            {
                if (b->objectRefs.size() >= b->capacity) break;

                const UINT i = (UINT)b->objectRefs.size();

                auto obj = std::make_unique<CGameObject>(1);

                auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
                obj->SetMappedGameObjectCB(cb);

				obj->SetMesh(0, assetOne.mesh);
				obj->AddComponent<CSkinnedMeshRendererComponent>();
				obj->AddComponent<CColliderComponent>(EColliderType::BCapsule);
				auto* animComp = obj->AddComponent<CAnimatorComponent>();

                {
                    auto* tag = obj->AddComponent<CActorTagComponent>();
                    tag->kind = EActorKind::NPC;
                    tag->control = EPlayerControl::None;
                    tag->playerSlot = -1;
                }

                // GameStartData에서 좌표 가져오기
                XMFLOAT3 pos;
                float yaw = 180.0f;
                if (enemyIndex < (UINT)gameStartData.enemies.size())
                {
                    const auto& state = gameStartData.enemies[enemyIndex];
                    pos = state.position;
                    yaw = state.yaw;
                }
                else
                {
                    pos.x = enemyBase.x + 0.0f;
                    pos.y = enemyBase.y;
                    pos.z = enemyBase.z + 12.0f;
                }
                obj->SetPosition(pos.x, pos.y, pos.z);
                obj->Rotate(0.0f, yaw, 0.0f);

                ++enemyIndex;

                obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

                if (assetOne.mesh && assetOne.mesh->IsSkinnedMesh())
                    obj->EnableSkinning(dev, assetOne.mesh->GetBoneCount());

				auto mesh0 = obj->GetMeshShared(0);
				if ( mesh0 && animComp )
				{
					auto LoadAndAddClip = [ & ] (const char* filePath, const char* clipName)
						{
							AnimationClip clip{};
							if ( mesh0->LoadAnimationFromBIN(filePath, clipName, clip, 1.0f) )
							{
								clip.name = clipName;
								animComp->AddClip(clip);
							}
						};

					LoadAndAddClip("Assets/Boss/Animation/Boss_Anim_Appear.bin", "Appear");
					LoadAndAddClip("Assets/Boss/Animation/Boss_Anim_AttackLeft.bin", "AttackLeft");
					LoadAndAddClip("Assets/Boss/Animation/Boss_Anim_AttackRight.bin", "AttackRight");
					LoadAndAddClip("Assets/Boss/Animation/Boss_Anim_Call.bin", "Call");
					LoadAndAddClip("Assets/Boss/Animation/Boss_Anim_Death.bin", "Death");
					LoadAndAddClip("Assets/Boss/Animation/Boss_Anim_Hit.bin", "Hit");
					LoadAndAddClip("Assets/Boss/Animation/Boss_Anim_Idle.bin", "Idle");
					LoadAndAddClip("Assets/Boss/Animation/Boss_Anim_Spell.bin", "Spell");
					LoadAndAddClip("Assets/Boss/Animation/Boss_Anim_Walk.bin", "Walk");
				}

				if ( animComp )
				{
					auto* ctrl = animComp->EnsureMonsterController();
					if ( ctrl )
					{
						MonsterAnimProfile p{};
						p.idleClip = "Idle";
						p.moveClip = "Walk";
						p.hitClip = "Hit";
						p.deathClip = "Death";

						p.attackClip = "AttackLeft";
						p.appearClip = "Appear";
						p.callClip = "Call";
						p.spellClip = "Spell";

						ctrl->SetProfile(p);
						ctrl->SetLocomotionState(EMonsterAnimState::Idle);
						ctrl->Update(0.0f);
					}
				}

				obj->CreateComponents(dev, cmd);
				if ( animComp ) animComp->EvaluatePose(0.0f);

                CGameObject* raw = obj.get();
                m_skinnedObjects.push_back(std::move(obj));
                b->objectRefs.push_back(raw);
                b->count = (UINT)b->objectRefs.size();
            }
        }
    }

    // ------------------------------------------------------------------------
    // Player (Players slot 0..3)
    // ------------------------------------------------------------------------
    {
        AssetBuildDesc FighterDesc0 =
        {
            AssetType::Player,
            "Assets/Player/Mesh/Player_Mesh1.bin",
            "Assets/Player/Texture"
        };
        AssetBuildDesc FighterDesc1 =
        {
            AssetType::Player,
            "Assets/Player/Mesh/Player_Mesh2.bin",
            "Assets/Player/Texture"
        };
        AssetBuildDesc FighterDesc2 =
        {
            AssetType::Player,
            "Assets/Player/Mesh/Player_Mesh3.bin",
            "Assets/Player/Texture"
        };
        AssetBuildDesc FighterDesc3 =
        {
            AssetType::Player,
            "Assets/Player/Mesh/Player_Mesh4.bin",
            "Assets/Player/Texture"
        };

        for (UINT k = 0; k < fighterCount; ++k)
        {
            if (b->objectRefs.size() >= b->capacity) break;

            const UINT i = (UINT)b->objectRefs.size();

            const int slot = (int)k;
            const bool isLocal = (slot == m_localPlayerSlot);

            BuiltAsset asset{};
            if (slot == 0) asset = AssetManager::BuildAsset(dev, cmd, m_pMaterials.get(), FighterDesc0);
            else if (slot == 1) asset = AssetManager::BuildAsset(dev, cmd, m_pMaterials.get(), FighterDesc1);
            else if (slot == 2) asset = AssetManager::BuildAsset(dev, cmd, m_pMaterials.get(), FighterDesc2);
            else /*slot==3*/ asset = AssetManager::BuildAsset(dev, cmd, m_pMaterials.get(), FighterDesc3);

            auto obj = std::make_unique<CGameObject>(1);

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
            obj->SetMappedGameObjectCB(cb);

            obj->SetMesh(0, asset.mesh);
            obj->AddComponent<CSkinnedMeshRendererComponent>();
            obj->AddComponent<CColliderComponent>(EColliderType::BCapsule);

            auto* animComp = obj->AddComponent<CAnimatorComponent>();
            auto* equipComp = obj->AddComponent<CPlayerEquipmentComponent>();

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

            // GameStartData에서 플레이어 좌표 가져오기
            XMFLOAT3 pos;
            float yaw = 0.0f;
            if (k < (UINT)gameStartData.players.size())
            {
                const auto& state = gameStartData.players[k];
                pos = state.position;
                yaw = state.yaw;

				// 플레이어별 초기 장비 세팅
                equipComp->SetLoadout(state.weaponType);
            }
            else
            {
                // fallback: 기존 하드코딩 좌표
                pos.x = playerBase.x + 2.0f * (float)slot;
                pos.y = playerBase.y;
                pos.z = playerBase.z;
            }
            obj->SetPosition(pos.x, pos.y, pos.z);
            obj->Rotate(0.0f, yaw, 0.0f);

            obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

            if (asset.mesh && asset.mesh->IsSkinnedMesh())
            {
                obj->EnableSkinning(dev, asset.mesh->GetBoneCount());
            }

            auto mesh0 = obj->GetMeshShared(0);

            bool hasIdleNormal = false;
            bool hasRunF = false;
            bool hasAttackSword = false;

            if (mesh0 && animComp)
            {
                auto LoadAndAddClip =
                    [&](const char* filePath, const char* clipName, bool* loadedFlag = nullptr)
                    {
                        AnimationClip clip{};
                        bool loaded = mesh0->LoadAnimationFromBIN(
                            filePath,
                            clipName,
                            clip,
                            1.0f
                        );

                        if (loaded)
                        {
                            clip.name = clipName;
                            animComp->AddClip(clip);
                        }

                        if (loadedFlag) *loadedFlag = loaded;
                        return loaded;
                    };

                // --------------------------------------------------------------------
                // Idle / Hit / Death
                // --------------------------------------------------------------------
                LoadAndAddClip("Assets/Player/Animation/Player_Normal_Idle.bin", "Idle_Normal", &hasIdleNormal);
                LoadAndAddClip("Assets/Player/Animation/Player_Sword_Idle.bin", "Idle_Sword");
                LoadAndAddClip("Assets/Player/Animation/Player_Axe_Idle.bin", "Idle_Axe");
                LoadAndAddClip("Assets/Player/Animation/Player_Bow_Idle.bin", "Idle_Bow");
                LoadAndAddClip("Assets/Player/Animation/Player_Gun_Idle.bin", "Idle_Gun");

                LoadAndAddClip("Assets/Player/Animation/Player_Normal_Hit.bin", "Hit_Normal");
                LoadAndAddClip("Assets/Player/Animation/Player_Sword_Hit.bin", "Hit_Sword");
                LoadAndAddClip("Assets/Player/Animation/Player_Death.bin", "Death");

                // --------------------------------------------------------------------
                // Attack / Action
                // --------------------------------------------------------------------
                LoadAndAddClip("Assets/Player/Animation/Player_Sword_Attack.bin", "Attack_Sword", &hasAttackSword);
                LoadAndAddClip("Assets/Player/Animation/Player_Axe_Attack.bin", "Attack_Axe");
                LoadAndAddClip("Assets/Player/Animation/Player_Bow_Load.bin", "Bow_Load");
                LoadAndAddClip("Assets/Player/Animation/Player_Bow_Release.bin", "Bow_Release");
                LoadAndAddClip("Assets/Player/Animation/Player_Gun_Shoot.bin", "Gun_Shoot");
                LoadAndAddClip("Assets/Player/Animation/Player_Roll_F.bin", "Roll_F");
                LoadAndAddClip("Assets/Player/Animation/Player_Roll_B.bin", "Roll_B");
                // --------------------------------------------------------------------
                // Walk
                // --------------------------------------------------------------------
                LoadAndAddClip("Assets/Player/Animation/Player_Walk_F.bin", "Walk_F");
                LoadAndAddClip("Assets/Player/Animation/Player_Walk_B.bin", "Walk_B");
                LoadAndAddClip("Assets/Player/Animation/Player_Walk_L.bin", "Walk_L");
                LoadAndAddClip("Assets/Player/Animation/Player_Walk_R.bin", "Walk_R");
                LoadAndAddClip("Assets/Player/Animation/Player_Walk_FL.bin", "Walk_FL");
                LoadAndAddClip("Assets/Player/Animation/Player_Walk_FR.bin", "Walk_FR");
                LoadAndAddClip("Assets/Player/Animation/Player_Walk_BL.bin", "Walk_BL");
                LoadAndAddClip("Assets/Player/Animation/Player_Walk_BR.bin", "Walk_BR");

                // --------------------------------------------------------------------
                // Run
                // --------------------------------------------------------------------
                LoadAndAddClip("Assets/Player/Animation/Player_Run_F.bin", "Run_F", &hasRunF);
                LoadAndAddClip("Assets/Player/Animation/Player_Run_B.bin", "Run_B");
                LoadAndAddClip("Assets/Player/Animation/Player_Run_L.bin", "Run_L");
                LoadAndAddClip("Assets/Player/Animation/Player_Run_R.bin", "Run_R");
                LoadAndAddClip("Assets/Player/Animation/Player_Run_FL.bin", "Run_FL");
                LoadAndAddClip("Assets/Player/Animation/Player_Run_FR.bin", "Run_FR");
                LoadAndAddClip("Assets/Player/Animation/Player_Run_BL.bin", "Run_BL");
                LoadAndAddClip("Assets/Player/Animation/Player_Run_BR.bin", "Run_BR");
            }

            if (animComp)
            {
                auto* ctrl = animComp->EnsureController();
                if (ctrl)
                {
                    ctrl->EnablePlayerClipSet(true);

                    // fallback 값
                    ctrl->SetIdleClip("Idle_Normal");
                    ctrl->SetMoveClip("Walk_F");
                    ctrl->SetHitClip("Hit_Normal");
                    ctrl->SetAttackClip("Attack_Sword");

                    ctrl->SetSpeed(0.0f);
                    ctrl->SetMoveDirection(0);
                    ctrl->SetRunRequested(false);
                    ctrl->SetRollMoveSpeed(5.0f);
                    ctrl->SetRollMoveWindow(0.08f, 0.55f);
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

    // ------------------------------------------------------------------------
    // PlayerBow pool (Skinned attachment)
    // ------------------------------------------------------------------------
    {
        AssetBuildDesc BowDesc =
        {
            AssetType::Bow,
            "Assets/Weapon/BowP/Mesh/Bow_Mesh.bin",
            "Assets/Weapon/BowP/Texture"
        };

        BuiltAsset bowAsset = AssetManager::BuildAsset(
            dev, cmd,
            m_pMaterials.get(),
            BowDesc
        );

        m_PlayerBowRefs.clear();
        m_PlayerBowRefs.reserve(m_PlayerBowCount);

        for (UINT k = 0; k < m_PlayerBowCount; ++k)
        {
            if (b->objectRefs.size() >= b->capacity) break;

            const UINT i = (UINT)b->objectRefs.size();

            auto obj = std::make_unique<CGameObject>(1);

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
            obj->SetMappedGameObjectCB(cb);

            obj->SetMesh(0, bowAsset.mesh);
            obj->AddComponent<CSkinnedMeshRendererComponent>();

            auto* animComp = obj->AddComponent<CAnimatorComponent>();

            obj->SetPosition(0.0f, -10000.0f, 0.0f);
            obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

            if (bowAsset.mesh && bowAsset.mesh->IsSkinnedMesh())
            {
                obj->EnableSkinning(dev, bowAsset.mesh->GetBoneCount());
            }

            if (animComp)
            {
                auto mesh0 = obj->GetMeshShared(0);
                if (mesh0)
                {
                    AnimationClip bowClip{};
                    bool bowLoaded = mesh0->LoadAnimationFromBIN(
                        "Assets/Weapon/BowP/Animation/Bow_Anim.bin",
                        "Fire", bowClip, 1.0f
                    );

                    if (bowLoaded)
                    {
                        bowClip.name = "Fire";
                        animComp->AddClip(bowClip);
                    }
                }
            }

            obj->CreateComponents(dev, cmd);
            if (animComp) animComp->EvaluatePose(0.0f);

            CGameObject* raw = obj.get();
            m_skinnedObjects.push_back(std::move(obj));
            b->objectRefs.push_back(raw);
            b->count = (UINT)b->objectRefs.size();

            m_PlayerBowRefs.push_back(raw);
        }
    }

    // ------------------------------------------------------------------------
    // EnemyBow pool (Skinned attachment)
    // ------------------------------------------------------------------------
    {
        AssetBuildDesc BowDesc =
        {
            AssetType::Bow,
            "Assets/Weapon/BowE/Mesh/Bow_Mesh.bin",
            "Assets/Weapon/BowE/Texture"
        };

        BuiltAsset bowAsset = AssetManager::BuildAsset(
            dev, cmd,
            m_pMaterials.get(),
            BowDesc
        );

        m_EnemyBowRefs.clear();
        m_EnemyBowRefs.reserve(m_bowManCount);

        for (UINT k = 0; k < m_bowManCount; ++k)
        {
            if (b->objectRefs.size() >= b->capacity) break;

            const UINT i = (UINT)b->objectRefs.size();

            auto obj = std::make_unique<CGameObject>(1);

            auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
            obj->SetMappedGameObjectCB(cb);

            obj->SetMesh(0, bowAsset.mesh);
            obj->AddComponent<CSkinnedMeshRendererComponent>();

            auto* animComp = obj->AddComponent<CAnimatorComponent>();

            obj->SetPosition(0.0f, -10000.0f, 0.0f);
            obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

            if (bowAsset.mesh && bowAsset.mesh->IsSkinnedMesh())
            {
                obj->EnableSkinning(dev, bowAsset.mesh->GetBoneCount());
            }

            if (animComp)
            {
                auto mesh0 = obj->GetMeshShared(0);
                if (mesh0)
                {
                    AnimationClip bowClip{};
                    bool bowLoaded = mesh0->LoadAnimationFromBIN(
                        "Assets/Weapon/BowE/Animation/Bow_Anim.bin",
                        "Fire", bowClip, 1.0f
                    );

                    if (bowLoaded)
                    {
                        bowClip.name = "Fire";
                        animComp->AddClip(bowClip);
                    }
                }
            }

            obj->CreateComponents(dev, cmd);
            if (animComp) animComp->EvaluatePose(0.0f);

            CGameObject* raw = obj.get();
            m_skinnedObjects.push_back(std::move(obj));
            b->objectRefs.push_back(raw);
            b->count = (UINT)b->objectRefs.size();

            m_EnemyBowRefs.push_back(raw);
        }
    }

	m_preparedBowmanArrows.assign(m_bowManRefs.size(), nullptr);
	m_prevEnemyBowReleasePhase.assign(m_bowManRefs.size(), false);
}

XMFLOAT4X4 CGameScene::BuildAttachmentOffsetMatrix(
    const XMFLOAT3& pos,
    const XMFLOAT3& rotDeg,
    const XMFLOAT3& scale)
{
    XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
    XMMATRIX R = XMMatrixRotationRollPitchYaw(
        XMConvertToRadians(rotDeg.x),
        XMConvertToRadians(rotDeg.y),
        XMConvertToRadians(rotDeg.z)
    );
    XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);

    XMFLOAT4X4 out{};
    XMStoreFloat4x4(&out, S * R * T);
    return out;
}

void CGameScene::LinkSceneObjects()
{
    // ------------------------------------------------------------------------
    // Light follow target
    // ------------------------------------------------------------------------
    if (m_pPlayerSpotFollower)
    {
        CGameObject* local = GetPlayer();
        if (!local) local = GetPlayerBySlot(0);
        if (local) m_pPlayerSpotFollower->SetTarget(local);
    }

    // ------------------------------------------------------------------------
    // Player weapon offsets (authoring LOCAL transform)
    // ------------------------------------------------------------------------
    const XMFLOAT4X4 swordOffset = BuildAttachmentOffsetMatrix(
        XMFLOAT3(0.09635256f, -0.02604572f, -0.008439302f),
        XMFLOAT3(-15.925f, 0.919f, -7.3f),
        XMFLOAT3(1.0f, 1.0f, 1.0f)
    );

    const XMFLOAT4X4 axeOffset = BuildAttachmentOffsetMatrix(
        XMFLOAT3(0.099f, 0.025f, 0.166f),
        XMFLOAT3(-15.925f, 0.919f, 172.7f),
        XMFLOAT3(1.0f, 1.0f, 1.0f)
    );

    const XMFLOAT4X4 bowOffset = BuildAttachmentOffsetMatrix(
        XMFLOAT3(-0.1f, 0.0f, 0.0f),
        XMFLOAT3(0.0f, 180.0f, 0.0f),
        XMFLOAT3(2.0f, 1.0f, 1.0f)
    );

    const XMFLOAT4X4 gunOffset = BuildAttachmentOffsetMatrix(
        XMFLOAT3(-0.00403512f, 0.00763781f, 0.04897089f),
        XMFLOAT3(-2.441f, 75.104f, 80.119f),
        XMFLOAT3(1.0f, 1.0f, 1.0f)
    );

    // ------------------------------------------------------------------------
    // Enemy weapon offsets
    // ------------------------------------------------------------------------
    const XMFLOAT4X4 enemySwordOffset = BuildAttachmentOffsetMatrix(
        XMFLOAT3(
            0.09635256f * 1.5f,
            -0.02604572f * 1.5f,
            -0.008439302f * 1.5f
        ),
        XMFLOAT3(-15.925f, 0.919f, -7.3f),
        XMFLOAT3(1.0f, 1.0f, 1.0f)
    );

    const XMFLOAT4X4 enemyBowOffset = BuildAttachmentOffsetMatrix(
        XMFLOAT3(
            -0.1f * 1.5f,
            0.0f * 1.5f,
            0.0f * 1.5f
        ),
        XMFLOAT3(0.0f, 180.0f, 0.0f),
        XMFLOAT3(2.0f, 1.0f, 1.0f)
    );

    // ------------------------------------------------------------------------
    // Generic attachment list reset
    // ------------------------------------------------------------------------
    m_attachmentBinds.clear();

    {
        const size_t helmetPairCount =
            (m_helmetRefs.size() < m_MutantRefs.size()) ? m_helmetRefs.size() : m_MutantRefs.size();

        const size_t playerWeaponBindCount = static_cast<size_t>(m_PlayerCount) * 4;

		const size_t enemyWeaponBindCount =
			m_EnemySwordRefs.size() +
			m_EnemyBowRefs.size();

        m_attachmentBinds.reserve(helmetPairCount + playerWeaponBindCount + enemyWeaponBindCount);
    }

    auto AddWeaponBind = [this](CGameObject* follower, CGameObject* target, const char* boneName, const XMFLOAT4X4& localOffset)
        {
            if (!follower || !target || !boneName || !boneName[0])
                return;

            AttachmentBindSpec spec{};
            spec.follower = follower;
            spec.target = target;
            spec.boneName = boneName;
            spec.localOffset = localOffset;
            m_attachmentBinds.push_back(spec);
        };

    // ------------------------------------------------------------------------
    // Player weapon refs / test loadout / follow-bone binding
    // - slot0 = Sword
    // - slot1 = Bow
    // - slot2 = Axe
    // - slot3 = Gun
    // ------------------------------------------------------------------------
    for (int slot = 0; slot < static_cast<int>(m_PlayerCount); ++slot)
    {
        CGameObject* player = GetPlayerBySlot(slot);
        if (!player) continue;

        auto* equip = player->GetComponent<CPlayerEquipmentComponent>();
        if (!equip) continue;

        // 1) 장비 컴포넌트에 무기 오브젝트 포인터 등록
        if ((size_t)slot < m_PlayerSwordRefs.size())
            equip->SetWeaponObject(EWeaponType::Sword, m_PlayerSwordRefs[slot]);

        if ((size_t)slot < m_PlayerBowRefs.size())
            equip->SetWeaponObject(EWeaponType::Bow, m_PlayerBowRefs[slot]);

        if ((size_t)slot < m_PlayerAxeRefs.size())
            equip->SetWeaponObject(EWeaponType::Axe, m_PlayerAxeRefs[slot]);

        if ((size_t)slot < m_PlayerGunRefs.size())
            equip->SetWeaponObject(EWeaponType::Gun, m_PlayerGunRefs[slot]);

        // 2) 테스트용 로드아웃 지정
        switch (slot)
        {
        case 0: equip->SetLoadout(EWeaponType::Sword); break;
        case 1: equip->SetLoadout(EWeaponType::Bow);   break;
        case 2: equip->SetLoadout(EWeaponType::Axe);   break;
        case 3: equip->SetLoadout(EWeaponType::Gun);   break;
        default: equip->ClearOwnedWeapons();           break;
        }

        // 3) 플레이어 무기 바인드
        AddWeaponBind(equip->GetWeaponObject(EWeaponType::Sword), player, "hand_r", swordOffset);
        AddWeaponBind(equip->GetWeaponObject(EWeaponType::Bow), player, "hand_l", bowOffset);
        AddWeaponBind(equip->GetWeaponObject(EWeaponType::Axe), player, "hand_r", axeOffset);
        AddWeaponBind(equip->GetWeaponObject(EWeaponType::Gun), player, "hand_r", gunOffset);
    }

    // ------------------------------------------------------------------------
    // Enemy weapon binding
    // - SwordMan : sword only
    // - BowMan   : bow only
    // - Mutant   : axe only
    // ------------------------------------------------------------------------

    // SwordMan <-> EnemySword
    {
        const size_t pairCount =
            (m_swordManRefs.size() < m_EnemySwordRefs.size()) ? m_swordManRefs.size() : m_EnemySwordRefs.size();

        for (size_t i = 0; i < pairCount; ++i)
        {
            AddWeaponBind(m_EnemySwordRefs[i], m_swordManRefs[i], "hand_r", enemySwordOffset);
        }
    }

    // BowMan <-> EnemyBow
    {
        const size_t pairCount =
            (m_bowManRefs.size() < m_EnemyBowRefs.size()) ? m_bowManRefs.size() : m_EnemyBowRefs.size();

        for (size_t i = 0; i < pairCount; ++i)
        {
            AddWeaponBind(m_EnemyBowRefs[i], m_bowManRefs[i], "hand_l", enemyBowOffset);
        }
    }

    // ------------------------------------------------------------------------
    // Mutant Helmet Attachment
    //  - 1:1 매칭: helmet[i] -> Mutant[i]
    //  - bone: CATRigHub002
    // ------------------------------------------------------------------------
    {
        const size_t helmetCount = m_helmetRefs.size();
        const size_t axeCount = m_MutantRefs.size();
        const size_t pairCount = (helmetCount < axeCount) ? helmetCount : axeCount;

        const XMFLOAT4X4 helmetOffset = BuildAttachmentOffsetMatrix(
            XMFLOAT3(3.8f, 0.35f, 0.0f),
            XMFLOAT3(-90.0f, 0.0f, 90.0f),
            XMFLOAT3(1.0f, 1.0f, 1.0f)
        );

        for (size_t i = 0; i < pairCount; ++i)
        {
            AttachmentBindSpec spec{};
            spec.follower = m_helmetRefs[i];
            spec.target = m_MutantRefs[i];
            spec.boneName = "CATRigHub002";
            spec.localOffset = helmetOffset;
            m_attachmentBinds.push_back(spec);
        }
    }

    // ------------------------------------------------------------------------
    // Apply all binds
    // ------------------------------------------------------------------------
    for (AttachmentBindSpec& spec : m_attachmentBinds)
    {
        if (!spec.follower || !spec.target || spec.boneName.empty())
            continue;

        CFollowBoneComponent* follow = spec.follower->GetComponent<CFollowBoneComponent>();
        if (!follow)
            follow = spec.follower->AddComponent<CFollowBoneComponent>();

        follow->Bind(spec.target, spec.boneName, spec.localOffset);
        follow->SnapNow();
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

bool CGameScene::GetPauseOverlayRect(XMFLOAT4& outRect) const
{
    if (!m_inactiveOverlayTex || (m_inactiveOverlaySrvIndex == UINT_MAX))
        return false;

    float drawW = static_cast<float>(m_inactiveOverlayTex->GetTextureWidth(0));
    float drawH = static_cast<float>(m_inactiveOverlayTex->GetTextureHeight(0));

    if (drawW <= 0.0f || drawH <= 0.0f)
    {
        drawW = 512.0f;
        drawH = 512.0f;
    }

    float fitScale = 1.0f;
    const float scaleX = (drawW > 0.0f) ? (static_cast<float>(FRAME_BUFFER_WIDTH) / drawW) : 1.0f;
    const float scaleY = (drawH > 0.0f) ? (static_cast<float>(FRAME_BUFFER_HEIGHT) / drawH) : 1.0f;

    if (scaleX < fitScale) fitScale = scaleX;
    if (scaleY < fitScale) fitScale = scaleY;
    if (fitScale > 1.0f) fitScale = 1.0f;

    drawW *= fitScale;
    drawH *= fitScale;

    outRect = XMFLOAT4(
        FRAME_BUFFER_WIDTH * 0.5f,
        FRAME_BUFFER_HEIGHT * 0.5f,
        drawW,
        drawH
    );
    return true;
}

bool CGameScene::IsPointInPauseOverlay(POINT clientPt) const
{
    XMFLOAT4 rect{};
    if (!GetPauseOverlayRect(rect))
        return false;

    const float left = rect.x - rect.z * 0.5f;
    const float right = rect.x + rect.z * 0.5f;
    const float top = rect.y - rect.w * 0.5f;
    const float bottom = rect.y + rect.w * 0.5f;

    const float px = static_cast<float>(clientPt.x);
    const float py = static_cast<float>(clientPt.y);

    return (px >= left && px <= right && py >= top && py <= bottom);
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
	if ( !obj ) return;

	constexpr float kArrowPullBackDistance = 0.35f;
	constexpr float kBulletSpeed = 10.0f;
	constexpr float kBulletLife = 3.0f;

	bool shouldPrepareArrow = false;
	bool shouldFireBullet = false;

	if ( auto* equip = obj->GetComponent<CPlayerEquipmentComponent>() )
	{
		const EWeaponType weapon = equip->GetEquippedWeapon();
		shouldPrepareArrow = ( weapon == EWeaponType::Bow );
		shouldFireBullet = ( weapon == EWeaponType::Gun );
	}

	if ( auto* animComp = obj->GetComponent<CAnimatorComponent>() )
	{
		if ( auto* ctrl = animComp->EnsureController() )
		{
			const bool accepted = ctrl->RequestAttack();

			if ( accepted && shouldPrepareArrow )
				RequestPrepareArrow(obj, kArrowPullBackDistance);

			if ( accepted && shouldFireBullet )
				RequestFireBullet(obj, kBulletSpeed, kBulletLife);

			return;
		}
	}

	if ( auto* ctrl = obj->GetAnimController() )
	{
		const bool accepted = ctrl->RequestAttack();

		if ( accepted && shouldPrepareArrow )
			RequestPrepareArrow(obj, kArrowPullBackDistance);

		if ( accepted && shouldFireBullet )
			RequestFireBullet(obj, kBulletSpeed, kBulletLife);

		return;
	}
}


int CGameScene::GetPlayerSlotFromObject(const CGameObject* obj) const
{
    if (!obj) return -1;

    auto* tag = obj->GetComponent<CActorTagComponent>();
    if (!tag) return -1;
    if (tag->kind != EActorKind::Player) return -1;
    if (tag->playerSlot < 0 || tag->playerSlot > 3) return -1;

    return tag->playerSlot;
}

int CGameScene::GetBowManIndexFromObject(const CGameObject* obj) const
{
	if ( !obj ) return -1;

	for ( size_t i = 0; i < m_bowManRefs.size(); ++i )
	{
		if ( m_bowManRefs[i] == obj )
			return static_cast< int >(i);
	}

	return -1;
}

void CGameScene::RequestPrepareArrow(CGameObject* shooter, float pullBackDistance)
{
    if (!shooter) return;

    auto* equip = shooter->GetComponent<CPlayerEquipmentComponent>();
    if (!equip) return;
    if (equip->GetEquippedWeapon() != EWeaponType::Bow) return;

    CGameObject* bowObj = equip->GetWeaponObject(EWeaponType::Bow);
    if (!bowObj) return;

    const int slot = GetPlayerSlotFromObject(shooter);
    if (slot < 0 || slot > 3) return;

    // 이미 준비된 화살이 있으면 중복 생성 안 함
    if (m_preparedPlayerArrows[(size_t)slot])
        return;

    for (CGameObject* arrowObj : m_arrowRefs)
    {
        if (!arrowObj) continue;

        auto* arrow = arrowObj->GetComponent<CArrowComponent>();
        if (!arrow) continue;
        if (arrow->IsActive()) continue;

        arrow->Prepare(bowObj, shooter, pullBackDistance);
        m_preparedPlayerArrows[(size_t)slot] = arrowObj;
        return;
    }
}

void CGameScene::RequestPrepareBowmanArrow(CGameObject* bowman, float pullBackDistance)
{
	if ( !bowman ) return;

	const int bowmanIndex = GetBowManIndexFromObject(bowman);
	if ( bowmanIndex < 0 ) return;

	const size_t idx = static_cast< size_t >(bowmanIndex);

	if ( idx >= m_preparedBowmanArrows.size() ) return;
	if ( idx >= m_EnemyBowRefs.size() ) return;

	// 이미 준비된 화살이 있으면 중복 생성 안 함
	if ( m_preparedBowmanArrows[idx] )
		return;

	CGameObject* bowObj = m_EnemyBowRefs[idx];
	if ( !bowObj ) return;

	for ( CGameObject* arrowObj : m_arrowRefs )
	{
		if ( !arrowObj ) continue;

		auto* arrow = arrowObj->GetComponent<CArrowComponent>();
		if ( !arrow ) continue;
		if ( arrow->IsActive() ) continue;

		arrow->Prepare(bowObj, bowman, pullBackDistance);
		m_preparedBowmanArrows[idx] = arrowObj;
		return;
	}
}

void CGameScene::RequestReleasePreparedBowmanArrow(CGameObject* bowman, float speed, float lifeSec)
{
	if ( !bowman ) return;

	const int bowmanIndex = GetBowManIndexFromObject(bowman);
	if ( bowmanIndex < 0 ) return;

	const size_t idx = static_cast< size_t >(bowmanIndex);
	if ( idx >= m_preparedBowmanArrows.size() ) return;

	CGameObject* arrowObj = m_preparedBowmanArrows[idx];
	if ( !arrowObj ) return;

	auto* arrow = arrowObj->GetComponent<CArrowComponent>();
	if ( !arrow || !arrow->IsPrepared() )
	{
		m_preparedBowmanArrows[idx] = nullptr;
		return;
	}

	const XMFLOAT4X4& W = bowman->GetWorldMatrix();
	XMFLOAT3 dir = { W._31, W._32, W._33 };

	XMVECTOR dirV = XMLoadFloat3(&dir);
	const float lenSq = XMVectorGetX(XMVector3LengthSq(dirV));
	if ( lenSq < 1e-8f )
	{
		dir = XMFLOAT3(0.0f, 0.0f, 1.0f);
		dirV = XMLoadFloat3(&dir);
	}

	dirV = XMVector3Normalize(dirV);

	XMFLOAT3 dirN{};
	XMStoreFloat3(&dirN, dirV);

	if ( auto* tr = arrowObj->GetComponent<CTransformComponent>() )
	{
		tr->SetLookDirection(dirN);
	}

	const XMFLOAT3 vel =
	{
		dirN.x * speed,
		dirN.y * speed,
		dirN.z * speed
	};

	arrow->Launch(vel, lifeSec);
	m_preparedBowmanArrows[idx] = nullptr;
}

void CGameScene::RequestReleasePreparedArrow(CGameObject* shooter, float speed, float lifeSec)
{
    if (!shooter) return;

    const int slot = GetPlayerSlotFromObject(shooter);
    if (slot < 0 || slot > 3) return;

    CGameObject* arrowObj = m_preparedPlayerArrows[(size_t)slot];
    if (!arrowObj) return;

    auto* arrow = arrowObj->GetComponent<CArrowComponent>();
    if (!arrow || !arrow->IsPrepared())
    {
        m_preparedPlayerArrows[(size_t)slot] = nullptr;
        return;
    }

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

    if (auto* tr = arrowObj->GetComponent<CTransformComponent>())
    {
        tr->SetLookDirection(dirN);
    }

    const XMFLOAT3 vel =
    {
        dirN.x * speed,
        dirN.y * speed,
        dirN.z * speed
    };

    arrow->Launch(vel, lifeSec);
    m_preparedPlayerArrows[(size_t)slot] = nullptr;
}

void CGameScene::UpdatePreparedBowArrows()
{
	constexpr float kArrowSpeed = 3.0f;
	constexpr float kArrowLife = 6.0f;

	constexpr float kEnemyArrowPullBackDistance = 0.35f * 1.5f;
	constexpr float kEnemyArrowSpeed = 3.0f;
	constexpr float kEnemyArrowLife = 6.0f;

    for (int slot = 0; slot < 4; ++slot)
    {
        CGameObject* player = GetPlayerBySlot(slot);

        bool isBowLoad = false;
        bool isBowRelease = false;
        bool hasBowEquipped = false;

        if (player)
        {
            if (auto* equip = player->GetComponent<CPlayerEquipmentComponent>())
            {
                hasBowEquipped = (equip->GetEquippedWeapon() == EWeaponType::Bow);
            }

            if (auto* animComp = player->GetComponent<CAnimatorComponent>())
            {
                if (auto* ctrl = animComp->EnsureController())
                {
                    isBowLoad = ctrl->IsBowLoadPhase();
                    isBowRelease = ctrl->IsBowReleasePhase();
                }
            }
            else if (auto* ctrl = player->GetAnimController())
            {
                isBowLoad = ctrl->IsBowLoadPhase();
                isBowRelease = ctrl->IsBowReleasePhase();
            }
        }

        // Bow_Release 진입 순간에만 발사
        if (isBowRelease && !m_prevBowReleasePhase[(size_t)slot])
        {
            RequestReleasePreparedArrow(player, kArrowSpeed, kArrowLife);
        }

        // 공격이 끝났거나 장비가 바뀌면 준비 화살 정리
        if ((!hasBowEquipped || (!isBowLoad && !isBowRelease)) && m_preparedPlayerArrows[(size_t)slot])
        {
            if (auto* arrow = m_preparedPlayerArrows[(size_t)slot]->GetComponent<CArrowComponent>())
            {
                arrow->Deactivate();
            }
            m_preparedPlayerArrows[(size_t)slot] = nullptr;
        }

        m_prevBowReleasePhase[(size_t)slot] = isBowRelease;
    }
	for ( size_t i = 0; i < m_bowManRefs.size(); ++i )
	{
		CGameObject* bowman = m_bowManRefs[i];

		bool isBowLoad = false;
		bool isBowRelease = false;

		if ( bowman )
		{
			if ( auto* animComp = bowman->GetComponent<CAnimatorComponent>() )
			{
				if ( auto* ctrl = animComp->EnsureMonsterController() )
				{
					isBowLoad = ctrl->IsAttackPrimaryPhase();   // Bow_Load
					isBowRelease = ctrl->IsAttackChainPhase();  // Bow_Release
				}
			}
		}

		// Bow_Load 상태이면 준비 화살 생성
		if ( isBowLoad )
		{
			if ( i < m_preparedBowmanArrows.size() && m_preparedBowmanArrows[i] == nullptr )
			{
				RequestPrepareBowmanArrow(bowman, kEnemyArrowPullBackDistance);
			}
		}

		// Bow_Release 진입 순간에만 발사
		if ( i < m_prevEnemyBowReleasePhase.size() )
		{
			if ( isBowRelease && !m_prevEnemyBowReleasePhase[i] )
			{
				RequestReleasePreparedBowmanArrow(bowman, kEnemyArrowSpeed, kEnemyArrowLife);
			}
		}

		// 공격이 끝났거나 다른 액션으로 빠지면 준비 화살 정리
		if ( ( !isBowLoad && !isBowRelease ) &&
			i < m_preparedBowmanArrows.size() &&
			m_preparedBowmanArrows[i] )
		{
			if ( auto* arrow = m_preparedBowmanArrows[i]->GetComponent<CArrowComponent>() )
			{
				arrow->Deactivate();
			}
			m_preparedBowmanArrows[i] = nullptr;
		}

		if ( i < m_prevEnemyBowReleasePhase.size() )
		{
			m_prevEnemyBowReleasePhase[i] = isBowRelease;
		}
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

bool CGameScene::OnProcessingKeyboardMessage(HWND /*hWnd*/, UINT msg, WPARAM wParam, LPARAM /*lParam*/)
{
#ifdef USING_NETWORK
	return false;
#else
	if ( msg != WM_KEYDOWN )
		return false;

	switch ( ( int ) wParam )
	{
	case 'Y':
		if ( !m_ghoulCount ) return true;
		if ( !m_skinnedObjects.empty() )
		{
			// Ghoul 테스트: Attack
			TriggerMonsterTestCommand(m_skinnedObjects[0].get(), EMonsterAnimCommand::Attack);
		}
		return true;

	case 'U':
		if ( !m_swordManRefs.empty() )
		{
			// SwordMan 테스트: Attack
			TriggerMonsterTestCommand(m_swordManRefs[0], EMonsterAnimCommand::Attack);
		}
		return true;

	case 'I':
		if ( !m_bowManRefs.empty() )
		{
			// BowMan 테스트: Attack (Bow_Load -> Bow_Release)
			TriggerMonsterTestCommand(m_bowManRefs[0], EMonsterAnimCommand::Attack);
		}
		return true;

	case 'O':
		if ( !m_MutantRefs.empty() )
		{
			// Mutant 테스트: Attack
			TriggerMonsterTestCommand(m_MutantRefs[0], EMonsterAnimCommand::Attack);
		}
		return true;

	case 'P':
	{
		const UINT bossIndex = m_ghoulCount + m_swordManCount + m_bowManCount + m_MutantCount;
		if ( bossIndex < ( UINT ) m_skinnedObjects.size() )
		{
			// Boss 테스트: Spell
			TriggerMonsterTestCommand(m_skinnedObjects[bossIndex].get(), EMonsterAnimCommand::Spell);
		}
		return true;
	}
	}
#endif
	return false;
}

void CGameScene::RequestFireArrow(CGameObject* shooter, float speed, float lifeSec, float yOffset)
{
    if (!shooter) return;

    CGameObject* bowObj = nullptr;

    if (auto* equip = shooter->GetComponent<CPlayerEquipmentComponent>())
    {
        // 장비 컴포넌트가 있는 플레이어라면 Bow 장착시에만 발사 허용
        if (equip->GetEquippedWeapon() != EWeaponType::Bow)
            return;

        bowObj = equip->GetWeaponObject(EWeaponType::Bow);
    }

    // 방향/회전은 기존 로직 유지: shooter의 forward 사용
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

    // 시작 위치는 활 위치 사용, 없으면 기존 fallback
    XMFLOAT3 startPos{};
    if (bowObj)
    {
        startPos = bowObj->GetPosition();
    }
    else
    {
        startPos = shooter->GetPosition();
        startPos.y += yOffset;
    }

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

void CGameScene::RequestFireBullet(CGameObject* shooter, float speed, float lifeSec)
{
	if ( !shooter ) return;

	CGameObject* gunObj = nullptr;

	if ( auto* equip = shooter->GetComponent<CPlayerEquipmentComponent>() )
	{
		if ( equip->GetEquippedWeapon() != EWeaponType::Gun )
			return;

		gunObj = equip->GetWeaponObject(EWeaponType::Gun);
	}

	const XMFLOAT4X4& W = shooter->GetWorldMatrix();
	XMFLOAT3 dir = { W._31, W._32, W._33 };

	XMVECTOR dirV = XMLoadFloat3(&dir);
	const float lenSq = XMVectorGetX(XMVector3LengthSq(dirV));
	if ( lenSq < 1e-8f )
	{
		dir = XMFLOAT3(0.0f, 0.0f, 1.0f);
		dirV = XMLoadFloat3(&dir);
	}

	dirV = XMVector3Normalize(dirV);

	XMFLOAT3 dirN{};
	XMStoreFloat3(&dirN, dirV);

	XMFLOAT3 startPos{};
	if ( gunObj )
	{
		startPos = gunObj->GetPosition();
	}
	else
	{
		startPos = shooter->GetPosition();
	}

	const XMFLOAT3 vel =
	{
		dirN.x * speed,
		dirN.y * speed,
		dirN.z * speed
	};

	for ( CGameObject* bulletObj : m_bulletRefs )
	{
		if ( !bulletObj ) continue;

		auto* bullet = bulletObj->GetComponent<CBulletComponent>();
		if ( !bullet ) continue;
		if ( bullet->IsActive() ) continue;

		if ( auto* tr = bulletObj->GetComponent<CTransformComponent>() )
		{
			tr->SetLookDirection(dirN);
		}

		bullet->Activate(startPos, vel, lifeSec);
		return;
	}
}

bool CGameScene::ProcessInput(UCHAR* /*pKeysBuffer*/)
{
    return false;
}

void CGameScene::AnimateObjects(float dt)
{
    // ------------------------------------------------------------------------
    // FrameSnapshot에서 좌표 업데이트
    // ------------------------------------------------------------------------

#ifdef USING_NETWORK
    DequeueNetworkMessage(NetworkMessageType::FrameState);
    if (std::holds_alternative<FrameSnapshot>(m_pendingNetworkMessage.data))
    {
        const FrameSnapshot& snapshot = std::get<FrameSnapshot>(m_pendingNetworkMessage.data);

        // Player 좌표 업데이트
        for (const auto& state : snapshot.players)
        {
            // id를 slot으로 사용 (0~3)
            int slot = static_cast<int>(state.id);
            CGameObject* player = GetPlayerBySlot(slot);
            if (!player) continue;


            // 로컬 플레이어는 서버 좌표로 덮어쓰지 않음 (선택적)
            // if (slot == m_localPlayerSlot) continue;

            player->SetPosition(state.position.x, state.position.y, state.position.z);

            // yaw 회전 적용
            if (auto* tr = player->GetComponent<CTransformComponent>())
            {
                tr->SetYawDegrees(state.yaw);
            }

            // 데모: animation state 강제 적용
            if (auto ac = player->GetAnimController())
            {
                if (state.animation.animationId == EAnimState::Attack)
                    ac->RequestAttack();

                ac->SetAnimState(state.animation.animationId);


            }

            if (auto wc = player->GetComponent<CPlayerEquipmentComponent>())
            {
                wc->SetLoadout(state.weaponType);
            }

            if (slot == m_localPlayerSlot)
            {
                // 로컬 플레이어로 카메라 동기화
				auto pCamera = GetMainCamera();
                if (pCamera)
                {
                    XMFLOAT3 pos = player->GetPosition();
                    pCamera->Update(pos, dt);
                    pCamera->SetLookAt(pos);
                    pCamera->RegenerateViewMatrix();
                }
            }
        }

        // Enemy 좌표 업데이트
        // skinnedObjects에서 NPC만 순회 (Fighter 제외)
        UINT enemyIndex = 0;
        const UINT totalEnemies = m_ghoulCount + m_swordManCount + m_bowManCount + m_MutantCount + m_bossCount;

        for (UINT j = 0; j < totalEnemies && j < (UINT)m_skinnedObjects.size(); ++j)
        {
            auto* obj = m_skinnedObjects[j].get();
            if (!obj) continue;

            auto* tag = obj->GetComponent<CActorTagComponent>();
            if (!tag || tag->kind != EActorKind::NPC) continue;

            if (enemyIndex < (UINT)snapshot.enemies.size())
            {
                const auto& state = snapshot.enemies[enemyIndex];
                obj->SetPosition(state.position.x, state.position.y, state.position.z);

                if (auto* tr = obj->GetComponent<CTransformComponent>())
                {
                    tr->SetYawDegrees(state.yaw);
                }
            }
            ++enemyIndex;
        }
    }
#endif
   

    // ------------------------------------------------------------------------
    // 기존 애니메이션 로직
    // ------------------------------------------------------------------------
    for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
    {
        if (!m_skinnedObjects[j]) continue;
        m_skinnedObjects[j]->Animate(dt);
    }

    for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
    {
        if (!m_staticObjects[j]) continue;
        m_staticObjects[j]->Animate(dt);
    }
    UpdatePreparedBowArrows();

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

void CGameScene::CollisionObjects()
{
    m_Collision->OnUpdate();
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

    if (m_bInactiveOverlayVisible && m_inactiveOverlayShader && (m_inactiveOverlaySrvIndex != UINT_MAX))
    {
        XMFLOAT4 uiRect{};
        if (!GetPauseOverlayRect(uiRect))
        {
            uiRect = XMFLOAT4(
                FRAME_BUFFER_WIDTH * 0.5f,
                FRAME_BUFFER_HEIGHT * 0.5f,
                512.0f,
                512.0f
            );
        }

        PS_CB_DRAW_OPTIONS opt{};
        opt.m_xmn4DrawOptions = XMINT4('T', 0, 0, 0);
        opt.m_xmu4PostSrvIdx0 = XMUINT4(m_inactiveOverlaySrvIndex, 0, 0, 0);
        opt.m_xmu4PostSrvIdx1 = XMUINT4(0, 0, 0, 0);
        opt.m_xmf4UiRect = uiRect;
        opt.m_xmf4Viewport = XMFLOAT4(
            static_cast<float>(FRAME_BUFFER_WIDTH),
            static_cast<float>(FRAME_BUFFER_HEIGHT),
            1.0f / static_cast<float>(FRAME_BUFFER_WIDTH),
            1.0f / static_cast<float>(FRAME_BUFFER_HEIGHT)
        );

        m_inactiveOverlayShader->Render(cmd, camera, &opt);
    }

    if (m_Collision)
    {
    }
}

void CGameScene::BuildObjectsCollider()
{
    m_Collision = make_unique<CCollisionSystem>();
    for (auto& obj : m_staticObjects)
    {
        m_Collision->RegisterCollider(obj->GetComponent<CColliderComponent>());
    }
    for (auto& obj : m_skinnedObjects)
    {
        m_Collision->RegisterCollider(obj->GetComponent<CColliderComponent>());
    }
}
