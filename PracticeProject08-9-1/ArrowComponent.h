//-----------------------------------------------------------------------------
// File: ArrowComponent.h
//-----------------------------------------------------------------------------

#pragma once

#include "Component.h"
#include <DirectXMath.h>

using namespace DirectX;

class CGameObject;

// Transform만으로 이동/수명 관리하는 단순 화살 컴포넌트
class CArrowComponent final : public CComponentT<CArrowComponent>
{
public:
    explicit CArrowComponent(CGameObject* owner);

    // 활성화(발사)
    void Activate(const XMFLOAT3& position, const XMFLOAT3& velocity, float lifeSec);

    // 비활성화(풀로 복귀)
    void Deactivate();

    bool IsActive() const { return m_active; }

    void OnUpdate(float dt) override;

private:
    bool     m_active = false;
    float    m_lifeRemaining = 0.0f;
    XMFLOAT3 m_velocity = { 0.0f, 0.0f, 0.0f };
};