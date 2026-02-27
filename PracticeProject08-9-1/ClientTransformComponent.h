#pragma once
#include "BaseComponent.h"
#include "stdafx.h"


using namespace DirectX;

// forward declarations (include 최소화)
class CGameObject;
struct ID3D12Device;
struct ID3D12GraphicsCommandList;


class ClientTransformComponent final : public CComponentT<ClientTransformComponent>
{
public:
	explicit ClientTransformComponent(CGameObject* owner)
		: CComponentT(owner)
	{
		// OnCreate 이전에도 안전하게 값이 유효하도록 기본값 세팅
		XMStoreFloat4x4(&worldMatrix, XMMatrixIdentity());
		rotation = XMFLOAT4(0, 0, 0, 1);
		position = XMFLOAT3(0, 0, 0);
		direction = XMFLOAT3(0, 0, 1);
		scale = XMFLOAT3(1, 1, 1);
	}

	// 요구된 데이터
	XMFLOAT3  position = XMFLOAT3(0, 0, 0);
	XMFLOAT3  direction = XMFLOAT3(0, 0, 1);   // look(forward)
	XMFLOAT3  scale = XMFLOAT3(1, 1, 1);
	XMFLOAT4X4 worldMatrix = {};

	// 내부 회전(쿼터니언) - 규칙에 없지만 구현을 위해 내부 보유
	XMFLOAT4 rotation = XMFLOAT4(0, 0, 0, 1);  // (x,y,z,w)

	// --------------------
	// Lifecycle
	// --------------------
	void OnCreate(ID3D12Device*, ID3D12GraphicsCommandList*) override
	{
		RebuildWorld();
	}

	// --------------------
	// Basic setters
	// --------------------
	void SetPosition(const XMFLOAT3& p) { position = p; RebuildWorld(); }
	void Translate(const XMFLOAT3& d) { position.x += d.x; position.y += d.y; position.z += d.z; RebuildWorld(); }
	void SetScale(const XMFLOAT3& s) { scale = s; RebuildWorld(); }
	const XMFLOAT4X4& GetWorldMatrix() const { return worldMatrix; }

	// --------------------
	// Orientation API
	// --------------------
	XMFLOAT3 GetLook() const { return direction; }

	XMFLOAT3 GetRight() const
	{
		XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&rotation));
		XMVECTOR v = XMVector3TransformNormal(XMVectorSet(1, 0, 0, 0), R);
		XMFLOAT3 out; XMStoreFloat3(&out, XMVector3Normalize(v));
		return out;
	}

	XMFLOAT3 GetUp() const
	{
		XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&rotation));
		XMVECTOR v = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), R);
		XMFLOAT3 out; XMStoreFloat3(&out, XMVector3Normalize(v));
		return out;
	}

	void SetLookDirection(const XMFLOAT3& lookWorld, const XMFLOAT3& upHintWorld = XMFLOAT3(0, 1, 0))
	{
		XMVECTOR f = XMVector3Normalize(XMLoadFloat3(&lookWorld));
		XMVECTOR up = XMVector3Normalize(XMLoadFloat3(&upHintWorld));

		// up과 f가 거의 평행이면 다른 up을 사용
		const float d = fabsf(XMVectorGetX(XMVector3Dot(f, up)));
		if (d > 0.99f) up = XMVectorSet(0, 0, 1, 0);

		XMVECTOR r = XMVector3Normalize(XMVector3Cross(up, f));
		XMVECTOR u = XMVector3Normalize(XMVector3Cross(f, r));

		XMFLOAT3 right, upv, look;
		XMStoreFloat3(&right, r);
		XMStoreFloat3(&upv, u);
		XMStoreFloat3(&look, f);

		SetRotationFromBasis(right, upv, look);
	}

	// yaw only (Player 바디 회전용): world up 기준 절대 yaw
	void SetYawDegrees(float yawDeg)
	{
		const float yaw = XMConvertToRadians(yawDeg);
		XMVECTOR q = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), yaw);
		XMStoreFloat4(&rotation, XMQuaternionNormalize(q));
		RebuildWorld();
	}

	// world axis 기준 회전(증분)
	void RotateWorldAxisDegrees(const XMFLOAT3& axisWorld, float deg)
	{
		XMVECTOR axis = XMVector3Normalize(XMLoadFloat3(&axisWorld));
		XMVECTOR dq = XMQuaternionRotationAxis(axis, XMConvertToRadians(deg));
		XMVECTOR q = XMQuaternionMultiply(dq, XMLoadFloat4(&rotation)); // pre-multiply
		XMStoreFloat4(&rotation, XMQuaternionNormalize(q));
		RebuildWorld();
	}

	void RotateWorldEulerDegrees(float pitchDeg, float yawDeg, float rollDeg)
	{
		XMVECTOR dq = XMQuaternionRotationRollPitchYaw(
			XMConvertToRadians(pitchDeg),
			XMConvertToRadians(yawDeg),
			XMConvertToRadians(rollDeg)
		);

		// world-pre-multiply: dq * q
		XMVECTOR q = XMQuaternionMultiply(dq, XMLoadFloat4(&rotation));
		XMStoreFloat4(&rotation, XMQuaternionNormalize(q));
		RebuildWorld();
	}

	// 카메라 basis를 트랜스폼 회전으로 세팅(스페이스쉽 모드 진입 시)
	void SetRotationFromBasis(const XMFLOAT3& right, const XMFLOAT3& up, const XMFLOAT3& look)
	{
		// row-major/column-major 이슈를 피하려고 “축을 행”으로 구성 (DirectXMath는 row-major 저장이지만 XM은 행렬 의미 일관)
		XMMATRIX M =
			XMMATRIX(
				right.x, right.y, right.z, 0.0f,
				up.x, up.y, up.z, 0.0f,
				look.x, look.y, look.z, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			);

		XMVECTOR q = XMQuaternionRotationMatrix(M);
		XMStoreFloat4(&rotation, XMQuaternionNormalize(q));
		RebuildWorld();
	}

	// 외부에서 WorldMatrix를 직접 넣어야 할 때(호환용)
	void SetWorldMatrixFromMatrix(const XMFLOAT4X4& m)
	{
		XMMATRIX W = XMLoadFloat4x4(&m);

		XMVECTOR S, R, T;
		if (XMMatrixDecompose(&S, &R, &T, W))
		{
			XMFLOAT3 s; XMStoreFloat3(&s, S);
			XMFLOAT4 r; XMStoreFloat4(&r, R);
			XMFLOAT3 t; XMStoreFloat3(&t, T);

			scale = s;
			rotation = r;
			position = t;

			RebuildWorld();
		}
		else
		{
			// 분해 실패 시 최소한 행렬만 보관
			worldMatrix = m;
			// direction은 그대로 둠
		}
	}

private:
	void RebuildWorld()
	{
		XMMATRIX S = XMMatrixScaling(scale.x, scale.y, scale.z);
		XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&rotation));
		XMMATRIX T = XMMatrixTranslation(position.x, position.y, position.z);

		XMMATRIX W = S * R * T;
		XMStoreFloat4x4(&worldMatrix, W);

		// direction 갱신 (forward)
		XMVECTOR f = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), R);
		XMFLOAT3 d; XMStoreFloat3(&d, XMVector3Normalize(f));
		direction = d;
	}
};