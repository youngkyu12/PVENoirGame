#include "stdafx.h"
#include "Camera.h"
#include "Player.h"

void CCamera::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	XMFLOAT4X4 xmf4x4View;
	XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4View)));

	//루트 파라메터 인덱스 1의
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4View, 0);

	XMFLOAT4X4 xmf4x4Projection;
	XMStoreFloat4x4(&xmf4x4Projection, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4Projection)));
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4Projection, 16);
}

void CCamera::SetViewportsAndScissorRects(ID3D12GraphicsCommandList* pd3dCommandList)
{
	pd3dCommandList->RSSetViewports(1, &m_d3dViewport);
	pd3dCommandList->RSSetScissorRects(1, &m_d3dScissorRect);
}

void CSpaceShipCamera::Rotate(float fPitch, float fYaw, float fRoll)
{
	if (m_pPlayer && (fPitch != 0.0f)) {
		//플레이어의 로컬 x-축에 대한 x 각도의 회전 행렬을 계산한다.
		XMFLOAT3 xmf3Right = m_pPlayer->GetRightVector();
		XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&xmf3Right), XMConvertToRadians(fPitch));

		//카메라의 로컬 x-축, y-축, z-축을 회전한다.
		m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, xmmtxRotate);
		m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, xmmtxRotate);
		m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, xmmtxRotate);

		//카메라의 위치 벡터에서 플레이어의 위치 벡터를 뺀다. 결과는 플레이어 위치를 기준(원점)으로 한 카메라의 위치 벡터이다.
		m_xmf3Position = Vector3::Subtract(m_xmf3Position, m_pPlayer->GetPosition());

		//플레이어의 위치를 중심으로 카메라의 위치 벡터(플레이어를 기준으로 한)를 회전한다.
		m_xmf3Position = Vector3::TransformCoord(m_xmf3Position, xmmtxRotate);

		//회전시킨 카메라의 위치 벡터에 플레이어의 위치를 더하여 카메라의 위치 벡터를 구한다.
		m_xmf3Position = Vector3::Add(m_xmf3Position, m_pPlayer->GetPosition());
	}

	if (m_pPlayer && (fYaw != 0.0f)) {
		XMFLOAT3 xmf3Up = m_pPlayer->GetUpVector();
		XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&xmf3Up), XMConvertToRadians(fYaw));

		m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, xmmtxRotate);
		m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, xmmtxRotate);
		m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, xmmtxRotate);

		m_xmf3Position = Vector3::Subtract(m_xmf3Position, m_pPlayer->GetPosition());

		m_xmf3Position = Vector3::TransformCoord(m_xmf3Position, xmmtxRotate);

		m_xmf3Position = Vector3::Add(m_xmf3Position, m_pPlayer->GetPosition());
	}

	if (m_pPlayer && (fRoll != 0.0f)) {
		XMFLOAT3 xmf3Look = m_pPlayer->GetLookVector();
		XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&xmf3Look), XMConvertToRadians(fRoll));

		m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, xmmtxRotate);
		m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, xmmtxRotate);
		m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, xmmtxRotate);

		m_xmf3Position = Vector3::Subtract(m_xmf3Position, m_pPlayer->GetPosition());

		m_xmf3Position = Vector3::TransformCoord(m_xmf3Position, xmmtxRotate);

		m_xmf3Position = Vector3::Add(m_xmf3Position, m_pPlayer->GetPosition());
	}
}

void CFirstPersonCamera::Rotate(float fPitch, float fYaw, float fRoll)
{
	if (fPitch != 0.0f) {
		//카메라의 로컬 x-축을 기준으로 회전하는 행렬을 생성한다. 사람의 경우 고개를 끄떡이는 동작이다.
		XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Right), XMConvertToRadians(fPitch));

		//카메라의 로컬 x-축, y-축, z-축을 회전 행렬을 사용하여 회전한다.
		m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, xmmtxRotate);
		m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, xmmtxRotate);
		m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, xmmtxRotate);
	}

	if (m_pPlayer && (fYaw != 0.0f)) {
		//플레이어의 로컬 y-축을 기준으로 회전하는 행렬을 생성한다.
		XMFLOAT3 xmf3Up = m_pPlayer->GetUpVector();
		XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&xmf3Up), XMConvertToRadians(fYaw));

		//카메라의 로컬 x-축, y-축, z-축을 회전 행렬을 사용하여 회전한다.
		m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, xmmtxRotate);
		m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, xmmtxRotate);
		m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, xmmtxRotate);
	}

	if (m_pPlayer && (fRoll != 0.0f)) {
		//플레이어의 로컬 z-축을 기준으로 회전하는 행렬을 생성한다.
		XMFLOAT3 xmf3Look = m_pPlayer->GetLookVector();
		XMMATRIX xmmtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&xmf3Look), XMConvertToRadians(fRoll));

		//카메라의 위치 벡터를 플레이어 좌표계로 표현한다(오프셋 벡터).
		m_xmf3Position = Vector3::Subtract(m_xmf3Position, m_pPlayer->GetPosition());
		//오프셋 벡터 벡터를 회전한다.
		m_xmf3Position = Vector3::TransformCoord(m_xmf3Position, xmmtxRotate);
		//회전한 카메라의 위치를 월드 좌표계로 표현한다.
		m_xmf3Position = Vector3::Add(m_xmf3Position, m_pPlayer->GetPosition());

		//카메라의 로컬 x-축, y-축, z-축을 회전한다.
		m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, xmmtxRotate);
		m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, xmmtxRotate);
		m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, xmmtxRotate);
	}
}

void CThirdPersonCamera::Update(XMFLOAT3& xmf3LookAt, float fTimeElapsed)
{
	//플레이어가 있으면 플레이어의 회전에 따라 3인칭 카메라도 회전해야 한다.
	if (m_pPlayer) {
		XMFLOAT4X4 xmf4x4Rotate = Matrix4x4::Identity();
		XMFLOAT3 xmf3Right = m_pPlayer->GetRightVector();
		XMFLOAT3 xmf3Up = m_pPlayer->GetUpVector();
		XMFLOAT3 xmf3Look = m_pPlayer->GetLookVector();

		//플레이어의 로컬 x-축, y-축, z-축 벡터로부터 회전 행렬(플레이어와 같은 방향을 나타내는 행렬)을 생성한다.
		xmf4x4Rotate._11 = xmf3Right.x; xmf4x4Rotate._21 = xmf3Up.x; xmf4x4Rotate._31 = xmf3Look.x;
		xmf4x4Rotate._12 = xmf3Right.y; xmf4x4Rotate._22 = xmf3Up.y; xmf4x4Rotate._32 = xmf3Look.y;
		xmf4x4Rotate._13 = xmf3Right.z; xmf4x4Rotate._23 = xmf3Up.z; xmf4x4Rotate._33 = xmf3Look.z;

		//카메라 오프셋 벡터를 회전 행렬로 변환(회전)한다.
		XMFLOAT3 xmf3Offset = Vector3::TransformCoord(m_xmf3Offset, xmf4x4Rotate);
		//회전한 카메라의 위치는 플레이어의 위치에 회전한 카메라 오프셋 벡터를 더한 것이다.
		XMFLOAT3 xmf3Position = Vector3::Add(m_pPlayer->GetPosition(), xmf3Offset);
		//현재의 카메라의 위치에서 회전한 카메라의 위치까지의 방향과 거리를 나타내는 벡터이다.
		XMFLOAT3 xmf3Direction = Vector3::Subtract(xmf3Position, m_xmf3Position);
		float fLength = Vector3::Length(xmf3Direction);
		xmf3Direction = Vector3::Normalize(xmf3Direction);

		// 3인칭 카메라의 래그(Lag)는 플레이어가 회전하더라도 카메라가 동시에 따라서 회전하지 않고 약간의 시차를 두고 
		// 회전하는 효과를 구현하기 위한 것이다. 
		// m_fTimeLag가 1보다 크면 fTimeLagScale이 작아지고 실제 회전(이동)이 적게 일어날 것이다. 
		// m_fTimeLag가 0이 아닌 경우 fTimeElapsed를 곱하고 있으므로 3인칭 카메라는 
		// 1초의 시간동안(1.0f / m_fTimeLag)의 비율만큼 플레이어의 회전을 따라가게 될 것이다.
		float fTimeLagScale = (m_fTimeLag) ? fTimeElapsed * (1.0f / m_fTimeLag) : 1.0f;
		float fDistance = fLength * fTimeLagScale;
		if (fDistance > fLength) fDistance = fLength;
		if (fLength < 0.01f) fDistance = fLength;
		if (fDistance > 0) {
			//실제로 카메라를 회전하지 않고 이동을 한다(회전의 각도가 작은 경우 회전 이동은 선형 이동과 거의 같다).
			m_xmf3Position = Vector3::Add(m_xmf3Position, xmf3Direction, fDistance);
			//카메라가 플레이어를 바라보도록 한다.
			SetLookAt(xmf3LookAt);
		}
	}
}

void CThirdPersonCamera::SetLookAt(XMFLOAT3& xmf3LookAt)
{
	//현재 카메라의 위치에서 플레이어를 바라보기 위한 카메라 변환 행렬을 생성한다.
	XMFLOAT4X4 mtxLookAt = Matrix4x4::LookAtLH(m_xmf3Position, xmf3LookAt, m_pPlayer->GetUpVector());
	//카메라 변환 행렬에서 카메라의 x-축, y-축, z-축을 구한다.
	m_xmf3Right = XMFLOAT3(mtxLookAt._11, mtxLookAt._21, mtxLookAt._31);
	m_xmf3Up = XMFLOAT3(mtxLookAt._12, mtxLookAt._22, mtxLookAt._32);
	m_xmf3Look = XMFLOAT3(mtxLookAt._13, mtxLookAt._23, mtxLookAt._33);
}

