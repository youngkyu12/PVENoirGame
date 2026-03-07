//-----------------------------------------------------------------------------
// File: FollowBoneComponent.h
//-----------------------------------------------------------------------------
#pragma once

#include "Component.h"

class CFollowBoneComponent final : public CComponentT<CFollowBoneComponent>
{
public:
    explicit CFollowBoneComponent(CGameObject* owner);

    void Bind(CGameObject* target, int boneIndex, const XMFLOAT4X4& localOffset);
    void Clear();

    CGameObject* GetTarget() const { return m_pTarget; }
    int GetBoneIndex() const { return m_boneIndex; }
    const XMFLOAT4X4& GetLocalOffset() const { return m_localOffset; }

    bool IsBound() const { return (m_pTarget != nullptr) && (m_boneIndex >= 0); }

    void SnapNow();
    void OnLateUpdate(float dt) override;

private:
    bool TryBuildFollowerWorld(XMFLOAT4X4& outWorld) const;

private:
    CGameObject* m_pTarget = nullptr;
    int          m_boneIndex = -1;
    XMFLOAT4X4   m_localOffset{};
};