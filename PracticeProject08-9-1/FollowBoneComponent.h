//-----------------------------------------------------------------------------
// File: FollowBoneComponent.h
//-----------------------------------------------------------------------------
#pragma once

#include "Component.h"

class CFollowBoneComponent final : public CComponentT<CFollowBoneComponent>
{
public:
    explicit CFollowBoneComponent(CGameObject* owner);

    void Bind(CGameObject* target, const std::string& boneName, const XMFLOAT4X4& localOffset);
    void Clear();

    CGameObject* GetTarget() const { return m_pTarget; }
    const std::string& GetBoneName() const { return m_boneName; }
    int GetBoneIndex() const { return m_boneIndex; }
    const XMFLOAT4X4& GetLocalOffset() const { return m_localOffset; }

    bool IsBound() const { return (m_pTarget != nullptr) && !m_boneName.empty(); }

    void SnapNow();
    void OnLateUpdate(float dt) override;

private:
    bool ResolveBoneIndex();
    bool TryBuildFollowerWorld(XMFLOAT4X4& outWorld);

private:
    CGameObject* m_pTarget = nullptr;
    std::string  m_boneName;
    int          m_boneIndex = -1;
    bool         m_boneResolved = false;
    XMFLOAT4X4   m_localOffset{};
};