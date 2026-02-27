//-----------------------------------------------------------------------------
// File: AnimatorComponent.h
//-----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <string>
#include "Component.h"

// forward
class CGameObject;
class CAnimator;
class CAnimController;
struct AnimationClip;

class CAnimatorComponent final : public CComponentT<CAnimatorComponent>
{
public:
    explicit CAnimatorComponent(CGameObject* owner);
    ~CAnimatorComponent() override;

    // ---- accessors ----
    CAnimator* GetAnimator() const { return m_pAnimator.get(); }
    CAnimController* GetController() const { return m_pController.get(); }

    // ---- ensure ----
    CAnimator* EnsureAnimator();
    CAnimController* EnsureController();

    // ---- clip/control wrappers ----
    void AddClip(const AnimationClip& clip);
    bool HasClip(const char* name) const;

    void SetSpeed(float s);
    void SetIdleClip(const char* name);
    void SetMoveClip(const char* name);

    // ctor/PreCreateComponents 단계에서도 강제 평가 가능
    void EvaluatePose(float dt);

    // lifecycle
    void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;
    void OnUpdate(float dt) override;
    void OnLateUpdate(float dt) override;

    // ---- play wrappers ----
    bool Play(const std::string& name, bool loop = true, float start = 0.0f);
    bool CrossFade(const std::string& name, float blendTime, bool loop = true, float start = 0.0f);

    // mesh 변경 등으로 skeleton 재바인딩이 필요할 때
    void InvalidateSkeleton();

private:
    void SyncSkeletonIfPossible();
    void UploadIfSkinned();

private:
    std::unique_ptr<CAnimator>       m_pAnimator;
    std::unique_ptr<CAnimController> m_pController;
    bool                             m_bSkeletonBound = false;
};
