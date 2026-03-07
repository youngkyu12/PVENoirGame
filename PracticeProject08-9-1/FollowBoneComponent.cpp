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

void CFollowBoneComponent::Bind(CGameObject* target, int boneIndex, const XMFLOAT4X4& localOffset)
{
    m_pTarget = target;
    m_boneIndex = boneIndex;
    m_localOffset = localOffset;
}

void CFollowBoneComponent::Clear()
{
    m_pTarget = nullptr;
    m_boneIndex = -1;
    XMStoreFloat4x4(&m_localOffset, XMMatrixIdentity());
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

bool CFollowBoneComponent::TryBuildFollowerWorld(XMFLOAT4X4& outWorld) const
{
    if (!m_pTarget) return false;
    if (m_boneIndex < 0) return false;

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