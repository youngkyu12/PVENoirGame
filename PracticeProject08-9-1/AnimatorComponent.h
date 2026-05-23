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
class CMonsterAnimController;
struct AnimationClip;

class CAnimatorComponent final : public CComponentT<CAnimatorComponent>
{
public:
	using AnimationClipRef = std::shared_ptr<const AnimationClip>;

public:
	explicit CAnimatorComponent(CGameObject* owner);
	~CAnimatorComponent() override;

    // ---- accessors ----
	CAnimator* GetAnimator() const { return m_pAnimator.get(); }
	CAnimController* GetController() const { return m_pController.get(); }
	CMonsterAnimController* GetMonsterController() const { return m_pMonsterController.get(); }

    // ---- ensure ----
	CAnimator* EnsureAnimator();
	CAnimController* EnsureController();
	CMonsterAnimController* EnsureMonsterController();

	// ---- clip/control wrappers ----
	void AddClip(AnimationClipRef clip);
	void AddClip(const AnimationClip& clip);
	bool HasClip(const char* name) const;

    void SetSpeed(float s);
    void SetIdleClip(const char* name);
    void SetMoveClip(const char* name);

    // ctor/PreCreateComponents
    void EvaluatePose(float dt);

    // lifecycle
    void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;
    void OnUpdate(float dt) override;
    void OnLateUpdate(float dt) override;

    // ---- play wrappers ----
    bool Play(const std::string& name, bool loop = true, float start = 0.0f);
    bool CrossFade(const std::string& name, float blendTime, bool loop = true, float start = 0.0f);

    void InvalidateSkeleton();
	const std::vector<XMFLOAT4X4>* GetCurrentGlobalPose() const;

private:
    void SyncSkeletonIfPossible();
    void UploadIfSkinned();

private:
	std::unique_ptr<CAnimator>              m_pAnimator;
	std::unique_ptr<CAnimController>        m_pController;
	std::unique_ptr<CMonsterAnimController> m_pMonsterController;
	bool                                    m_bSkeletonBound = false;

public:
	void SetPoseEvaluationEnabled(bool enabled) { m_poseEvaluationEnabled = enabled; }
	bool IsPoseEvaluationEnabled() const { return m_poseEvaluationEnabled; }

private:
	bool m_poseEvaluationEnabled = true;
};
