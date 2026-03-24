//-----------------------------------------------------------------------------
// File: AnimatorComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "AnimatorComponent.h"
#include "Object.h"
#include "Animator.h"
#include "AnimController.h"
#include "SkinningComponent.h"
#include "MonsterAnimController.h"

CAnimatorComponent::CAnimatorComponent(CGameObject* owner)
    : CComponentT<CAnimatorComponent>(owner)
{
}

CAnimatorComponent::~CAnimatorComponent() = default;

void CAnimatorComponent::OnCreate(ID3D12Device*, ID3D12GraphicsCommandList*)
{
    EnsureAnimator();
    EnsureController();
    SyncSkeletonIfPossible();
}

void CAnimatorComponent::OnUpdate(float dt)
{
    // 1) controller/state update + anim update
    CGameObject* owner = GetOwner();
    if (!owner) return;

    CAnimator* anim = EnsureAnimator();
    if (!anim) return;

    EnsureController();
    SyncSkeletonIfPossible();

	if ( m_pController ) m_pController->Update(dt);
	if ( m_pMonsterController ) m_pMonsterController->Update(dt);
	anim->Update(dt);
}

void CAnimatorComponent::OnLateUpdate(float /*dt*/)
{
    UploadIfSkinned();
}


CAnimator* CAnimatorComponent::EnsureAnimator()
{
    if (!m_pAnimator)
        m_pAnimator = std::make_unique<CAnimator>();

    SyncSkeletonIfPossible();
    return m_pAnimator.get();
}

CAnimController* CAnimatorComponent::EnsureController()
{
    if (!m_pController)
        m_pController = std::make_unique<CAnimController>(GetOwner());
    return m_pController.get();
}
CMonsterAnimController* CAnimatorComponent::EnsureMonsterController()
{
	if ( !m_pMonsterController )
		m_pMonsterController = std::make_unique<CMonsterAnimController>(GetOwner());
	return m_pMonsterController.get();
}

void CAnimatorComponent::AddClip(const AnimationClip& clip)
{
    CAnimator* anim = EnsureAnimator();
    if (!anim) return;

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
    return anim->CrossFade(name.c_str(), blendTime, loop, start);
}

void CAnimatorComponent::InvalidateSkeleton()
{
    m_bSkeletonBound = false;
}
