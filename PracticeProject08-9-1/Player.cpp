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
}

CPlayer::~CPlayer()
{
	ReleaseShaderVariables();

}

void CPlayer::CreateShaderVariables(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
	if (m_pCamera)
		m_pCamera->CreateShaderVariables(dev, cmd);

	// -------------------------
	// (1) b0: Player CB 유지(남겨도 무방)
	// -------------------------
	UINT cbPlayerBytes = (sizeof(CB_PLAYER_INFO) + 255) & ~255;
	m_pd3dcbPlayer = ::CreateBufferResource(
		dev, cmd,
		nullptr,
		cbPlayerBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);
	m_pd3dcbPlayer->Map(0, nullptr, (void**)&m_pcbMappedPlayer);

	// -------------------------
	// (2) b2: CB_GAMEOBJECT_INFO (CSkinnedObjectsShader가 읽는 CB)
	// -------------------------
	UINT cbObjBytes = (sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255;
	m_pd3dcbGameObject = ::CreateBufferResource(
		dev, cmd,
		nullptr,
		cbObjBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr
	);
	m_pd3dcbGameObject->Map(0, nullptr, (void**)&m_pcbMappedGameObject);
}

void CPlayer::ReleaseShaderVariables()
{
	if (m_pCamera) m_pCamera->ReleaseShaderVariables();

	if (m_pd3dcbPlayer)
	{
		m_pd3dcbPlayer->Unmap(0, nullptr);
		m_pd3dcbPlayer.Reset();
		m_pcbMappedPlayer = nullptr;
	}

	CGameObject::ReleaseShaderVariables();
}


void CPlayer::UpdateShaderVariables(ID3D12GraphicsCommandList* cmd)
{
	(void)cmd;

	const XMFLOAT4X4& W = m_pTransform->GetWorldMatrix();

	// b0
	if (m_pcbMappedPlayer)
	{
		XMStoreFloat4x4(
			&m_pcbMappedPlayer->m_xmf4x4World,
			XMMatrixTranspose(XMLoadFloat4x4(&W))
		);
	}

	// b2
	if (m_pcbMappedGameObject)
	{
		XMStoreFloat4x4(
			&m_pcbMappedGameObject->m_xmf4x4World,
			XMMatrixTranspose(XMLoadFloat4x4(&W))
		);
		m_pcbMappedGameObject->m_nObjectID = 0;
	}
}

void CPlayer::Move(DWORD dwDirection, float fDistance, bool bUpdateVelocity,
	CPlayer::EVerticalMoveSpace upSpace)
{
	XMFLOAT3 look = GetLookVector();
	XMFLOAT3 right = GetRightVector();

	// DIR_UP/DOWN은 옵션: 월드업 vs 로컬업
	XMFLOAT3 up = (upSpace == EVerticalMoveSpace::LocalUp)
		? GetUpVector()
		: XMFLOAT3(0.0f, 1.0f, 0.0f);

	if (dwDirection)
	{
		XMFLOAT3 xmf3Shift = XMFLOAT3(0, 0, 0);
		if (dwDirection & DIR_FORWARD)
			xmf3Shift = Vector3::Add(xmf3Shift, look, fDistance);

		if (dwDirection & DIR_BACKWARD)
			xmf3Shift = Vector3::Add(xmf3Shift, look, -fDistance);

		if (dwDirection & DIR_RIGHT)
			xmf3Shift = Vector3::Add(xmf3Shift, right, fDistance);

		if (dwDirection & DIR_LEFT)
			xmf3Shift = Vector3::Add(xmf3Shift, right, -fDistance);

		if (dwDirection & DIR_UP)
			xmf3Shift = Vector3::Add(xmf3Shift, up, fDistance);

		if (dwDirection & DIR_DOWN)
			xmf3Shift = Vector3::Add(xmf3Shift, up, -fDistance);

		Move(xmf3Shift, bUpdateVelocity);
	}
}

void CPlayer::Move(const XMFLOAT3& xmf3Shift, bool bUpdateVelocity)
{
	if (bUpdateVelocity)
	{
		m_xmf3Velocity = Vector3::Add(m_xmf3Velocity, xmf3Shift);
	}
	else
	{
		if (m_pTransform)
			m_pTransform->Translate(xmf3Shift);

		m_pCamera->Move(xmf3Shift);
	}

}

void CPlayer::Rotate(float x, float y, float z)
{
	DWORD nCurrentCameraMode = m_pCamera->GetMode();

	if ((nCurrentCameraMode == FIRST_PERSON_CAMERA) || (nCurrentCameraMode == THIRD_PERSON_CAMERA))
	{
		if (y != 0.0f)
		{
			m_fYaw += y;
			if (m_fYaw > 360.0f) m_fYaw -= 360.0f;
			if (m_fYaw < 0.0f)   m_fYaw += 360.0f;
		}

		// 카메라 회전은 기존대로
		m_pCamera->Rotate(x, y, z);

		// Player 바디는 “yaw만” Transform으로 반영 (기존 코드와 동일한 의미)
		if (m_pTransform)
			m_pTransform->SetYawDegrees(m_fYaw);
	}
	else if (nCurrentCameraMode == SPACESHIP_CAMERA)
	{
		// 스페이스쉽 모드: 카메라도, 바디도 자유 회전
		m_pCamera->Rotate(x, y, z);

		if (m_pTransform)
		{
			if (x != 0.0f)
			{
				XMFLOAT3 axis = m_pTransform->GetRight();
				m_pTransform->RotateWorldAxisDegrees(axis, x);
			}
			if (y != 0.0f)
			{
				XMFLOAT3 axis = m_pTransform->GetUp();
				m_pTransform->RotateWorldAxisDegrees(axis, y);
			}
			if (z != 0.0f)
			{
				XMFLOAT3 axis = m_pTransform->GetLook();
				m_pTransform->RotateWorldAxisDegrees(axis, z);
			}
		}
	}
}

void CPlayer::Update(float fTimeElapsed)
{
	m_xmf3Velocity = Vector3::Add(m_xmf3Velocity, Vector3::ScalarProduct(m_xmf3Gravity, fTimeElapsed, false));
	float fLength = sqrtf(m_xmf3Velocity.x * m_xmf3Velocity.x + m_xmf3Velocity.z * m_xmf3Velocity.z);
	float fMaxVelocityXZ = m_fMaxVelocityXZ * fTimeElapsed;

	if (fLength > m_fMaxVelocityXZ)
	{
		m_xmf3Velocity.x *= (fMaxVelocityXZ / fLength);
		m_xmf3Velocity.z *= (fMaxVelocityXZ / fLength);
	}
	float fMaxVelocityY = m_fMaxVelocityY * fTimeElapsed;
	fLength = sqrtf(m_xmf3Velocity.y * m_xmf3Velocity.y);

	if (fLength > m_fMaxVelocityY)
		m_xmf3Velocity.y *= (fMaxVelocityY / fLength);

	Move(m_xmf3Velocity, false);

	if (m_pPlayerUpdatedContext)
		OnPlayerUpdateCallback(fTimeElapsed);

	DWORD nCurrentCameraMode = m_pCamera->GetMode();

	XMFLOAT3 pos = GetPosition();

	if (nCurrentCameraMode == THIRD_PERSON_CAMERA)
		m_pCamera->Update(pos, fTimeElapsed);

	if (m_pCameraUpdatedContext)
		OnCameraUpdateCallback(fTimeElapsed);

	if (nCurrentCameraMode == THIRD_PERSON_CAMERA)
		m_pCamera->SetLookAt(pos);

	m_pCamera->RegenerateViewMatrix();

	fLength = Vector3::Length(m_xmf3Velocity);
	float fDeceleration = (m_fFriction * fTimeElapsed);

	if (fDeceleration > fLength)
		fDeceleration = fLength;

	m_xmf3Velocity = Vector3::Add(m_xmf3Velocity, Vector3::ScalarProduct(m_xmf3Velocity, -fDeceleration, true));
	this->Animate(fTimeElapsed);
}

unique_ptr<CCamera> CPlayer::OnChangeCamera(DWORD nNewCameraMode, DWORD nCurrentCameraMode)
{
	unique_ptr<CCamera> pNewCamera;
	switch (nNewCameraMode)
	{
		case FIRST_PERSON_CAMERA:
			pNewCamera = make_unique<CFirstPersonCamera>(m_pCamera.get());
			break;
		case THIRD_PERSON_CAMERA:
			pNewCamera = make_unique<CThirdPersonCamera>(m_pCamera.get());
			break;
		case SPACESHIP_CAMERA:
			pNewCamera = make_unique<CSpaceShipCamera>(m_pCamera.get());
			break;
	}
	if (nCurrentCameraMode == SPACESHIP_CAMERA)
	{
		XMFLOAT3 look = GetLookVector();
		look.y = 0.0f;
		look = Vector3::Normalize(look);

		m_fYaw = Vector3::Angle(XMFLOAT3(0.0f, 0.0f, 1.0f), look);
		if (look.x < 0.0f) m_fYaw = -m_fYaw;

		if (m_pTransform)
			m_pTransform->SetYawDegrees(m_fYaw);
	}
	else if ((nNewCameraMode == SPACESHIP_CAMERA) && m_pCamera)
	{
		if (m_pTransform)
		{
			XMFLOAT3 r = m_pCamera->GetRightVector();
			XMFLOAT3 u = m_pCamera->GetUpVector();
			XMFLOAT3 l = m_pCamera->GetLookVector();
			m_pTransform->SetRotationFromBasis(r, u, l);
		}
	}

	if (pNewCamera)
	{
		pNewCamera->SetMode(nNewCameraMode);
		pNewCamera->SetPlayer(this);
	}

	return(move(pNewCamera));
}

void CPlayer::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera)
{
	(void)pd3dCommandList;
	(void)pCamera;
}


void CPlayer::SetRootParameter(ID3D12GraphicsCommandList* cmd)
{
	cmd->SetGraphicsRootDescriptorTable(
		ROOT_PARAMETER_OBJECT,
		m_d3dCbvGPUDescriptorHandle
	);

	if (m_bSkinnedObject && m_pd3dcbBoneTransforms)
	{
		cmd->SetGraphicsRootConstantBufferView(
			ROOT_PARAMETER_BONE_PALETTE,
			m_pd3dcbBoneTransforms->GetGPUVirtualAddress()
		);
	}
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
	m_pCamera = OnChangeCamera(THIRD_PERSON_CAMERA, 0.0f);
	switch (m_pCamera->GetMode())
	{
	case FIRST_PERSON_CAMERA:
		SetFriction(200.0f);
		SetGravity(XMFLOAT3(0.0f, 0.0f, 0.0f));
		SetMaxVelocityXZ(125.0f);
		SetMaxVelocityY(400.0f);
		m_pCamera->SetTimeLag(0.0f);
		m_pCamera->SetOffset(XMFLOAT3(0.0f, 20.0f, 0.0f));
		m_pCamera->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
		m_pCamera->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
		m_pCamera->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
		break;
	case SPACESHIP_CAMERA:
		SetFriction(125.0f);
		SetGravity(XMFLOAT3(0.0f, 0.0f, 0.0f));
		SetMaxVelocityXZ(400.0f);
		SetMaxVelocityY(400.0f);
		m_pCamera->SetTimeLag(0.0f);
		m_pCamera->SetOffset(XMFLOAT3(0.0f, 0.0f, 0.0f));
		m_pCamera->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
		m_pCamera->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
		m_pCamera->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
		break;
	case THIRD_PERSON_CAMERA:
		SetFriction(250.0f);
		SetGravity(XMFLOAT3(0.0f, 0.0f, 0.0f));
		SetMaxVelocityXZ(125.0f);
		SetMaxVelocityY(400.0f);
		m_pCamera->SetTimeLag(0.25f);
		m_pCamera->SetOffset(XMFLOAT3(0.0f, 1.0f, -2.0f));
		m_pCamera->GenerateProjectionMatrix(1.01f, 5000.0f, ASPECT_RATIO, 60.0f);
		m_pCamera->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
		m_pCamera->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
		break;
	default:
		break;
	}
	m_pCamera->SetPosition(Vector3::Add(GetPosition(), m_pCamera->GetOffset()));
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
	AnimationClip idleClip;
	if (!m_ppMeshes.empty() && m_ppMeshes[0] &&
		m_ppMeshes[0]->LoadAnimationFromBIN(
			"Assets/Fighter/Animation/FighterIdle.bin",
			"Idle", idleClip, 1.0f))
	{
		idleClip.name = "Idle";
		if (animComp) animComp->AddClip(idleClip);
	}

	AnimationClip runClip;
	if (!m_ppMeshes.empty() && m_ppMeshes[0] &&
		m_ppMeshes[0]->LoadAnimationFromBIN(
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
	UINT cbObjBytes = (sizeof(CB_GAMEOBJECT_INFO) + 255) & ~255;

	D3D12_GPU_DESCRIPTOR_HANDLE hPlayerObjCbv =
		CScene::m_pDescriptorHeap->CreateConstantBufferView(
			pd3dDevice,
			m_pd3dcbGameObject.Get(),
			cbObjBytes
		);

	SetCbvGPUDescriptorHandle(hPlayerObjCbv);

	// ------------------------------------------------------------
	// (3) Material에 Shader 연결
	// ------------------------------------------------------------
	SetShader(pShader);
	if (pPlayerMesh && pPlayerMesh->IsSkinnedMesh())
	{
		if (!GetRenderer()) AddComponent<CSkinnedMeshRendererComponent>();
	}
}

CFighterPlayer::~CFighterPlayer()
{
}

void CFighterPlayer::OnPrepareRender(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera)
{
	CPlayer::OnPrepareRender(pd3dCommandList, pCamera);
}

CCamera * CFighterPlayer::ChangeCamera(DWORD nNewCameraMode, float fTimeElapsed)
{
	DWORD nCurrentCameraMode = (m_pCamera)? m_pCamera->GetMode(): 0x00;
	if (nCurrentCameraMode == nNewCameraMode)return(m_pCamera.get());
	switch (nNewCameraMode)
	{
		case FIRST_PERSON_CAMERA:
			SetFriction(200.0f);
			SetGravity(XMFLOAT3(0.0f, 0.0f, 0.0f));
			SetMaxVelocityXZ(125.0f);
			SetMaxVelocityY(400.0f);
			m_pCamera = OnChangeCamera(FIRST_PERSON_CAMERA, nCurrentCameraMode);
			m_pCamera->SetTimeLag(0.0f);
			m_pCamera->SetOffset(XMFLOAT3(0.0f, 20.0f, 0.0f));
			m_pCamera->GenerateProjectionMatrix(10.01f, 5000.0f, ASPECT_RATIO, 60.0f);
			m_pCamera->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
			m_pCamera->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
			break;
		case SPACESHIP_CAMERA:
			SetFriction(125.0f);
			SetGravity(XMFLOAT3(0.0f, 0.0f, 0.0f));
			SetMaxVelocityXZ(400.0f);
			SetMaxVelocityY(400.0f);
			m_pCamera = OnChangeCamera(SPACESHIP_CAMERA, nCurrentCameraMode);
			m_pCamera->SetTimeLag(0.0f);
			m_pCamera->SetOffset(XMFLOAT3(0.0f, 0.0f, 0.0f));
			m_pCamera->GenerateProjectionMatrix(10.01f, 5000.0f, ASPECT_RATIO, 60.0f);
			m_pCamera->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
			m_pCamera->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
			break;
		case THIRD_PERSON_CAMERA:
			SetFriction(250.0f);
			SetGravity(XMFLOAT3(0.0f, 0.0f, 0.0f));
			SetMaxVelocityXZ(125.0f);
			SetMaxVelocityY(400.0f);
			m_pCamera = OnChangeCamera(THIRD_PERSON_CAMERA, nCurrentCameraMode);
			m_pCamera->SetTimeLag(0.25f);
			m_pCamera->SetOffset(XMFLOAT3(0.0f, 1.0f, -2.0f));
			m_pCamera->GenerateProjectionMatrix(10.01f, 5000.0f, ASPECT_RATIO, 60.0f);
			m_pCamera->SetViewport(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT, 0.0f, 1.0f);
			m_pCamera->SetScissorRect(0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT);
			break;
		default:
			break;
	}
	m_pCamera->SetPosition(Vector3::Add(GetPosition(), m_pCamera->GetOffset()));
	Update(fTimeElapsed);

	return(m_pCamera.get());
}
