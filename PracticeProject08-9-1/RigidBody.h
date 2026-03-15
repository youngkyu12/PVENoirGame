#pragma once
#include "Component.h"
#include <vector>
#include <unordered_map>
#include <string>

class CTransformComponent;

class CRigidBody final : public CComponentT<CRigidBody>
{
public:
    explicit CRigidBody(CGameObject* owner);

//    void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;
//    void OnUpdate(float dt) override;
//
//    // Mass
//    void SetMass(float m);
//    float GetMass() const { return mMass; }
//    float GetInvMass() const { return mInvMass; }
//    bool  IsStatic() const { return mInvMass == 0.0f; }
//
//    // Forces
//    void AddForce(const DirectX::XMFLOAT3& f);
//    void AddForce(float x, float y, float z) { AddForce(DirectX::XMFLOAT3{ x,y,z }); }
//    void ClearForces();
//
//    // State access
//    const DirectX::XMFLOAT3& GetVelocity() const { return mVelocity; }
//    const DirectX::XMFLOAT3& GetAcceleration() const { return mAcceleration; }
//    const DirectX::XMFLOAT3& GetAccumForce() const { return mForceAccum; }
//
//    void SetVelocity(const DirectX::XMFLOAT3& v) { mVelocity = v; }
//
//    // Optional basic params
//    void SetLinearDamping(float d) { mLinearDamping = d; } // 0~1 권장 (예: 0.02)
//    void SetUseGravity(bool v) { mUseGravity = v; }
//    void SetGravity(const DirectX::XMFLOAT3& g) { mGravity = g; }
//
//private:
//    void Integrate(float dt);

private:
    CTransformComponent* mTransform = nullptr;

    float mMass = 1.0f;

    XMFLOAT3 mVelocity = { 0,0,0 };
    XMFLOAT3 mAcceleration = { 0,0,0 };   // a = F * invMass
    vector<XMFLOAT3> mForceAccum;   // 누적 힘

    // MVP 옵션들
    bool mUseGravity = true;
    XMFLOAT3 mGravity = { 0.0f, -9.81f, 0.0f };

    float mLinearDamping = 0.02f; // 속도 감쇠(공기저항 느낌)
};