//-----------------------------------------------------------------------------
// File: CPlayer.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Player.h"
#include "Shader.h"
#include "Scene.h"
#include "AssetManager.h"
#include "AnimController.h"
#include "AnimatorComponent.h"
#include "RenderObjectComponent.h"
#include "PlayerControllerComponent.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPlayer

CPlayer::CPlayer(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, void *pContext, int nMeshes): CGameObject(nMeshes)
{
	if (m_pTransform)
	{
		m_pTransform->SetPosition(XMFLOAT3(0.0f, 0.0f, 0.0f));
		m_pTransform->SetYawDegrees(0.0f);
	}

	m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmf3Gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_fMaxVelocityXZ = 0.0f;
	m_fMaxVelocityY = 0.0f;
	m_fFriction = 0.0f;

	m_fYaw = 0.0f;
	// =====================================================
	// Movement/Physics/Input/AnimSpeed 권위: PlayerControllerComponent
	// =====================================================
	m_pPlayerController = AddComponent<CPlayerControllerComponent>();

	if (m_pPlayerController)
	{
		m_pPlayerController->SetVelocity(m_xmf3Velocity);
		m_pPlayerController->SetGravity(m_xmf3Gravity);
		m_pPlayerController->SetMaxVelocityXZ(m_fMaxVelocityXZ);
		m_pPlayerController->SetMaxVelocityY(m_fMaxVelocityY);
		m_pPlayerController->SetFriction(m_fFriction);
		m_pPlayerController->SetYawDegrees(m_fYaw);
	}

}

CPlayer::~CPlayer()
{
	ReleaseShaderVariables();

}

void CPlayer::CreateShaderVariables(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
	CreateComponents(dev, cmd);

	// 안전장치: 플레이어는 로컬 CB가 필요
	if (auto* ro = GetComponent<CRenderObjectComponent>())
		ro->CreateLocalCB(dev, cmd);

	// (1) b0: Player CB 유지
	UINT cbPlayerBytes = (sizeof(CB_PLAYER_INFO) + 255) & ~255;
	m_pd3dcbPlayer = ::CreateBufferResource(
		dev, cmd, nullptr, cbPlayerBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);
	m_pd3dcbPlayer->Map(0, nullptr, (void**)&m_pcbMappedPlayer);

	// (2) b2: CB_GAMEOBJECT_INFO 는 RenderObjectComponent가 이미 생성/맵핑했어야 함
	//     -> 여기서 직접 만들지 않는다.
}


void CPlayer::ReleaseShaderVariables()
{
	if (m_pd3dcbPlayer)
	{
		m_pd3dcbPlayer->Unmap(0, nullptr);
		m_pd3dcbPlayer.Reset();
		m_pcbMappedPlayer = nullptr;
	}

	// b2/bone 은 컴포넌트가 소유/정리
	CGameObject::ReleaseShaderVariables(); // 여기서도 레거시 해제가 없도록 정리돼 있어야 함
}



void CPlayer::UpdateShaderVariables(ID3D12GraphicsCommandList* cmd)
{
	(void)cmd;
	const XMFLOAT4X4& W = m_pTransform->GetWorldMatrix();

	// b0
	if (m_pcbMappedPlayer)
	{
		XMStoreFloat4x4(&m_pcbMappedPlayer->m_xmf4x4World,
			XMMatrixTranspose(XMLoadFloat4x4(&W)));
	}

	// b2 (RenderObjectComponent 로컬 CB)
	if (auto* ro = GetComponent<CRenderObjectComponent>())
	{
		if (auto* cb = ro->GetMappedCB())
		{
			XMStoreFloat4x4(&cb->m_xmf4x4World,
				XMMatrixTranspose(XMLoadFloat4x4(&W)));
			cb->m_nObjectID = ro->GetObjectID();
		}
	}
}


void CPlayer::Move(DWORD dwDirection, float fDistance, bool bUpdateVelocity,
	CPlayer::EVerticalMoveSpace upSpace)
{
	if (!m_pPlayerController)
		return;

	const auto space =
		(upSpace == CPlayer::EVerticalMoveSpace::LocalUp)
		? CPlayerControllerComponent::EVerticalMoveSpace::LocalUp
		: CPlayerControllerComponent::EVerticalMoveSpace::WorldUp;

	m_pPlayerController->Move(dwDirection, fDistance, bUpdateVelocity, space);
}


void CPlayer::Move(const XMFLOAT3& xmf3Shift, bool bUpdateVelocity)
{
	if (!m_pPlayerController)
		return;

	m_pPlayerController->MoveShift(xmf3Shift, bUpdateVelocity);
}


void CPlayer::Rotate(float x, float y, float z)
{
	if (!m_pPlayerController)
		return;

	m_pPlayerController->Rotate(x, y, z);
}



void CPlayer::Update(float fTimeElapsed)
{
	// movement/physics 는 CPlayerControllerComponent::OnUpdate(dt) 에서 처리됨
	// (CGameObject::Animate -> UpdateComponents -> OnUpdate 호출)
	this->Animate(fTimeElapsed);

	if (m_pPlayerUpdatedContext)
		OnPlayerUpdateCallback(fTimeElapsed);
}


void CPlayer::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	(void)pd3dCommandList;
	(void)pCamera;
}


void CPlayer::SetRootParameter(ID3D12GraphicsCommandList* cmd)
{
	CGameObject::SetRootParameter(cmd); // RenderObject+Skinning 기반 바인딩
}

void CPlayer::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	DWORD nCameraMode = (pCamera) ? pCamera->GetMode() : 0x00;
	if (nCameraMode == THIRD_PERSON_CAMERA)CGameObject::Render(pd3dCommandList, pCamera);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CFighterPlayer

CFighterPlayer::CFighterPlayer(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, void *pContext, int nMeshes): CPlayer(pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, pContext, nMeshes)
{
	// ---- movement tuning (필수) ----
	SetMaxVelocityXZ(50.0f);   // 값은 취향 (클수록 빨라짐)
	SetMaxVelocityY(50.0f);    // 점프/상하 이동 안 쓰면 커도 무방
	SetFriction(20.0f);        // 키 떼면 멈추게 하려면 0보다 크게
	// SetGravity(XMFLOAT3(0.0f, -9.8f, 0.0f)); // 지금 중력 원치 않으면 주석 유지


	Update(0.0f);

	CreateShaderVariables(pd3dDevice, pd3dCommandList);

	MATERIALS* pMaterials = reinterpret_cast<MATERIALS*>(pContext);

	// ------------------------------------------------------------
	// Fighter 에셋 로드 (CSkinnedObjectsShader의 Fighter와 동일)
	// ------------------------------------------------------------
	AssetBuildDesc FighterDesc =
	{
		AssetType::Fighter,
		"Assets/Fighter/Mesh/Fighter.bin",
		"Assets/Fighter/Texture"
	};

	BuiltAsset built = AssetManager::BuildAsset(
		pd3dDevice,
		pd3dCommandList,
		pMaterials,
		FighterDesc
	);

	std::shared_ptr<CMesh> pPlayerMesh = built.mesh;
	SetMesh(0, pPlayerMesh);

	// ------------------------------------------------------------
	// 스키닝 활성화 (CSkinnedObjectsShader의 obj->EnableSkinning과 동일)
	// ------------------------------------------------------------
	if (pPlayerMesh && pPlayerMesh->IsSkinnedMesh())
	{
		const int nBones = pPlayerMesh->GetBoneCount();
		EnableSkinning(pd3dDevice, nBones);
	}

	// ------------------------------------------------------------
	// 애니메이션 로드 + Animator 세팅 + 재생 
	// ------------------------------------------------------------
	// ------------------------------------------------------------
	// Components: Renderer + Animator
	// (이미 추가돼있다면 중복 생성 방지)
	// ------------------------------------------------------------
	if (!GetRenderer())
		AddComponent<CSkinnedMeshRendererComponent>();

	CAnimatorComponent* animComp = GetComponent<CAnimatorComponent>();
	if (!animComp)
		animComp = AddComponent<CAnimatorComponent>();

	// ------------------------------------------------------------
	// 애니메이션 로드 + AnimatorComponent 세팅 + 초기 포즈 적용
	// ------------------------------------------------------------
	auto mesh0 = GetMeshShared(0); // ★ ModelComponent 경유

	AnimationClip idleClip;
	if (mesh0 && mesh0->LoadAnimationFromBIN(
		"Assets/Fighter/Animation/FighterIdle.bin",
		"Idle", idleClip, 1.0f))
	{
		idleClip.name = "Idle";
		if (animComp) animComp->AddClip(idleClip);
	}

	AnimationClip runClip;
	if (mesh0 && mesh0->LoadAnimationFromBIN(
		"Assets/Fighter/Animation/FighterRun.bin",
		"Run", runClip, 1.0f))
	{
		runClip.name = "Run";
		if (animComp) animComp->AddClip(runClip);
	}

	if (animComp)
	{
		animComp->SetIdleClip("Idle");
		animComp->SetMoveClip("Run");
		animComp->SetSpeed(0.0f);

		// ctor 단계에서도 바로 포즈 계산 + bone palette 업로드
		animComp->EvaluatePose(0.0f);
	}


	// ------------------------------------------------------------
	// Player CBV 생성/바인딩
	// ------------------------------------------------------------
	UINT ncbElementBytes = ((sizeof(CB_PLAYER_INFO) + 255) & ~255); // 256 align

	// ------------------------------------------------------------
	// CPlayerShader 생성 (여기서 MRT를 쓸 거면 RenderTarget 5개로 생성해야 함)
	// ------------------------------------------------------------
	DXGI_FORMAT rtvFormats[5] =
	{
		DXGI_FORMAT_R8G8B8A8_UNORM, // SV_TARGET0 color
		DXGI_FORMAT_R8G8B8A8_UNORM, // SV_TARGET1 cTexture
		DXGI_FORMAT_R8G8B8A8_UNORM, // SV_TARGET2 cIllumination
		DXGI_FORMAT_R8G8B8A8_UNORM, // SV_TARGET3 normal
		DXGI_FORMAT_R32_FLOAT       // SV_TARGET4 zDepth
	};

	// ------------------------------------------------------------
	// (1) 플레이어 셰이더를 CSkinnedObjectsShader로 생성
	// ------------------------------------------------------------
	shared_ptr<CSkinnedObjectsShader> pShader = make_shared<CSkinnedObjectsShader>();
	pShader->CreateShader(
		pd3dDevice,
		pd3dGraphicsRootSignature,
		5,
		rtvFormats,
		DXGI_FORMAT_D24_UNORM_S8_UINT
	);

	// ------------------------------------------------------------
	// (2) 플레이어는 b2(CB_GAMEOBJECT_INFO) 테이블을 사용해야 하므로
	//     m_pd3dcbGameObject로 CBV 디스크립터를 만든다.
	// ------------------------------------------------------------
	auto* ro = GetComponent<CRenderObjectComponent>();
	// ro는 GameObject ctor에서 기본으로 붙였다고 했으니 null이면 구조가 흔들린 것.
	if (ro)
	{
		if (!ro->GetCBResource())
			ro->CreateLocalCB(pd3dDevice, pd3dCommandList);

		const UINT cbObjBytes = (sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255;

		D3D12_GPU_DESCRIPTOR_HANDLE hPlayerObjCbv =
			CScene::m_pDescriptorHeap->CreateConstantBufferView(
				pd3dDevice,
				ro->GetCBResource(),
				cbObjBytes
			);

		SetCbvGPUDescriptorHandle(hPlayerObjCbv);
	}

	// ------------------------------------------------------------
	// (3) Material에 Shader 연결
	// ------------------------------------------------------------
	SetShader(pShader);
	//if (pPlayerMesh && pPlayerMesh->IsSkinnedMesh())
	//{
	//	if (!GetRenderer()) AddComponent<CSkinnedMeshRendererComponent>();
	//}
}

CFighterPlayer::~CFighterPlayer()
{
}

void CFighterPlayer::OnPrepareRender(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera)
{
	CPlayer::OnPrepareRender(pd3dCommandList, pCamera);
}
