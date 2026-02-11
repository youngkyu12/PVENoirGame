//------------------------------------------------------- ----------------------
// File: AnimatorComponent.h
//-----------------------------------------------------------------------------

#pragma once

#include <string>
#include "Component.h"

// forward
class CAnimator;
class CAnimController;
struct AnimationClip;

// ============================================================================
// Animator Component
//  - "소유"는 CGameObject에 유지 (m_pAnimator / m_pAnimController)
//  - 이 컴포넌트는 매 프레임 애니메이션 갱신 + bone palette 업로드만 담당
// ============================================================================
class CAnimatorComponent final : public CComponentT<CAnimatorComponent>
{
public:
    explicit CAnimatorComponent(CGameObject* owner) : CComponentT(owner) {}

    // build-time warmup (필수는 아님)
    void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;

    // per-frame tick
    void OnUpdate(float dt) override;

public:
    // convenience wrappers
    CAnimator* EnsureAnimator();
    CAnimController* EnsureController();

    void AddClip(const AnimationClip& clip);
    void Play(const std::string& clipName, bool loop = true, float start = 0.0f);

    void SetIdleClip(const std::string& name);
    void SetMoveClip(const std::string& name);
    void SetSpeed(float s);

    // ctor 등에서 0프레임 즉시 평가하고 싶을 때
    void EvaluatePose(float dt = 0.0f);

private:
    void UploadBonePaletteIfSkinned();
};
