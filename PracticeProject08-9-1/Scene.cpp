//-----------------------------------------------------------------------------
// File: Scene.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Scene.h"
#include "AnimatorComponent.h"
#include "AnimController.h"
#include "Material.h"
#include "AssetManager.h"
#include "LightComponent.h"
#include "PlayerControllerComponent.h"
#include "Object.h"
#include "ThreadManager.h"
#include "Service.h"
#include "Session.h"
#include "BufferReader.h"
#include "ServerPacketHandler.h"

std::unique_ptr<CDescriptorHeap> CScene::m_pDescriptorHeap = std::make_unique<CDescriptorHeap>();
;
CScene::CScene()
{
	m_staticBatch.capacity = 4;
	m_staticBatch.count = 0;

	m_skinnedBatch.capacity = 10;
	m_skinnedBatch.count = 0;

	m_demoFighters.fill(nullptr);
}

CScene::~CScene()
{
}

CGameObject* CScene::GetDemoFighter(int index) const
{
	if (index < 0 || index >= (int)m_demoFighters.size()) return nullptr;
	return m_demoFighters[(size_t)index];
}

void CScene::RequestDemoFighterAttack(int index)
{
	CGameObject* obj = GetDemoFighter(index);
	if (!obj) return;

	// 1) 컴포넌트 기반(향후 서버/상태동기화 확장용)
	if (auto* animComp = obj->GetComponent<CAnimatorComponent>())
	{
		if (auto* ctrl = animComp->EnsureController())
			ctrl->RequestAttack();
		return;
	}

	// 2) 레거시 기반(혼용/호환)
	if (auto* ctrl = obj->GetAnimController())
		ctrl->RequestAttack();
}

void CScene::ReleaseObjects()
{
	if (m_pd3dGraphicsRootSignature)
		m_pd3dGraphicsRootSignature.Reset();

	// shaders: shared_ptr reset만(필요하면)
	m_staticBatch.shader.reset();
	m_skinnedBatch.shader.reset();

	// objects clear
	m_staticObjects.clear();
	m_skinnedObjects.clear();
	m_pPlayer.reset();

	m_lightObjects.clear();
	m_pPlayerSpotFollower = nullptr;

	m_demoFighters.fill(nullptr);

	// Main Camera (GameObject + Camera Component)
	if (m_pMainCamera)
	{
		m_pMainCamera->ReleaseShaderVariables();
		m_pMainCamera = nullptr;
	}
	m_pMainCameraObject.reset();


	// Batch ref lists clear
	m_staticBatch.objectRefs.clear();
	m_skinnedBatch.objectRefs.clear();


	// scene-owned CB/리소스 해제
	ReleaseShaderVariables();
}

void CScene::ReleaseUploadBuffers()
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
	if (m_pPlayer)
		m_pPlayer->ReleaseUploadBuffers();


#ifdef _WITH_BATCH_MATERIAL
	if (m_staticBatch.material)  m_staticBatch.material->ReleaseUploadBuffers();
#endif
}



void CScene::ReleaseShaderVariables()
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

	// ---- Skinned batch CBs ----	if (m_skinnedBatch.cbGameObjects)
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

	if (m_pd3dcbPlayerGameObject)
	{
		if (m_pcbMappedPlayerGameObject)
		{
			m_pd3dcbPlayerGameObject->Unmap(0, NULL);
			m_pcbMappedPlayerGameObject = nullptr;
		}
		m_pd3dcbPlayerGameObject.Reset();
	}

}

void CScene::BuildLightsAndMaterials()
{
	// ============================================================
	// Light Objects (Empty GameObjects + Components)
	// ============================================================
	m_lightObjects.clear();
	m_lightObjects.reserve(4);
	m_pPlayerSpotFollower = nullptr;

	// [0] Point Light (기존 lights[0])
	{
		auto obj = std::make_unique<CGameObject>(0); // empty
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

	// [1] Spot Light (플레이어 추적, 기존 lights[1])
	{
		auto obj = std::make_unique<CGameObject>(0); // empty

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
		// 여기서는 target을 바로 못 잡을 수도 있으니 Animate에서 세팅한다.
		m_pPlayerSpotFollower = follow;

		m_lightObjects.push_back(std::move(obj));
	}

	// [2] Directional Light (기존 lights[2])
	{
		auto obj = std::make_unique<CGameObject>(0); // empty
		// 방향만 의미 있음
		if (auto* tr = obj->GetComponent<CTransformComponent>())
			tr->SetLookDirection(XMFLOAT3(1.0f, 0.0f, 0.0f));

		auto* lc = obj->AddComponent<CLightComponent>();
		lc->type = ELightType::Directional;
		lc->ambient = XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f);
		lc->diffuse = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
		lc->specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		m_lightObjects.push_back(std::move(obj));
	}

	// [3] Spot Light (기존 lights[3])
	{
		auto obj = std::make_unique<CGameObject>(0); // empty
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
	{
		m_pMaterials->m_pReflections[i].m_xmn4TextureIndices = XMUINT4(0, 0, 0, 0);
	}

}
void CScene::BuildStaticBatch(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
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


	// PSO
	pStaticShader->CreateShader(
		pd3dDevice,
		m_pd3dGraphicsRootSignature.Get(),
		nRenderTargets,
		rtvFormats,
		dsvFormat
	);

	// ===== CB (capacity 기준) =====
	b->cbElementBytes = ((sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255);

	b->cbGameObjects = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList, nullptr,
		b->cbElementBytes * cap,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	b->cbGameObjects->Map(0, nullptr, (void**)&b->mappedGameObjects);

	// ===== CBV (capacity 기준) =====
	b->baseCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
	b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	m_pDescriptorHeap->CreateConstantBufferViews(
		pd3dDevice,
		cap,
		b->cbGameObjects.Get(),
		b->cbElementBytes
	);

	// ===== scene-owned objects + batch refs =====
	m_staticObjects.clear();
	m_staticObjects.reserve(cap);

	b->objectRefs.clear();
	b->objectRefs.reserve(cap);

	b->count = 0;

	// ---- 실제 생성 ----
	// AssetBuildDesc WorldDesc = {
	//     AssetType::World,
	//     "Assets/World/Mesh/StartWorld.bin",
	//     "Assets/World/Texture"
	// };
	// BuiltAsset worldAsset = AssetManager::BuildAsset(pd3dDevice, pd3dCommandList, m_pMaterials.get(), WorldDesc);

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
			pd3dDevice, pd3dCommandList,
			m_pMaterials.get(),
			Desc
		);

		auto obj = std::make_unique<CGameObject>(1);

		// ===== per-object CB 매핑 =====
		auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
		obj->SetMappedGameObjectCB(cb);

		// ===== 메시/렌더러/트랜스폼/핸들 =====
		obj->SetMesh(0, asset.mesh);
		obj->AddComponent<CStaticMeshRendererComponent>();

		obj->SetPosition(positions[k]);

		obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

		obj->CreateComponents(pd3dDevice, pd3dCommandList);

		CGameObject* raw = obj.get();
		m_staticObjects.push_back(std::move(obj));
		b->objectRefs.push_back(raw);
		b->count = (UINT)b->objectRefs.size();
	}

}

void CScene::BuildSkinnedBatch(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList,
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

	// PSO
	pSkinnedShader->CreateShader(
		pd3dDevice,
		m_pd3dGraphicsRootSignature.Get(),
		nRenderTargets,
		rtvFormats,
		dsvFormat
	);

	// ===== CB (capacity 기준) =====
	b->cbElementBytes = ((sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255);

	b->cbGameObjects = ::CreateBufferResource(
		pd3dDevice, pd3dCommandList, nullptr,
		b->cbElementBytes * cap,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	b->cbGameObjects->Map(0, nullptr, (void**)&b->mappedGameObjects);

	// ===== CBV (capacity 기준) =====
	b->baseCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
	b->cbvInc = ::gnCbvSrvDescriptorIncrementSize;

	m_pDescriptorHeap->CreateConstantBufferViews(
		pd3dDevice,
		cap,
		b->cbGameObjects.Get(),
		b->cbElementBytes
	);

	// ===== scene-owned objects + batch refs =====
	m_skinnedObjects.clear();
	m_skinnedObjects.reserve(cap);

	b->objectRefs.clear();
	b->objectRefs.reserve(cap);

	b->count = 0;

	const UINT fighterCount = 3;
	const UINT zombieCount = 3;

	// "플레이어 기준" 위치 (BuildSkinnedBatch 시점에 player가 없으니 기본 원점 기준)
	const XMFLOAT3 playerBase(0.0f, 0.0f, 0.0f);

	// ---------- Zombie ----------
	{
		AssetBuildDesc ZombieDesc =
		{
			AssetType::Zombie,
			"Assets/Zombie/Mesh/Zombie.bin",
			"Assets/Zombie/Texture"
		};

		BuiltAsset asset = AssetManager::BuildAsset(pd3dDevice, pd3dCommandList, m_pMaterials.get(), ZombieDesc);

		for (UINT k = 0; k < zombieCount; ++k)
		{
			if (b->objectRefs.size() >= b->capacity) break;

			const UINT i = (UINT)b->objectRefs.size();

			auto obj = std::make_unique<CGameObject>(1);

			auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
			obj->SetMappedGameObjectCB(cb);

			obj->SetMesh(0, asset.mesh);
			obj->AddComponent<CSkinnedMeshRendererComponent>();

			const float x = playerBase.x + 2.0f * (float)k;
			const float z = playerBase.z + 2.0f;

			obj->SetPosition(x, playerBase.y, z);

			obj->Rotate(0.0f, 180.0f, 0.0f);

			obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

			if (asset.mesh && asset.mesh->IsSkinnedMesh())
			{
				obj->EnableSkinning(pd3dDevice, asset.mesh->GetBoneCount());
			}

			// 애니메이션 로드/세팅 (원래 코드 그대로)
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

			obj->CreateComponents(pd3dDevice, pd3dCommandList);

			CGameObject* raw = obj.get();
			if ((k - 1) < (UINT)m_demoFighters.size())
				m_demoFighters[k - 1] = raw;

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = (UINT)b->objectRefs.size();

		}
	}

	// ---------- Fighter ----------
	{
		m_demoFighters.fill(nullptr);
		AssetBuildDesc FighterDesc =
		{
			AssetType::Fighter,
			"Assets/Fighter/Mesh/Fighter.bin",
			"Assets/Fighter/Texture"
		};

		BuiltAsset asset = AssetManager::BuildAsset(pd3dDevice, pd3dCommandList, m_pMaterials.get(), FighterDesc);

		// (기존 코드 그대로) Player CB 생성 파트는 그대로 두되,
		// 여기서 생성되는 fighter들은 "일반 skinned object"이므로 기존 로직 유지
		// ---- Player per-object CB + CBV (b2 table 용) ----
		m_playerCbElementBytes = ((sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255);

		m_pd3dcbPlayerGameObject = ::CreateBufferResource(
			pd3dDevice, pd3dCommandList, nullptr,
			m_playerCbElementBytes,
			D3D12_HEAP_TYPE_UPLOAD,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
			nullptr
		);

		m_pd3dcbPlayerGameObject->Map(0, nullptr, (void**)&m_pcbMappedPlayerGameObject);

		m_playerCbvGpu = m_pDescriptorHeap->GetGPUCbvDescriptorNextHandle();
		m_pDescriptorHeap->CreateConstantBufferViews(
			pd3dDevice,
			1,
			m_pd3dcbPlayerGameObject.Get(),
			m_playerCbElementBytes
		);

		for (UINT k = 1; k < fighterCount + 1; ++k)
		{
			if (b->objectRefs.size() >= b->capacity) break;

			const UINT i = (UINT)b->objectRefs.size();

			auto obj = std::make_unique<CGameObject>(1);

			auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)b->mappedGameObjects + i * b->cbElementBytes);
			obj->SetMappedGameObjectCB(cb);

			obj->SetMesh(0, asset.mesh);
			obj->AddComponent<CSkinnedMeshRendererComponent>();

			// (추후 서버/상태기반 전환을 고려) 애니메이션은 컴포넌트로 운용
			auto* animComp = obj->AddComponent<CAnimatorComponent>();

			// matId 추출
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

			const float x = playerBase.x + 2.0f * (float)k;
			obj->SetPosition(x, playerBase.y, playerBase.z);

			obj->Rotate(0.0f, 0.0f, 0.0f);

			obj->SetCbvGPUDescriptorHandlePtr(b->baseCbvGpu.ptr + (UINT64)i * b->cbvInc);

			if (asset.mesh && asset.mesh->IsSkinnedMesh())
			{
				obj->EnableSkinning(pd3dDevice, asset.mesh->GetBoneCount());
			}

			// 애니메이션 로드/세팅 (Idle + Run + Attack) - Player와 동일 구성
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

			obj->CreateComponents(pd3dDevice, pd3dCommandList);
			if (animComp) animComp->EvaluatePose(0.0f);

			CGameObject* raw = obj.get();

			if ((k >= 1) && (k <= 3))
				m_demoFighters[k - 1] = raw;

			m_skinnedObjects.push_back(std::move(obj));
			b->objectRefs.push_back(raw);
			b->count = (UINT)b->objectRefs.size();
		}
	}
}


void CScene::CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* target)
{
	// 1) 빈 오브젝트 생성 (Scene 소유)
	m_pMainCameraObject = std::make_unique<CGameObject>(0);

	// 2) 카메라 컴포넌트 부착
	auto* cam = m_pMainCameraObject->AddComponent<CThirdPersonCamera>();
	m_pMainCamera = cam;

	// 3) 기존과 동일 파라미터 세팅
	cam->SetMode(THIRD_PERSON_CAMERA);
	cam->SetTarget(target);

	cam->SetTimeLag(0.25f);
	cam->SetOffset(XMFLOAT3(0.0f, 1.0f, -2.0f));
	cam->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
	cam->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
	cam->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);

	// 4) 컴포넌트 GPU 리소스 생성 (b1 카메라 CB)
	//    기존 m_pMainCamera->CreateShaderVariables(dev, cmd) 호출을 이 한 줄로 대체
	m_pMainCameraObject->CreateComponents(dev, cmd);

	// 5) 초기 위치/뷰 세팅(기존과 동일)
	if (target)
	{
		XMFLOAT3 pos = target->GetPosition();
		cam->SetPosition(Vector3::Add(pos, cam->GetOffset()));
		cam->Update(pos, 0.0f);
		cam->SetLookAt(pos);
		cam->RegenerateViewMatrix();
	}
}

void CScene::BuildPlayer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	// 1) Fighter 에셋 빌드 (BuildSkinnedBatch의 Fighter와 동일)
	AssetBuildDesc FighterDesc =
	{
		AssetType::Fighter,
		"Assets/Fighter/Mesh/Fighter.bin",
		"Assets/Fighter/Texture"
	};

	BuiltAsset asset = AssetManager::BuildAsset(pd3dDevice, pd3dCommandList, m_pMaterials.get(), FighterDesc);

	// 2) CGameObject 생성
	auto obj = std::make_shared<CGameObject>(1);
	obj->SetMappedGameObjectCB(m_pcbMappedPlayerGameObject);
	obj->SetCbvGPUDescriptorHandlePtr(m_playerCbvGpu.ptr);


	// 3) 메시/렌더러/머티리얼(현재 코드 스타일 그대로)
	obj->SetMesh(0, asset.mesh);
	obj->AddComponent<CStaticMeshRendererComponent>();

	// 4) 트랜스폼 초기값(원하는 값으로 고정)
	obj->SetPosition(0.0f, 0.0f, 0.0f);
	obj->Rotate(0.0f, 0.0f, 0.0f);

	// 5) 스키닝 켜기 (skinned mesh면)
	if (asset.mesh && asset.mesh->IsSkinnedMesh())
	{
		obj->EnableSkinning(pd3dDevice, asset.mesh->GetBoneCount());
	}

	// 6) 애니메이션(Idle) 로드/세팅 (BuildSkinnedBatch Fighter와 동일)
	AnimationClip idleClip;
	bool idleLoaded = false;

	auto mesh0 = obj->GetMeshShared(0);
	if (mesh0)
	{
		idleLoaded = mesh0->LoadAnimationFromBIN(
			"Assets/Fighter/Animation/FighterIdle.bin",
			"Idle", idleClip, 1.0f
		);
	}

	if (idleLoaded)
	{
		idleClip.name = "Idle";

		CAnimator* anim = obj->EnsureAnimator();
		if (anim) anim->AddClip(idleClip);

		// === Move clip 추가 로드 ===
		AnimationClip moveClip;
		bool moveLoaded = false;

		if (mesh0)
		{
			moveLoaded = mesh0->LoadAnimationFromBIN(
				"Assets/Fighter/Animation/FighterRun.bin",
				"Run", moveClip, 1.0f
			);
		}

		if (moveLoaded)
		{
			moveClip.name = "Run";
			if (anim) anim->AddClip(moveClip);
		}

		// === Attack clip 추가 로드 ===
		AnimationClip attackClip;
		bool attackLoaded = false;

		if (mesh0)
		{
			attackLoaded = mesh0->LoadAnimationFromBIN(
				"Assets/Fighter/Animation/FighterAttack.bin",
				"Attack", attackClip, 1.0f
			);
		}

		if (attackLoaded)
		{
			attackClip.name = "Attack";
			if (anim) anim->AddClip(attackClip);
		}


		auto* ctrl = obj->EnsureAnimController();
		ctrl->SetIdleClip("Idle");
		ctrl->SetMoveClip(moveLoaded ? "Run" : "Idle");
		ctrl->SetAttackClip("Attack");

		ctrl->SetSpeed(0.0f);
		ctrl->Update(0.0f);

		obj->Animate(0.0f);
	}


	// 7) 플레이어 컨트롤러 컴포넌트 부착
	obj->AddComponent<CPlayerControllerComponent>();

	// 8) 컴포넌트 생성(기존 패턴)
	obj->CreateComponents(pd3dDevice, pd3dCommandList);

	// 9) Scene이 소유
	m_pPlayer = obj;
}




void CScene::BuildObjects(
	ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList)
{
	// ============================================================
	// 1. Root Signature
	// ============================================================
	CreateGraphicsRootSignature(pd3dDevice);

	// ============================================================
	// 2. Shader 인스턴스 생성 + 오브젝트 개수 선확보
	// ============================================================
	constexpr int MAX_GLOBAL_SRVS = 1024;

	auto pStaticShader = std::make_shared<CStaticObjectsShader>();
	auto pSkinnedShader = std::make_shared<CSkinnedObjectsShader>();

	m_staticBatch.shader = pStaticShader;
	m_skinnedBatch.shader = pSkinnedShader;

	const UINT cbvTotal =
		m_staticBatch.capacity +
		m_skinnedBatch.capacity +
		1 /*Camera*/ +
		1 /*Player*/ +
		1 /*etc*/;

	// ============================================================
	// 3. DescriptorHeap은 "한 번만" 생성
	// ============================================================
	m_pDescriptorHeap->CreateCbvSrvDescriptorHeaps(
		pd3dDevice,
		cbvTotal,
		MAX_GLOBAL_SRVS
	);

	// 공통 RTV 포맷
	DXGI_FORMAT rtvFormats[5] =
	{
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		DXGI_FORMAT_R32_FLOAT
	};

	// ============================================================
	// 4/5. Materials + Batches
	// ============================================================
	BuildLightsAndMaterials();

	// Light objects components create
	for (auto& lo : m_lightObjects)
	{
		if (lo) lo->CreateComponents(pd3dDevice, pd3dCommandList);
	}

	constexpr UINT kRTCount = 5;
	const DXGI_FORMAT kDsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	BuildStaticBatch(pd3dDevice, pd3dCommandList, pStaticShader, kRTCount, rtvFormats, kDsvFormat);
	BuildSkinnedBatch(pd3dDevice, pd3dCommandList, pSkinnedShader, kRTCount, rtvFormats, kDsvFormat);

	// ============================================================
	// 6. Scene 공통 Shader Variables
	// ============================================================
	CreateShaderVariables(pd3dDevice, pd3dCommandList);
	BuildPlayer(pd3dDevice, pd3dCommandList);
	CreateMainCamera(pd3dDevice, pd3dCommandList, m_pPlayer.get());
}



void CScene::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
	// (1) Descriptor Ranges: CBV table 1개 + Global SRV table 1개만 남긴다.
	D3D12_DESCRIPTOR_RANGE pd3dDescriptorRanges[2] = {};

	// b2: Game Objects (CBV table)
	pd3dDescriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	pd3dDescriptorRanges[0].NumDescriptors = 1;
	pd3dDescriptorRanges[0].BaseShaderRegister = 2; // b2
	pd3dDescriptorRanges[0].RegisterSpace = 0;
	pd3dDescriptorRanges[0].OffsetInDescriptorsFromTableStart = 0;

	// Global Texture2D pool: t1~t1024, space1 (SRV table)
	constexpr UINT MAX_GLOBAL_SRVS = 1024;
	pd3dDescriptorRanges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[1].NumDescriptors = MAX_GLOBAL_SRVS;
	pd3dDescriptorRanges[1].BaseShaderRegister = 0; // t0부터
	pd3dDescriptorRanges[1].RegisterSpace = 0;      // space0
	pd3dDescriptorRanges[1].OffsetInDescriptorsFromTableStart = 0;

	// (2) Root Parameters: SRV 분리(5/6) 제거, Global SRV 하나만 유지
	D3D12_ROOT_PARAMETER pd3dRootParameters[9] = {};

	// [0] b1: Camera
	pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[0].Descriptor.ShaderRegister = 1;
	pd3dRootParameters[0].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// [1] b0: Player
	pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[1].Descriptor.ShaderRegister = 0;
	pd3dRootParameters[1].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// [2] (Table) b2: Game Objects
	pd3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[2].DescriptorTable.pDescriptorRanges = &pd3dDescriptorRanges[0];
	pd3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// [3] b3: Materials
	pd3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[3].Descriptor.ShaderRegister = 3;
	pd3dRootParameters[3].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// [4] b4: Lights
	pd3dRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[4].Descriptor.ShaderRegister = 4;
	pd3dRootParameters[4].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// [5] b5: DrawOptions (PostProcessing 옵션 + SRV 인덱스들)
	pd3dRootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[5].Descriptor.ShaderRegister = 5;
	pd3dRootParameters[5].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// [6] (Table) Global SRV table (space1, t1~)
	pd3dRootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[6].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[6].DescriptorTable.pDescriptorRanges = &pd3dDescriptorRanges[1];
	pd3dRootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// pd3dRootParameters[7]
	pd3dRootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	pd3dRootParameters[7].Constants.Num32BitValues = 1;
	pd3dRootParameters[7].Constants.ShaderRegister = 6; // b6
	pd3dRootParameters[7].Constants.RegisterSpace = 0;
	pd3dRootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// [8] b7: Bones
	pd3dRootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	pd3dRootParameters[8].Descriptor.ShaderRegister = 7; // b7
	pd3dRootParameters[8].Descriptor.RegisterSpace = 0;
	pd3dRootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;


	// Static sampler (s0)
	D3D12_STATIC_SAMPLER_DESC d3dSamplerDesc = {};
	d3dSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	d3dSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	d3dSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	d3dSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	d3dSamplerDesc.MipLODBias = 0;
	d3dSamplerDesc.MaxAnisotropy = 1;
	d3dSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	d3dSamplerDesc.MinLOD = 0;
	d3dSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	d3dSamplerDesc.ShaderRegister = 0;
	d3dSamplerDesc.RegisterSpace = 0;
	d3dSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc = {};
	d3dRootSignatureDesc.NumParameters = _countof(pd3dRootParameters);
	d3dRootSignatureDesc.pParameters = pd3dRootParameters;
	d3dRootSignatureDesc.NumStaticSamplers = 1;
	d3dRootSignatureDesc.pStaticSamplers = &d3dSamplerDesc;
	d3dRootSignatureDesc.Flags = d3dRootSignatureFlags;

	ComPtr<ID3DBlob> pd3dSignatureBlob;
	ComPtr<ID3DBlob> pd3dErrorBlob;

	HRESULT hr = D3D12SerializeRootSignature(
		&d3dRootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&pd3dSignatureBlob,
		&pd3dErrorBlob
	);

	if (FAILED(hr))
	{
		if (pd3dErrorBlob)
			OutputDebugStringA((char*)pd3dErrorBlob->GetBufferPointer());
		return;
	}

	hr = pd3dDevice->CreateRootSignature(
		0,
		pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(m_pd3dGraphicsRootSignature.ReleaseAndGetAddressOf())
	);

	if (FAILED(hr))
	{
		OutputDebugStringA("CreateRootSignature failed.\n");
		return;
	}

	if (pd3dSignatureBlob) pd3dSignatureBlob.Reset();
	if (pd3dErrorBlob) pd3dErrorBlob.Reset();
}

void CScene::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	UINT ncbElementBytes = ((sizeof(LIGHTS) + 255) & ~255); //256의 배수
	m_pd3dcbLights = ::CreateBufferResource(
		pd3dDevice,
		pd3dCommandList,
		nullptr,
		ncbElementBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	m_pd3dcbLights->Map(0, nullptr, (void**)&m_pcbMappedLights);

	UINT ncbMaterialBytes = ((sizeof(MATERIALS) + 255) & ~255); //256의 배수
	m_pd3dcbMaterials = ::CreateBufferResource(
		pd3dDevice,
		pd3dCommandList,
		nullptr,
		ncbMaterialBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);

	m_pd3dcbMaterials->Map(0, nullptr, (void**)&m_pcbMappedMaterials);
}

bool CScene::OnProcessingMouseMessage(HWND /*hWnd*/, UINT nMessageID, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	if (nMessageID == WM_LBUTTONDOWN)
	{
		if (m_pPlayer)
		{
			// 1) AnimController가 있으면 Attack 요청
			if (auto* ctrl = m_pPlayer->GetAnimController())
			{
				ctrl->RequestAttack();
				return true; // 처리했음
			}

			// 2) AnimatorComponent 기반이면 (프로젝트에서 쓰는 경우) 여기서 추가 분기 가능
			//    (현재 첨부 코드상 GetAnimController 경로가 이미 존재하므로 우선 생략)
		}
	}
	return false;
}


bool CScene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
{
	return(false);
}

void CScene::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	// ============================================================
	// Pack LightComponents -> mapped LIGHTS (GPU upload heap)
	// ============================================================
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

	::memcpy(m_pcbMappedMaterials, m_pMaterials.get(), sizeof(MATERIALS));

	// =========================
	// Static batch per-object CB
	// =========================
	if (m_staticBatch.mappedGameObjects && !m_staticBatch.objectRefs.empty())
	{
		const UINT ncb = m_staticBatch.cbElementBytes;

		for (UINT j = 0; j < (UINT)m_staticBatch.objectRefs.size(); ++j)
		{
			auto* obj = m_staticBatch.objectRefs[j];
			if (!obj) continue;

			auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)m_staticBatch.mappedGameObjects + j * ncb);

			const XMFLOAT4X4& W = obj->GetWorldMatrix(); // Transform에서 가져와야 함

			XMStoreFloat4x4(
				&cb->m_xmf4x4World,
				XMMatrixTranspose(XMLoadFloat4x4(&W))
			);

			cb->m_nObjectID = j;
		}
	}

	// =========================
	// Skinned batch per-object CB
	// =========================
	if (m_skinnedBatch.mappedGameObjects && !m_skinnedBatch.objectRefs.empty())
	{
		const UINT ncb = m_skinnedBatch.cbElementBytes;

		for (UINT j = 0; j < (UINT)m_skinnedBatch.objectRefs.size(); ++j)
		{
			auto* obj = m_skinnedBatch.objectRefs[j];
			if (!obj) continue;

			auto* cb = (CB_GAMEOBJECT_INFO*)((UINT8*)m_skinnedBatch.mappedGameObjects + j * ncb);

			const XMFLOAT4X4& W = obj->GetWorldMatrix(); // Transform에서 가져와야 함

			XMStoreFloat4x4(
				&cb->m_xmf4x4World,
				XMMatrixTranspose(XMLoadFloat4x4(&W))
			);

			cb->m_nObjectID = j;
		}
	}
	// =========================
	// Player per-object CB (single)
	// =========================
	if (m_pPlayer && m_pcbMappedPlayerGameObject)
	{
		const XMFLOAT4X4& W = m_pPlayer->GetWorldMatrix();

		XMStoreFloat4x4(
			&m_pcbMappedPlayerGameObject->m_xmf4x4World,
			XMMatrixTranspose(XMLoadFloat4x4(&W))
		);

		m_pcbMappedPlayerGameObject->m_nObjectID = 0; // 필요하면 플레이어 ID로
	}

}

bool CScene::ProcessInput(UCHAR* pKeysBuffer)
{
	return(false);
}

void CScene::AnimateObjects(float fTimeElapsed)
{
	for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
	{
		if (!m_staticObjects[j]) continue;
		m_staticObjects[j]->Animate(fTimeElapsed);
	}

	for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
	{
		if (!m_skinnedObjects[j]) continue;
		m_skinnedObjects[j]->Animate(fTimeElapsed);
	}
	if (m_pPlayer)
		m_pPlayer->Animate(fTimeElapsed);

	// Player spot light follower target set (once)
	if (m_pPlayer && m_pPlayerSpotFollower && (m_pPlayerSpotFollower->GetTarget() == nullptr))
	{
		m_pPlayerSpotFollower->SetTarget(m_pPlayer.get());
	}

	// Update light objects (FollowTransformComponent 포함)
	for (UINT j = 0; j < (UINT)m_lightObjects.size(); ++j)
	{
		if (!m_lightObjects[j]) continue;
		m_lightObjects[j]->Animate(fTimeElapsed);
	}


}


void CScene::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get());
	pd3dCommandList->SetDescriptorHeaps(1, m_pDescriptorHeap->m_pd3dCbvSrvDescriptorHeap.GetAddressOf());

	pd3dCommandList->SetGraphicsRootDescriptorTable(
		ROOT_PARAMETER_GLOBAL_SRV,
		m_pDescriptorHeap->GetGPUSrvDescriptorStartHandle()
	);

	pCamera->SetViewportsAndScissorRects(pd3dCommandList);
	pCamera->UpdateShaderVariables(pd3dCommandList);

	UpdateShaderVariables(pd3dCommandList);

	D3D12_GPU_VIRTUAL_ADDRESS d3dcbLightsGpuVirtualAddress = m_pd3dcbLights->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_LIGHT, d3dcbLightsGpuVirtualAddress); //Lights

	D3D12_GPU_VIRTUAL_ADDRESS d3dcbMaterialsGpuVirtualAddress = m_pd3dcbMaterials->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(ROOT_PARAMETER_MATERIAL, d3dcbMaterialsGpuVirtualAddress); //Materials
}

void CScene::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	// ---- Static batch ----
	if (m_staticBatch.shader)
	{
		m_staticBatch.shader->Render(pd3dCommandList, pCamera, &m_staticBatch);
		for (UINT j = 0; j < (UINT)m_staticObjects.size(); ++j)
		{
			if (!m_staticObjects[j]) continue;
			m_staticObjects[j]->Render(pd3dCommandList, pCamera);
		}
	}

	// ---- Skinned batch ----
	if (m_skinnedBatch.shader)
	{
		m_skinnedBatch.shader->Render(pd3dCommandList, pCamera, &m_skinnedBatch);
		for (UINT j = 0; j < (UINT)m_skinnedObjects.size(); ++j)
		{
			if (!m_skinnedObjects[j]) continue;
			m_skinnedObjects[j]->Render(pd3dCommandList, pCamera);
		}
	}

	// ---- Player ----
	if (m_pPlayer)
		m_pPlayer->Render(pd3dCommandList, pCamera);
}
