//-----------------------------------------------------------------------------
// File: CPlayer.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Player.h"
#include "Shader.h"
#include "Scene.h"
#include "AssetManager.h"
#include "AnimController.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CPlayer

CPlayer::CPlayer(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, ID3D12RootSignature *pd3dGraphicsRootSignature, void *pContext, int nMeshes): CGameObject(nMeshes)
{
	m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);

	m_xmf3Velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmf3Gravity = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_fMaxVelocityXZ = 0.0f;
	m_fMaxVelocityY = 0.0f;
	m_fFriction = 0.0f;

	m_fPitch = 0.0f;
	m_fRoll = 0.0f;
	m_fYaw = 0.0f;
}

CPlayer::~CPlayer()
{
	ReleaseShaderVariables();

}

void CPlayer::CreateShaderVariables(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
	if (m_pCamera) m_pCamera->CreateShaderVariables(dev, cmd);

	// Player CB(b0)만 유지
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


void CPlayer::UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList)
{
	XMStoreFloat4x4(&m_pcbMappedPlayer->m_xmf4x4World,
		XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4World)));
}

void CPlayer::Move(DWORD dwDirection, float fDistance, bool bUpdateVelocity)
{
	if (dwDirection)
	{
		XMFLOAT3 xmf3Shift = XMFLOAT3(0, 0, 0);
		if (dwDirection & DIR_FORWARD)
			xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Look, fDistance);

		if (dwDirection & DIR_BACKWARD)
			xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Look, -fDistance);

		if (dwDirection & DIR_RIGHT)
			xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Right, fDistance);

		if (dwDirection & DIR_LEFT)
			xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Right, -fDistance);

		if (dwDirection & DIR_UP)
			xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Up, fDistance);

		if (dwDirection & DIR_DOWN)
			xmf3Shift = Vector3::Add(xmf3Shift, m_xmf3Up, -fDistance);

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
		m_xmf3Position = Vector3::Add(m_xmf3Position, xmf3Shift);
		m_pCamera->Move(xmf3Shift);
	}
}

void CPlayer::Rotate(float x, float y, float z)
{
	DWORD nCurrentCameraMode = m_pCamera->GetMode();
	if ((nCurrentCameraMode == FIRST_PERSON_CAMERA)|| (nCurrentCameraMode == THIRD_PERSON_CAMERA))
	{
		if (x != 0.0f)
		{
			m_fPitch += x;
			if (m_fPitch > +89.0f)
			{ 
				x -= (m_fPitch - 89.0f); 
				m_fPitch = +89.0f; 
			}

			if (m_fPitch < -89.0f)
			{ 
				x -= (m_fPitch + 89.0f);
				m_fPitch = -89.0f; 
			}
		}
		if (y != 0.0f)
		{
			m_fYaw += y;
			if (m_fYaw > 360.0f)
				m_fYaw -= 360.0f;

			if (m_fYaw < 0.0f)
				m_fYaw += 360.0f;
		}
		if (z != 0.0f)
		{
			m_fRoll += z;
			if (m_fRoll > +20.0f)
			{ 
				z -= (m_fRoll - 20.0f); 
				m_fRoll = +20.0f; 
			}

			if (m_fRoll < -20.0f)
			{ 
				z -= (m_fRoll + 20.0f); 
				m_fRoll = -20.0f; 
			}
		}
		m_pCamera->Rotate(x, y, z);

		if (y != 0.0f)
		{
			XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Up), XMConvertToRadians(y));
			m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, xmmtxRotate);
			m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, xmmtxRotate);
		}
	}
	else if (nCurrentCameraMode == SPACESHIP_CAMERA)
	{
		m_pCamera->Rotate(x, y, z);
		if (x != 0.0f)
		{
			XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Right), XMConvertToRadians(x));
			m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, xmmtxRotate);
			m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, xmmtxRotate);
		}
		if (y != 0.0f)
		{
			XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Up), XMConvertToRadians(y));
			m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, xmmtxRotate);
			m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, xmmtxRotate);
		}
		if (z != 0.0f)
		{
			XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Look), XMConvertToRadians(z));
			m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, xmmtxRotate);
			m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, xmmtxRotate);
		}
	}

	m_xmf3Look = Vector3::Normalize(m_xmf3Look);
	m_xmf3Right = Vector3::CrossProduct(m_xmf3Up, m_xmf3Look, true);
	m_xmf3Up = Vector3::CrossProduct(m_xmf3Look, m_xmf3Right, true);
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

	if (nCurrentCameraMode == THIRD_PERSON_CAMERA)
		m_pCamera->Update(m_xmf3Position, fTimeElapsed);

	if (m_pCameraUpdatedContext)
		OnCameraUpdateCallback(fTimeElapsed);

	if (nCurrentCameraMode == THIRD_PERSON_CAMERA)
		m_pCamera->SetLookAt(m_xmf3Position);

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
		m_xmf3Right = Vector3::Normalize(XMFLOAT3(m_xmf3Right.x, 0.0f, m_xmf3Right.z));
		m_xmf3Up = Vector3::Normalize(XMFLOAT3(0.0f, 1.0f, 0.0f));
		m_xmf3Look = Vector3::Normalize(XMFLOAT3(m_xmf3Look.x, 0.0f, m_xmf3Look.z));

		m_fPitch = 0.0f;
		m_fRoll = 0.0f;
		m_fYaw = Vector3::Angle(XMFLOAT3(0.0f, 0.0f, 1.0f), m_xmf3Look);
		if (m_xmf3Look.x < 0.0f)m_fYaw = -m_fYaw;
	}
	else if ((nNewCameraMode == SPACESHIP_CAMERA)&& m_pCamera)
	{
		m_xmf3Right = m_pCamera->GetRightVector();
		m_xmf3Up = m_pCamera->GetUpVector();
		m_xmf3Look = m_pCamera->GetLookVector();
	}

	if (pNewCamera)
	{
		pNewCamera->SetMode(nNewCameraMode);
		pNewCamera->SetPlayer(this);
	}

	return(move(pNewCamera));
}

void CPlayer::OnPrepareRender(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera)
{
	m_xmf4x4World._11 = m_xmf3Right.x; m_xmf4x4World._12 = m_xmf3Right.y; m_xmf4x4World._13 = m_xmf3Right.z;
	m_xmf4x4World._21 = m_xmf3Up.x; m_xmf4x4World._22 = m_xmf3Up.y; m_xmf4x4World._23 = m_xmf3Up.z;
	m_xmf4x4World._31 = m_xmf3Look.x; m_xmf4x4World._32 = m_xmf3Look.y; m_xmf4x4World._33 = m_xmf3Look.z;
	m_xmf4x4World._41 = m_xmf3Position.x; m_xmf4x4World._42 = m_xmf3Position.y; m_xmf4x4World._43 = m_xmf3Position.z;
}

void CPlayer::SetRootParameter(ID3D12GraphicsCommandList* cmd)
{
	cmd->SetGraphicsRootConstantBufferView(
		ROOT_PARAMETER_PLAYER,
		m_pd3dcbPlayer->GetGPUVirtualAddress()
	);

	// b7 bones
	if (m_pd3dcbBoneTransforms)
	{
		cmd->SetGraphicsRootConstantBufferView(
			ROOT_PARAMETER_BONE_PALETTE,
			m_pd3dcbBoneTransforms->GetGPUVirtualAddress()
		);
	}

	// b6 materialId
	cmd->SetGraphicsRoot32BitConstants(
		ROOT_PARAMETER_MATERIAL_ID,
		1, &m_nPlayerMaterialID, 0
	);
}

void CPlayer::Render(ID3D12GraphicsCommandList *pd3dCommandList, CCamera *pCamera)
{
	DWORD nCameraMode = (pCamera)? pCamera->GetMode(): 0x00;
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
	m_pCamera->SetPosition(Vector3::Add(m_xmf3Position, m_pCamera->GetOffset()));
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
	AnimationClip idleClip;
	bool idleLoaded = false;

	if (!m_ppMeshes.empty() && m_ppMeshes[0])
	{
		idleLoaded = m_ppMeshes[0]->LoadAnimationFromBIN(
			"Assets/Fighter/Animation/FighterIdle.bin", 
			"Idle",	idleClip, 1.0f );
	}

	if (idleLoaded)
	{
		idleClip.name = "Idle";

		CAnimator* anim = EnsureAnimator();
		if (anim)
			anim->AddClip(idleClip);
	}

	AnimationClip RunClip;
	bool RunLoaded = false;

	if (!m_ppMeshes.empty() && m_ppMeshes[0])
	{
		RunLoaded = m_ppMeshes[0]->LoadAnimationFromBIN(
			"Assets/Fighter/Animation/FighterRun.bin",
			"Run", RunClip, 1.0f);
	}
	if (RunLoaded)
	{
		RunClip.name = "Run";

		CAnimator* anim = EnsureAnimator();
		if (anim)
			anim->AddClip(RunClip);
	}

	auto* ctrl = EnsureAnimController();
	ctrl->SetIdleClip("Idle");
	ctrl->SetMoveClip("Run");
	ctrl->SetSpeed(0.0f);
	ctrl->Update(0.0f);

	this->Animate(0.0f);

	// ------------------------------------------------------------
	// 대표 materialId 선택 (기존 로직 유지하되 Fighter 기준)
	// ------------------------------------------------------------
	UINT playerMaterialId = 0;
	if (pPlayerMesh)
	{
		for (auto& sm : pPlayerMesh->m_SubMeshes)
		{
			if (sm.materialName.empty()) continue;
			if (sm.materialId == 0xFFFFFFFFu) continue;
			playerMaterialId = sm.materialId;
			break;
		}
	}
	m_nPlayerMaterialID = playerMaterialId;

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

	shared_ptr<CPlayerShader> pShader = make_shared<CPlayerShader>();
	pShader->CreateShader(pd3dDevice, pd3dGraphicsRootSignature, 5, rtvFormats, DXGI_FORMAT_D24_UNORM_S8_UINT);

	pShader->CreateShaderVariables(pd3dDevice, pd3dCommandList);

	// Player CBV 핸들 생성
	D3D12_GPU_DESCRIPTOR_HANDLE d3dCbvGPUDescriptorHandle =
		CScene::m_pDescriptorHeap->CreateConstantBufferView(
			pd3dDevice,
			m_pd3dcbPlayer.Get(),
			ncbElementBytes
		);

	SetCbvGPUDescriptorHandle(d3dCbvGPUDescriptorHandle);
	SetShader(pShader);

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
	m_pCamera->SetPosition(Vector3::Add(m_xmf3Position, m_pCamera->GetOffset()));
	Update(fTimeElapsed);

	return(m_pCamera.get());
}
