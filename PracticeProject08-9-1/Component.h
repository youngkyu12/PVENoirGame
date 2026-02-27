//=============================================================================
// Component.h
//=============================================================================
#pragma once

#include <cstdint>
#include "stdafx.h"
#include "BaseComponent.h"   // 여기서 CComponent/CComponentT를 가져온다

using namespace DirectX;

class CGameObject;

// 클라 전용 DX Transform은 유지 (기존 코드 그대로)
//-----------------------------------------------------------------------------
// Client DX Transform Component (Object.h / PlayerControllerComponent 등이 기대하는 API 포함)
//-----------------------------------------------------------------------------

class CTransformComponent final : public CComponentT<CTransformComponent>
{
public:
    explicit CTransformComponent(CGameObject* owner)
        : CComponentT(owner)
    {
        XMStoreFloat4x4(&worldMatrix, XMMatrixIdentity());
        rotation = XMFLOAT4(0, 0, 0, 1);   // quaternion
        position = XMFLOAT3(0, 0, 0);
        direction = XMFLOAT3(0, 0, 1);
        scale = XMFLOAT3(1, 1, 1);
    }

    // authoritative state
    XMFLOAT3   position = XMFLOAT3(0, 0, 0);
    XMFLOAT3   direction = XMFLOAT3(0, 0, 1);   // forward (cached)
    XMFLOAT3   scale = XMFLOAT3(1, 1, 1);
    XMFLOAT4X4 worldMatrix = {};
    XMFLOAT4   rotation = XMFLOAT4(0, 0, 0, 1); // quaternion (x,y,z,w)

    // lifecycle
    void OnCreate(ID3D12Device*, ID3D12GraphicsCommandList*) override
    {
        RebuildWorld();
    }

    // basic setters
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

    // yaw only (absolute)
    void SetYawDegrees(float yawDeg)
    {
        const float yaw = XMConvertToRadians(yawDeg);
        XMVECTOR q = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), yaw);
        XMStoreFloat4(&rotation, XMQuaternionNormalize(q));
        RebuildWorld();
    }

    // world axis incremental rotation (pre-multiply)
    void RotateWorldAxisDegrees(const XMFLOAT3& axisWorld, float deg)
    {
        XMVECTOR axis = XMVector3Normalize(XMLoadFloat3(&axisWorld));
        XMVECTOR dq = XMQuaternionRotationAxis(axis, XMConvertToRadians(deg));
        XMVECTOR q = XMQuaternionMultiply(dq, XMLoadFloat4(&rotation)); // dq * q
        XMStoreFloat4(&rotation, XMQuaternionNormalize(q));
        RebuildWorld();
    }

    // world euler incremental rotation (pre-multiply)
    void RotateWorldEulerDegrees(float pitchDeg, float yawDeg, float rollDeg)
    {
        XMVECTOR dq = XMQuaternionRotationRollPitchYaw(
            XMConvertToRadians(pitchDeg),
            XMConvertToRadians(yawDeg),
            XMConvertToRadians(rollDeg)
        );

        XMVECTOR q = XMQuaternionMultiply(dq, XMLoadFloat4(&rotation)); // dq * q
        XMStoreFloat4(&rotation, XMQuaternionNormalize(q));
        RebuildWorld();
    }

    // look 방향을 기준으로 회전 세팅
    void SetLookDirection(const XMFLOAT3& lookWorld, const XMFLOAT3& upHintWorld = XMFLOAT3(0, 1, 0))
    {
        XMVECTOR f = XMVector3Normalize(XMLoadFloat3(&lookWorld));
        XMVECTOR up = XMVector3Normalize(XMLoadFloat3(&upHintWorld));

        // up과 forward가 거의 평행하면 다른 up을 사용
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

    // basis(right/up/look)로 회전 세팅
    void SetRotationFromBasis(const XMFLOAT3& right, const XMFLOAT3& up, const XMFLOAT3& look)
    {
        // DirectXMath: 행렬 의미는 row-major로 일관되게 사용
        XMMATRIX M(
            right.x, right.y, right.z, 0.0f,
            up.x, up.y, up.z, 0.0f,
            look.x, look.y, look.z, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        );

        XMVECTOR q = XMQuaternionRotationMatrix(M);
        XMStoreFloat4(&rotation, XMQuaternionNormalize(q));
        RebuildWorld();
    }

    // external world matrix -> decompose and apply
    void SetWorldMatrixFromMatrix(const XMFLOAT4X4& m)
    {
        XMMATRIX W = XMLoadFloat4x4(&m);

        XMVECTOR S, R, T;
        if (XMMatrixDecompose(&S, &R, &T, W))
        {
            XMStoreFloat3(&scale, S);
            XMStoreFloat4(&rotation, R);
            XMStoreFloat3(&position, T);
            RebuildWorld();
        }
        else
        {
            worldMatrix = m; // fallback: store only
            // direction은 유지(원하면 여기서 재계산 가능)
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

        // cached forward(direction)
        XMVECTOR f = XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), R);
        XMFLOAT3 d; XMStoreFloat3(&d, XMVector3Normalize(f));
        direction = d;
    }
};