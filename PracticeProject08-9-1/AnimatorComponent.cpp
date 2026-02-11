//------------------------------------------------------- ----------------------
// File: AnimatorComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "AnimatorComponent.h"

#include "Object.h"
#include "Animator.h"
#include "AnimController.h"
#include "AnimatorData.h"

CAnimator* CAnimatorComponent::EnsureAnimator()
{
    CGameObject* owner = GetOwner();
    return owner ? owner->EnsureAnimator() : nullptr;
}

CAnimController* CAnimatorComponent::EnsureController()
{
    CGameObject* owner = GetOwner();
    return owner ? owner->EnsureAnimController() : nullptr;
}

void CAnimatorComponent::AddClip(const AnimationClip& clip)
{
    if (CAnimator* anim = EnsureAnimator())
        anim->AddClip(clip);
}

void CAnimatorComponent::Play(const std::string& clipName, bool loop, float start)
{
    CGameObject* owner = GetOwner();
    if (!owner) return;

    owner->PlayAnimation(clipName, loop, start);
}

void CAnimatorComponent::SetIdleClip(const std::string& name)
{
    if (CAnimController* ctrl = EnsureController())
        ctrl->SetIdleClip(name);
}

void CAnimatorComponent::SetMoveClip(const std::string& name)
{
    if (CAnimController* ctrl = EnsureController())
        ctrl->SetMoveClip(name);
}

void CAnimatorComponent::SetSpeed(float s)
{
    if (CAnimController* ctrl = EnsureController())
        ctrl->SetSpeed(s);
}

void CAnimatorComponent::UploadBonePaletteIfSkinned()
{
    CGameObject* owner = GetOwner();
    if (!owner) return;

    if (!owner->IsSkinnedObject())
        return;

    CAnimator* anim = owner->GetAnimator();
    if (!anim) return;

    const auto& mats = anim->GetFinalBoneMatrices();
    if (mats.empty())
        return;

    owner->UpdateBoneTransformsOnGPU(mats.data(), (int)mats.size());
}

void CAnimatorComponent::EvaluatePose(float dt)
{
    CGameObject* owner = GetOwner();
    if (!owner) return;

    // 1) 상태 결정(Idle/Run)
    if (CAnimController* ctrl = owner->GetAnimController())
        ctrl->Update(dt);

    // 2) 포즈 계산
    if (CAnimator* anim = owner->GetAnimator())
    {
        anim->Update(dt);

        // 3) 스키닝이면 GPU 업로드
        UploadBonePaletteIfSkinned();
    }
}

void CAnimatorComponent::OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
    (void)dev; (void)cmd;

    // skeleton/ctrl이 아직 없을 수도 있으니 준비만 해둠
    EnsureAnimator();
    EnsureController();
}

void CAnimatorComponent::OnUpdate(float dt)
{
    // 매 프레임 애니 갱신은 여기서 책임진다
    EvaluatePose(dt);
}
