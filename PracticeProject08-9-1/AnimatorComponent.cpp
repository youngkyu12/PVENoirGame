//-----------------------------------------------------------------------------
// File: AnimatorComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "AnimatorComponent.h"
#include "Object.h"
#include "Animator.h"
#include "AnimController.h"
#include "SkinningComponent.h"

CAnimatorComponent::CAnimatorComponent(CGameObject* owner)
    : CComponentT<CAnimatorComponent>(owner)
{
}

void CAnimatorComponent::OnCreate(ID3D12Device*, ID3D12GraphicsCommandList*)
{
    // 메시는 보통 ctor에서 SetMesh가 끝난 뒤 CreateComponents가 호출되므로
    // 여기서 스켈레톤 바인딩을 시도한다.
    EnsureAnimator();
    EnsureController();
    SyncSkeletonIfPossible();
}

void CAnimatorComponent::OnUpdate(float dt)
{
    // 1) controller/state update + anim update까지만
    CGameObject* owner = GetOwner();
    if (!owner) return;

    CAnimator* anim = EnsureAnimator();
    if (!anim) return;

    EnsureController();
    SyncSkeletonIfPossible();

    if (m_pController) m_pController->Update(dt);
    anim->Update(dt);
}

void CAnimatorComponent::OnLateUpdate(float /*dt*/)
{
    // 2) 최종 결과 업로드만
    UploadIfSkinned();
}


CAnimator* CAnimatorComponent::EnsureAnimator()
{
    if (!m_pAnimator)
        m_pAnimator = std::make_unique<CAnimator>();

    // 스켈레톤은 메시 준비 이후에만 성공하므로, 필요할 때마다 시도
    SyncSkeletonIfPossible();
    return m_pAnimator.get();
}

CAnimController* CAnimatorComponent::EnsureController()
{
    if (!m_pController)
        m_pController = std::make_unique<CAnimController>(GetOwner());
    return m_pController.get();
}

void CAnimatorComponent::AddClip(const AnimationClip& clip)
{
    CAnimator* anim = EnsureAnimator();
    if (!anim) return;

    // bones/map 준비된 경우에만 SetSkeleton 성공
    SyncSkeletonIfPossible();
    anim->AddClip(clip);
}

bool CAnimatorComponent::HasClip(const char* name) const
{
    if (!m_pAnimator) return false;
    return m_pAnimator->HasClip(name);
}

void CAnimatorComponent::SetSpeed(float s)
{
    EnsureController();
    if (m_pController) m_pController->SetSpeed(s);
}

void CAnimatorComponent::SetIdleClip(const char* name)
{
    EnsureController();
    if (m_pController) m_pController->SetIdleClip(name);
}

void CAnimatorComponent::SetMoveClip(const char* name)
{
    EnsureController();
    if (m_pController) m_pController->SetMoveClip(name);
}

void CAnimatorComponent::EvaluatePose(float dt)
{
    OnUpdate(dt);
    OnLateUpdate(dt);
}

void CAnimatorComponent::SyncSkeletonIfPossible()
{
    if (m_bSkeletonBound) return;
    if (!m_pAnimator) return;

    CGameObject* owner = GetOwner();
    if (!owner) return;

    const auto& bones = owner->GetBones();
    const auto& map = owner->GetBoneNameToIndex();
    if (!bones.empty() && !map.empty())
    {
        m_pAnimator->SetSkeleton(bones, map);
        m_bSkeletonBound = true;
    }
}

void CAnimatorComponent::UploadIfSkinned()
{
    CGameObject* owner = GetOwner();
    if (!owner || !m_pAnimator) return;

    auto* skin = owner->GetComponent<CSkinningComponent>();
    if (!skin || !skin->IsSkinned()) return;

    const auto& mats = m_pAnimator->GetFinalBoneMatrices();
    if (!mats.empty())
        skin->Upload(mats.data(), (int)mats.size());
}

bool CAnimatorComponent::Play(const std::string& name, bool loop, float start)
{
    CAnimator* anim = EnsureAnimator();
    if (!anim) return false;

    SyncSkeletonIfPossible();
    return anim->Play(name, loop, start);
}

bool CAnimatorComponent::CrossFade(const std::string& name, float blendTime, bool loop, float start)
{
    CAnimator* anim = EnsureAnimator();
    if (!anim) return false;

    SyncSkeletonIfPossible();
    return anim->CrossFade(name.c_str(), blendTime, loop, start); // 네 Animator 시그니처에 맞춰 조정
}

void CAnimatorComponent::InvalidateSkeleton()
{
    m_bSkeletonBound = false;
    // anim이 있으면 다음 SyncSkeletonIfPossible에서 다시 SetSkeleton 시도
}
