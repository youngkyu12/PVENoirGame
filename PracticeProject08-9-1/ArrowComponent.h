#pragma once

#include "Component.h"
#include <DirectXMath.h>

using namespace DirectX;

class CGameObject;

class CArrowComponent final : public CComponentT<CArrowComponent>
{
public:
    explicit CArrowComponent(CGameObject* owner);

    // 즉시 발사 상태로 활성화(기존 호환)
    void Activate(const XMFLOAT3& position, const XMFLOAT3& velocity, float lifeSec);

    // 활에 걸린 상태로 준비
	void Prepare(
	CGameObject* bowObject,
	CGameObject* directionSource,
	float pullBackDistance,
	bool enableCollisionOnLaunch = true
	);

	// 준비된 화살을 실제 발사
	void Launch(
		const XMFLOAT3& velocity,
		float lifeSec,
		bool enableCollision = true
	);

    void Deactivate();

    bool IsActive() const { return m_state != EState::Inactive; }
    bool IsPrepared() const { return m_state == EState::Prepared; }
    bool IsFlying() const { return m_state == EState::Flying; }

    void OnUpdate(float dt) override;

private:
    enum class EState : uint8_t
    {
        Inactive = 0,
        Prepared,
        Flying
    };

private:
    static XMFLOAT3 GetForwardFromObject(const CGameObject* obj);
    static XMFLOAT3 NormalizeSafe(const XMFLOAT3& v);

private:
    EState   m_state = EState::Inactive;
    float    m_lifeRemaining = 0.0f;
    XMFLOAT3 m_velocity = { 0.0f, 0.0f, 0.0f };

    CGameObject* m_bowObject = nullptr;         // 따라갈 활
    CGameObject* m_directionSource = nullptr;   // 방향 기준(플레이어)
    float        m_pullBackDistance = 0.0f;
	bool         m_enableCollisionOnLaunch = true;
};