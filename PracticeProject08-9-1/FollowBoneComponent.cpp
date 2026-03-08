//-----------------------------------------------------------------------------
// File: FollowBoneComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "FollowBoneComponent.h"

#include "Object.h"
#include "Animator.h"

using namespace DirectX;

CFollowBoneComponent::CFollowBoneComponent(CGameObject* owner)
    : CComponentT<CFollowBoneComponent>(owner)
{
    XMStoreFloat4x4(&m_localOffset, XMMatrixIdentity());
}

void CFollowBoneComponent::Bind(CGameObject* target, const std::string& boneName, const XMFLOAT4X4& localOffset)
{
    m_pTarget = target;
    m_boneName = boneName;
    m_boneIndex = -1;
    m_boneResolved = false;
    m_localOffset = localOffset;
}

void CFollowBoneComponent::Clear()
{
    m_pTarget = nullptr;
    m_boneName.clear();
    m_boneIndex = -1;
    m_boneResolved = false;
    XMStoreFloat4x4(&m_localOffset, XMMatrixIdentity());
}

bool CFollowBoneComponent::ResolveBoneIndex()
{
    if (!m_pTarget) return false;
    if (m_boneName.empty()) return false;

    if (m_boneResolved && m_boneIndex >= 0)
        return true;

    const auto& boneMap = m_pTarget->GetBoneNameToIndex();
    auto it = boneMap.find(m_boneName);
    if (it == boneMap.end())
        return false;

    m_boneIndex = it->second;
    m_boneResolved = true;
    return true;
}

void CFollowBoneComponent::SnapNow()
{
    CGameObject* owner = GetOwner();
    if (!owner) return;

    XMFLOAT4X4 world{};
    if (!TryBuildFollowerWorld(world)) return;

    owner->SetWorldMatrix(world);
}

void CFollowBoneComponent::OnLateUpdate(float /*dt*/)
{
    SnapNow();
}

bool CFollowBoneComponent::TryBuildFollowerWorld(XMFLOAT4X4& outWorld)
{
    if (!m_pTarget) return false;
    if (!ResolveBoneIndex()) return false;

    CAnimator* anim = m_pTarget->GetAnimator();
    if (!anim) return false;

    XMFLOAT4X4 boneGlobal{};
    if (!anim->GetGlobalBoneMatrix(m_boneIndex, boneGlobal))
        return false;

    const XMFLOAT4X4& targetWorld = m_pTarget->GetWorldMatrix();

    XMMATRIX Moff = XMLoadFloat4x4(&m_localOffset);
    XMMATRIX Mbone = XMLoadFloat4x4(&boneGlobal);
    XMMATRIX Mtarget = XMLoadFloat4x4(&targetWorld);

    // 엔진 규칙: childLocal * parent
    XMMATRIX W = Moff * Mbone * Mtarget;
    XMStoreFloat4x4(&outWorld, W);
    return true;
}