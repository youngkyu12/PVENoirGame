//------------------------------------------------------- ----------------------
// File: Animator.h
//-----------------------------------------------------------------------------

#pragma once
#include "stdafx.h"
#include "AnimatorData.h"

/*
    ============================================================
    CAnimator
    ------------------------------------------------------------
    - 스켈레톤(본 계층/오프셋) 보유
    - AnimationClip들을 등록/조회
    - 현재 클립 재생 및 크로스페이드(블렌딩) 지원
    - Update(dt)에서 로컬 포즈 평가 → 글로벌 포즈 누적 → 스키닝 팔레트(final matrices) 생성
    ============================================================
*/
class CAnimator
{
public:
    CAnimator() = default;
    ~CAnimator() = default;

public:
    void SetSkeleton(const std::vector<Bone>& bones,
        const std::unordered_map<std::string, int>& boneNameToIndex);

    void AddClip(const AnimationClip& clip);

    bool Play(const std::string& clipName, bool loop = true, float startTime = 0.0f);
    void SetTime(float timeSec);
	void Stop();

	void Update(float dt);
	void UpdateTimeOnly(float dt);
	void EvaluateCurrentPoseOnly();

    const std::string& GetCurrentClipName() const;
    const std::vector<XMFLOAT4X4>& GetFinalBoneMatrices() const;

    int GetBoneCount() const { return (int)m_Skeleton.size(); }
    bool HasClip(const std::string& name) const;

    bool PlayUpperBodyOverlay(const std::string& clipName, bool loop = false, float startTime = 0.0f, float blendTimeSec = 0.12f);
    void StopUpperBodyOverlay(bool immediate = false);
    bool IsUpperBodyOverlayActive() const { return m_bUpperBodyOverlay; }
    const std::string& GetUpperBodyOverlayClipName() const { return m_UpperBodyClipName; }

    void SetNextClipAfterEnd(const std::string& clip) { m_NextClipAfterEnd = clip; }

    const std::vector<XMFLOAT4X4>& GetGlobalBoneMatrices() const { return m_GlobalPose; }

    bool GetGlobalBoneMatrix(int boneIndex, XMFLOAT4X4& outMatrix) const;

public:
    bool CrossFade(const std::string& nextClipName, float blendTimeSec,
        bool loop = true, float startTime = 0.0f);

    bool IsBlending() const { return m_bBlending; }

    // --- query helpers ---
	float GetCurrentTime() const { return m_fCurrentTime; }
	float GetCurrentClipDuration() const;
	float GetClipDuration(const std::string& clipName) const;
	bool  IsPlaying() const { return m_bPlaying; }
    bool  IsLooping() const { return m_bLoop; }

    // 현재 클립이 "non-loop"로 재생 중이고 끝에 도달했는지
    bool  IsCurrentClipFinished(float eps = 1e-4f) const;

    float GetUpperBodyOverlayDuration() const;
    bool  IsUpperBodyOverlayFinished(float eps = 1e-4f) const;

    void SetVisualYawOffset(float targetYawDeg, float blendInEndNormalized, float blendOutStartNormalized);
    void ClearVisualYawOffset();

private:
    AnimationClip* FindClipPtr(const std::string& name);

    void AdvanceTime(AnimationClip* clip, float& time, float dt, bool loop);
    void BuildGlobalAndFinalFromLocal();

    // LocalPose(A/B)는 행렬을 TRS로 분해 후 (T,S는 lerp / R은 slerp)로 블렌딩한다.
    void BlendLocalPosesTRS(const std::vector<XMFLOAT4X4>& A,
        const std::vector<XMFLOAT4X4>& B,
        float alpha,
        std::vector<XMFLOAT4X4>& out);

	void BuildUpperBodyBoneWeights(const std::string& rootBoneName);
	void UpdateUpperBodyOverlayTimeOnly(float dt);
	void ApplyUpperBodyOverlayToCurrentPoseOnly();

    void BlendLocalPosesMaskedTRS(const std::vector<XMFLOAT4X4>& A,
        const std::vector<XMFLOAT4X4>& B,
        const std::vector<float>& boneWeights,
        std::vector<XMFLOAT4X4>& out);

    void BuildGlobalPoseFromLocal(const std::vector<XMFLOAT4X4>& localPose,
        std::vector<XMFLOAT4X4>& outGlobalPose) const;
    void RetargetUpperBodyOverlayRootToCurrentParent();
    void ApplyVisualYawOffsetToLocalPose();

private:
    std::vector<Bone> m_Skeleton;

    std::unordered_map<std::string, AnimationClip> m_Clips;

    std::string m_CurrentClipName;

    float m_fCurrentTime = 0.0f;
    bool  m_bPlaying = false;
    bool  m_bLoop = true;

    std::vector<XMFLOAT4X4> m_FinalBoneMatrices;

    std::vector<XMFLOAT4X4> m_LocalPose;
    std::vector<XMFLOAT4X4> m_GlobalPose;

    // Cross-fade state
    bool  m_bBlending = false;
    std::string m_NextClipName;
    float m_fNextTime = 0.0f;
    bool  m_bNextLoop = true;

    float m_fBlendElapsed = 0.0f;
    float m_fBlendDuration = 0.0f;

    std::vector<XMFLOAT4X4> m_LocalPoseA;
    std::vector<XMFLOAT4X4> m_LocalPoseB;

    std::string m_NextClipAfterEnd;

    std::vector<float> m_UpperBodyBoneWeights;

    bool  m_bUpperBodyOverlay = false;
    std::string m_UpperBodyClipName;
    float m_fUpperBodyTime = 0.0f;
    bool  m_bUpperBodyLoop = false;

    std::vector<XMFLOAT4X4> m_LocalPoseUpperBody;

    std::vector<float> m_UpperBodyBlendWeights;

    float m_fUpperBodyBlendAlpha = 0.0f;
    float m_fUpperBodyBlendStartAlpha = 0.0f;
    float m_fUpperBodyBlendTargetAlpha = 0.0f;
    float m_fUpperBodyBlendElapsed = 0.0f;
    float m_fUpperBodyBlendDuration = 0.0f;
    float m_fUpperBodyFadeOutDuration = 0.10f;

    std::vector<XMFLOAT4X4> m_GlobalPoseScratchBase;
    std::vector<XMFLOAT4X4> m_GlobalPoseScratchUpper;
    int m_UpperBodyRootBoneIndex = -1;

    bool  m_bVisualYawOffset = false;
    float m_fVisualYawTargetDeg = 0.0f;
    float m_fVisualYawBlendInEndNormalized = 0.0f;
    float m_fVisualYawBlendOutStartNormalized = 1.0f;

public:
	const std::vector<XMFLOAT4X4>& GetCurrentGlobalPose() const { return m_GlobalPose; }
};