//------------------------------------------------------- ----------------------
// File: Animator.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Animator.h"

static void DecomposeTRS_M(const XMFLOAT4X4& M, XMFLOAT3& outT, XMFLOAT4& outR, XMFLOAT3& outS)
{
    XMMATRIX m = XMLoadFloat4x4(&M);
    XMVECTOR S, R, T;
    if (!XMMatrixDecompose(&S, &R, &T, m))
    {
        outT = { 0,0,0 };
        outR = { 0,0,0,1 };
        outS = { 1,1,1 };
        return;
    }
    XMStoreFloat3(&outS, S);
    XMStoreFloat4(&outR, XMQuaternionNormalize(R));
    XMStoreFloat3(&outT, T);
}

static XMFLOAT4X4 ComposeTRS_M(const XMFLOAT3& t, const XMFLOAT4& r, const XMFLOAT3& s)
{
    XMVECTOR T = XMLoadFloat3(&t);
    XMVECTOR R = XMLoadFloat4(&r);
    XMVECTOR S = XMLoadFloat3(&s);

    XMMATRIX mS = XMMatrixScalingFromVector(S);
    XMMATRIX mR = XMMatrixRotationQuaternion(R);
    XMMATRIX mT = XMMatrixTranslationFromVector(T);

    // (주의) 이 파일은 로컬 변환을 S*R*T 순서로 구성한다고 가정한다.
    XMMATRIX M = mS * mR * mT;

    XMFLOAT4X4 out{};
    XMStoreFloat4x4(&out, M);
    return out;
}

void CAnimator::SetSkeleton(const std::vector<Bone>& bones,
    const std::unordered_map<std::string, int>& boneNameToIndex)
{
    (void)boneNameToIndex; // 현재 파일에선 사용하지 않음

    m_Skeleton = bones;

    int boneCount = (int)bones.size();

    m_LocalPose.resize(boneCount);
    m_GlobalPose.resize(boneCount);
    m_FinalBoneMatrices.resize(boneCount);

    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());

    for (int i = 0; i < boneCount; ++i)
    {
        m_LocalPose[i] = identity;
        m_GlobalPose[i] = identity;
        m_FinalBoneMatrices[i] = identity;
    }
    BuildUpperBodyBoneWeights("spine_01");
    // (pelvis/spine 캐시 및 로그 제거)
}

void CAnimator::AddClip(AnimationClipRef clip)
{
	if ( !clip )
		return;

	if ( clip->name.empty() )
		return;

	m_Clips[clip->name] = std::move(clip);
}

void CAnimator::AddClip(const AnimationClip& clip)
{
	// 기존 호출부 호환용.
	// 다음 단계에서 GameSceneObjectFactory / CAnimatorComponent가
	// shared_ptr<const AnimationClip>를 직접 넘기게 되면 이 경로는 제거 가능하다.
	AddClip(std::make_shared<AnimationClip>(clip));
}

bool CAnimator::HasClip(const std::string& name) const
{
    return (m_Clips.find(name) != m_Clips.end());
}

bool CAnimator::Play(const std::string& clipName, bool loop, float startTime)
{
	const AnimationClip* clip = FindClipPtr(clipName);
	if ( !clip ) return false;

    m_CurrentClipName = clipName;
    m_fCurrentTime = startTime;
    m_bLoop = loop;
    m_bPlaying = true;

    // 진행 중인 크로스페이드는 즉시 종료하고, 요청 클립으로 스냅 전환한다.
    m_bBlending = false;
    m_NextClipName.clear();
    m_fBlendElapsed = 0.0f;
    m_fBlendDuration = 0.0f;

    // startTime 시점의 로컬 포즈를 즉시 평가하여 첫 프레임부터 T-포즈를 피한다.
    clip->Evaluate(m_fCurrentTime, m_Skeleton, m_LocalPose);
    BuildGlobalAndFinalFromLocal();

    m_NextClipAfterEnd.clear();
    return true;
}

void CAnimator::Stop()
{
    m_bPlaying = false;
    m_fCurrentTime = 0.0f;
    StopUpperBodyOverlay(true);
    ClearVisualYawOffset();
}

void CAnimator::SetTime(float timeSec)
{
    m_fCurrentTime = timeSec;
}

void CAnimator::Update(float dt)
{
	UpdateTimeOnly(dt);
	EvaluateCurrentPoseOnly();
}

void CAnimator::UpdateTimeOnly(float dt)
{
	if ( !m_bPlaying )
		return;

	const AnimationClip* cur = FindClipPtr(m_CurrentClipName);
	if ( !cur )
		return;

	// 스켈레톤이 비어 있어도 시간 자체는 진행시킬 수 있다.
	// 단, 정상적인 skinned object라면 보통 skeleton은 이미 바인딩되어 있다.

	if ( !m_bBlending )
	{
		AdvanceTime(cur, m_fCurrentTime, dt, m_bLoop);
		UpdateUpperBodyOverlayTimeOnly(dt);
		return;
	}

	const AnimationClip* nxt = FindClipPtr(m_NextClipName);

	if ( !nxt )
	{
		m_bBlending = false;

		AdvanceTime(cur, m_fCurrentTime, dt, m_bLoop);
		UpdateUpperBodyOverlayTimeOnly(dt);
		return;
	}

	AdvanceTime(cur, m_fCurrentTime, dt, m_bLoop);
	AdvanceTime(nxt, m_fNextTime, dt, m_bNextLoop);

	m_fBlendElapsed += dt;

	float alpha =
		( m_fBlendDuration > 0.0f )
		? ( m_fBlendElapsed / m_fBlendDuration )
		: 1.0f;

	if ( alpha > 1.0f )
		alpha = 1.0f;

	UpdateUpperBodyOverlayTimeOnly(dt);

	if ( alpha >= 1.0f )
	{
		m_bBlending = false;
		m_CurrentClipName = m_NextClipName;
		m_NextClipName.clear();

		m_fCurrentTime = m_fNextTime;
		m_bLoop = m_bNextLoop;

		m_fBlendElapsed = 0.0f;
		m_fBlendDuration = 0.0f;
	}
}

void CAnimator::EvaluateCurrentPoseOnly()
{
	if ( !m_bPlaying )
		return;

	const AnimationClip* cur = FindClipPtr(m_CurrentClipName);
	if ( !cur )
		return;

	const int boneCount = static_cast< int >( m_Skeleton.size() );

	if ( boneCount <= 0 )
		return;

	if ( !m_bBlending )
	{
		cur->Evaluate(m_fCurrentTime, m_Skeleton, m_LocalPose);
		ApplyUpperBodyOverlayToCurrentPoseOnly();
		ApplyVisualYawOffsetToLocalPose();
		BuildGlobalAndFinalFromLocal();
		return;
	}

	const AnimationClip* nxt = FindClipPtr(m_NextClipName);

	if ( !nxt )
	{
		m_bBlending = false;

		cur->Evaluate(m_fCurrentTime, m_Skeleton, m_LocalPose);
		ApplyUpperBodyOverlayToCurrentPoseOnly();
		ApplyVisualYawOffsetToLocalPose();
		BuildGlobalAndFinalFromLocal();
		return;
	}

	cur->Evaluate(m_fCurrentTime, m_Skeleton, m_LocalPoseA);
	nxt->Evaluate(m_fNextTime, m_Skeleton, m_LocalPoseB);

	float alpha =
		( m_fBlendDuration > 0.0f )
		? ( m_fBlendElapsed / m_fBlendDuration )
		: 1.0f;

	if ( alpha < 0.0f )
		alpha = 0.0f;

	if ( alpha > 1.0f )
		alpha = 1.0f;

	BlendLocalPosesTRS(m_LocalPoseA, m_LocalPoseB, alpha, m_LocalPose);

	ApplyUpperBodyOverlayToCurrentPoseOnly();
	ApplyVisualYawOffsetToLocalPose();
	BuildGlobalAndFinalFromLocal();
}

const AnimationClip* CAnimator::FindClipPtr(const std::string& name) const
{
	auto it = m_Clips.find(name);
	if ( it == m_Clips.end() )
		return nullptr;

	if ( !it->second )
		return nullptr;

	return it->second.get();
}

void CAnimator::AdvanceTime(const AnimationClip* clip, float& time, float dt, bool loop)
{
	if ( !clip ) return;
	time += dt;

	if ( clip->duration <= 0.0f ) { time = 0.0f; return; }

	if ( time >= clip->duration )
	{
		if ( loop ) time = fmodf(time, clip->duration);
		else      time = clip->duration;
	}
	if ( time < 0.0f ) time = 0.0f;
}

void CAnimator::BlendLocalPosesTRS(const std::vector<XMFLOAT4X4>& A,
    const std::vector<XMFLOAT4X4>& B,
    float alpha,
    std::vector<XMFLOAT4X4>& out)
{
    const int n = (int)m_Skeleton.size();
    if (n <= 0) return;
    if ((int)out.size() != n) out.resize(n);

    alpha = (alpha < 0.f) ? 0.f : (alpha > 1.f ? 1.f : alpha);

    for (int i = 0; i < n; ++i)
    {
        XMFLOAT3 tA, sA, tB, sB;
        XMFLOAT4 rA, rB;

        DecomposeTRS_M(A[i], tA, rA, sA);
        DecomposeTRS_M(B[i], tB, rB, sB);

        XMVECTOR Ta = XMLoadFloat3(&tA);
        XMVECTOR Tb = XMLoadFloat3(&tB);
        XMVECTOR Sa = XMLoadFloat3(&sA);
        XMVECTOR Sb = XMLoadFloat3(&sB);

        XMVECTOR T = XMVectorLerp(Ta, Tb, alpha);
        XMVECTOR S = XMVectorLerp(Sa, Sb, alpha);

        XMVECTOR Ra = XMLoadFloat4(&rA);
        XMVECTOR Rb = XMLoadFloat4(&rB);
        XMVECTOR R = XMQuaternionSlerp(Ra, Rb, alpha);
        R = XMQuaternionNormalize(R);

        XMFLOAT3 t; XMFLOAT4 r; XMFLOAT3 s;
        XMStoreFloat3(&t, T);
        XMStoreFloat4(&r, R);
        XMStoreFloat3(&s, S);

        out[i] = ComposeTRS_M(t, r, s);
    }
}

void CAnimator::BlendLocalPosesMaskedTRS(const std::vector<XMFLOAT4X4>& A,
    const std::vector<XMFLOAT4X4>& B,
    const std::vector<float>& boneWeights,
    std::vector<XMFLOAT4X4>& out)
{
    const int n = (int)m_Skeleton.size();
    if (n <= 0) return;
    if ((int)out.size() != n) out.resize(n);

    for (int i = 0; i < n; ++i)
    {
        float alpha = 0.0f;
        if (i < (int)boneWeights.size())
            alpha = boneWeights[i];

        if (alpha <= 0.0f)
        {
            out[i] = A[i];
            continue;
        }

        if (alpha >= 1.0f)
        {
            out[i] = B[i];
            continue;
        }

        XMFLOAT3 tA, sA, tB, sB;
        XMFLOAT4 rA, rB;

        DecomposeTRS_M(A[i], tA, rA, sA);
        DecomposeTRS_M(B[i], tB, rB, sB);

        XMVECTOR Ta = XMLoadFloat3(&tA);
        XMVECTOR Tb = XMLoadFloat3(&tB);
        XMVECTOR Sa = XMLoadFloat3(&sA);
        XMVECTOR Sb = XMLoadFloat3(&sB);

        XMVECTOR T = XMVectorLerp(Ta, Tb, alpha);
        XMVECTOR S = XMVectorLerp(Sa, Sb, alpha);

        XMVECTOR Ra = XMLoadFloat4(&rA);
        XMVECTOR Rb = XMLoadFloat4(&rB);
        XMVECTOR R = XMQuaternionSlerp(Ra, Rb, alpha);
        R = XMQuaternionNormalize(R);

        XMFLOAT3 t;
        XMFLOAT4 r;
        XMFLOAT3 s;

        XMStoreFloat3(&t, T);
        XMStoreFloat4(&r, R);
        XMStoreFloat3(&s, S);

        out[i] = ComposeTRS_M(t, r, s);
    }
}

void CAnimator::BuildGlobalPoseFromLocal(const std::vector<XMFLOAT4X4>& localPose,
    std::vector<XMFLOAT4X4>& outGlobalPose) const
{
    const int boneCount = (int)m_Skeleton.size();
    if (boneCount <= 0)
    {
        outGlobalPose.clear();
        return;
    }

    if ((int)outGlobalPose.size() != boneCount)
        outGlobalPose.resize(boneCount);

    auto FindPrimaryRoot = [&]() -> int
        {
            std::vector<int> roots;
            for (int i = 0; i < boneCount; ++i)
            {
                if (m_Skeleton[i].parentIndex < 0)
                    roots.push_back(i);
            }

            if (roots.empty()) return 0;
            if (roots.size() == 1) return roots[0];

            auto CountDesc = [&](int root) -> int
                {
                    int cnt = 0;
                    for (int i = 0; i < boneCount; ++i)
                    {
                        int p = m_Skeleton[i].parentIndex;
                        while (p >= 0)
                        {
                            if (p == root)
                            {
                                cnt++;
                                break;
                            }
                            p = m_Skeleton[p].parentIndex;
                        }
                    }
                    return cnt;
                };

            int best = roots[0];
            int bestCnt = -1;

            for (int r : roots)
            {
                const int c = CountDesc(r);
                if (c > bestCnt)
                {
                    bestCnt = c;
                    best = r;
                }
            }

            return best;
        };

    const int primaryRoot = FindPrimaryRoot();

    {
        XMMATRIX localRoot = XMLoadFloat4x4(&localPose[primaryRoot]);
        XMStoreFloat4x4(&outGlobalPose[primaryRoot], localRoot);
    }

    for (int i = 0; i < boneCount; ++i)
    {
        if (i == primaryRoot)
            continue;

        int parent = m_Skeleton[i].parentIndex;
        XMMATRIX local = XMLoadFloat4x4(&localPose[i]);

        if (parent < 0)
        {
            XMMATRIX primaryG = XMLoadFloat4x4(&outGlobalPose[primaryRoot]);
            XMMATRIX invPrimaryG = XMMatrixInverse(nullptr, primaryG);
            local = local * invPrimaryG;
            parent = primaryRoot;
        }

        XMMATRIX parentM = XMLoadFloat4x4(&outGlobalPose[parent]);
        XMMATRIX global = local * parentM;
        XMStoreFloat4x4(&outGlobalPose[i], global);
    }
}

void CAnimator::RetargetUpperBodyOverlayRootToCurrentParent()
{
    if (!m_bUpperBodyOverlay)
        return;

    if (m_UpperBodyRootBoneIndex < 0)
        return;

    const int rootIndex = m_UpperBodyRootBoneIndex;
    const int parentIndex = m_Skeleton[rootIndex].parentIndex;

    if (parentIndex < 0)
        return;

    if ((int)m_LocalPose.size() != (int)m_Skeleton.size())
        return;

    if ((int)m_LocalPoseUpperBody.size() != (int)m_Skeleton.size())
        return;

    BuildGlobalPoseFromLocal(m_LocalPose, m_GlobalPoseScratchBase);
    BuildGlobalPoseFromLocal(m_LocalPoseUpperBody, m_GlobalPoseScratchUpper);

    XMMATRIX attackRootGlobal = XMLoadFloat4x4(&m_GlobalPoseScratchUpper[rootIndex]);
    XMMATRIX baseParentGlobal = XMLoadFloat4x4(&m_GlobalPoseScratchBase[parentIndex]);
    XMMATRIX invBaseParentGlobal = XMMatrixInverse(nullptr, baseParentGlobal);

    XMMATRIX correctedRootLocal = attackRootGlobal * invBaseParentGlobal;
    XMStoreFloat4x4(&m_LocalPoseUpperBody[rootIndex], correctedRootLocal);
}

void CAnimator::BuildUpperBodyBoneWeights(const std::string& rootBoneName)
{
    const int boneCount = (int)m_Skeleton.size();
    m_UpperBodyBoneWeights.assign(boneCount, 0.0f);
    m_UpperBodyRootBoneIndex = -1;

    if (boneCount <= 0)
        return;

    int rootIndex = -1;
    for (int i = 0; i < boneCount; ++i)
    {
        if (m_Skeleton[i].name == rootBoneName)
        {
            rootIndex = i;
            break;
        }
    }

    if (rootIndex < 0)
        return;

    m_UpperBodyRootBoneIndex = rootIndex;

    std::vector<int> stack;
    std::vector<int> depth(boneCount, -1);

    stack.push_back(rootIndex);
    depth[rootIndex] = 0;

    while (!stack.empty())
    {
        const int boneIndex = stack.back();
        stack.pop_back();

        const int d = depth[boneIndex];

        if (d == 0)      m_UpperBodyBoneWeights[boneIndex] = 0.60f;
        else if (d == 1) m_UpperBodyBoneWeights[boneIndex] = 0.85f;
        else             m_UpperBodyBoneWeights[boneIndex] = 1.00f;

        for (int i = 0; i < boneCount; ++i)
        {
            if (m_Skeleton[i].parentIndex == boneIndex)
            {
                depth[i] = d + 1;
                stack.push_back(i);
            }
        }
    }

    int ancestor = m_Skeleton[rootIndex].parentIndex;
    float ancestorWeight = 0.30f;

    while (ancestor >= 0)
    {
        if (m_UpperBodyBoneWeights[ancestor] < ancestorWeight)
            m_UpperBodyBoneWeights[ancestor] = ancestorWeight;

        ancestorWeight *= 0.5f;
        ancestor = m_Skeleton[ancestor].parentIndex;
    }
}

bool CAnimator::PlayUpperBodyOverlay(const std::string& clipName, bool loop, float startTime, float blendTimeSec)
{
	const AnimationClip* clip = FindClipPtr(clipName);
    if (!clip) return false;
    if (m_Skeleton.empty()) return false;

    const bool wasActive = m_bUpperBodyOverlay;
    const float startAlpha = wasActive ? m_fUpperBodyBlendAlpha : 0.0f;

    m_bUpperBodyOverlay = true;
    m_UpperBodyClipName = clipName;
    m_fUpperBodyTime = startTime;
    m_bUpperBodyLoop = loop;

    if ((int)m_LocalPoseUpperBody.size() != (int)m_Skeleton.size())
        m_LocalPoseUpperBody.resize(m_Skeleton.size());

    clip->Evaluate(m_fUpperBodyTime, m_Skeleton, m_LocalPoseUpperBody);

    m_fUpperBodyBlendAlpha = startAlpha;
    m_fUpperBodyBlendStartAlpha = startAlpha;
    m_fUpperBodyBlendTargetAlpha = 1.0f;
    m_fUpperBodyBlendElapsed = 0.0f;
    m_fUpperBodyBlendDuration = blendTimeSec;

    if (m_fUpperBodyBlendDuration <= 0.0f)
    {
        m_fUpperBodyBlendAlpha = 1.0f;
        m_fUpperBodyBlendStartAlpha = 1.0f;
        m_fUpperBodyBlendTargetAlpha = 1.0f;
        m_fUpperBodyBlendElapsed = 0.0f;
        m_fUpperBodyBlendDuration = 0.0f;
    }

    return true;
}

void CAnimator::StopUpperBodyOverlay(bool immediate)
{
    if (!m_bUpperBodyOverlay)
        return;

    if (!immediate)
    {
        if (m_fUpperBodyBlendTargetAlpha <= 0.0f)
            return;

        m_fUpperBodyBlendStartAlpha = m_fUpperBodyBlendAlpha;
        m_fUpperBodyBlendTargetAlpha = 0.0f;
        m_fUpperBodyBlendElapsed = 0.0f;
        m_fUpperBodyBlendDuration = m_fUpperBodyFadeOutDuration;

        if (m_fUpperBodyBlendDuration > 0.0f)
            return;
    }

    m_bUpperBodyOverlay = false;
    m_UpperBodyClipName.clear();
    m_fUpperBodyTime = 0.0f;
    m_bUpperBodyLoop = false;

    m_fUpperBodyBlendAlpha = 0.0f;
    m_fUpperBodyBlendStartAlpha = 0.0f;
    m_fUpperBodyBlendTargetAlpha = 0.0f;
    m_fUpperBodyBlendElapsed = 0.0f;
    m_fUpperBodyBlendDuration = 0.0f;
}

void CAnimator::UpdateUpperBodyOverlayTimeOnly(float dt)
{
	if ( !m_bUpperBodyOverlay )
		return;

	const  AnimationClip* overlay = FindClipPtr(m_UpperBodyClipName);

	if ( !overlay )
	{
		StopUpperBodyOverlay(true);
		return;
	}

	AdvanceTime(overlay, m_fUpperBodyTime, dt, m_bUpperBodyLoop);

	if ( m_fUpperBodyBlendDuration <= 0.0f )
	{
		m_fUpperBodyBlendAlpha = m_fUpperBodyBlendTargetAlpha;
	}
	else
	{
		m_fUpperBodyBlendElapsed += dt;

		float t = m_fUpperBodyBlendElapsed / m_fUpperBodyBlendDuration;

		if ( t < 0.0f )
			t = 0.0f;

		if ( t > 1.0f )
			t = 1.0f;

		m_fUpperBodyBlendAlpha =
			m_fUpperBodyBlendStartAlpha +
			( m_fUpperBodyBlendTargetAlpha - m_fUpperBodyBlendStartAlpha ) * t;
	}

	if ( m_fUpperBodyBlendAlpha < 0.0f )
		m_fUpperBodyBlendAlpha = 0.0f;

	if ( m_fUpperBodyBlendAlpha > 1.0f )
		m_fUpperBodyBlendAlpha = 1.0f;

	if ( ( m_fUpperBodyBlendTargetAlpha <= 0.0f ) &&
		( m_fUpperBodyBlendAlpha <= 0.0f ) )
	{
		StopUpperBodyOverlay(true);
		return;
	}
}

void CAnimator::ApplyUpperBodyOverlayToCurrentPoseOnly()
{
	if ( !m_bUpperBodyOverlay )
		return;

	const  AnimationClip* overlay = FindClipPtr(m_UpperBodyClipName);

	if ( !overlay )
	{
		StopUpperBodyOverlay(true);
		return;
	}

	if ( ( int ) m_LocalPoseUpperBody.size() != ( int ) m_Skeleton.size() )
		m_LocalPoseUpperBody.resize(m_Skeleton.size());

	if ( ( int ) m_UpperBodyBlendWeights.size() != ( int ) m_UpperBodyBoneWeights.size() )
		m_UpperBodyBlendWeights.resize(m_UpperBodyBoneWeights.size());

	overlay->Evaluate(m_fUpperBodyTime, m_Skeleton, m_LocalPoseUpperBody);
	RetargetUpperBodyOverlayRootToCurrentParent();

	for ( size_t i = 0; i < m_UpperBodyBoneWeights.size(); ++i )
	{
		m_UpperBodyBlendWeights[i] =
			m_UpperBodyBoneWeights[i] * m_fUpperBodyBlendAlpha;
	}

	BlendLocalPosesMaskedTRS(
		m_LocalPose,
		m_LocalPoseUpperBody,
		m_UpperBodyBlendWeights,
		m_LocalPose
	);
}

void CAnimator::SetVisualYawOffset(float targetYawDeg, float blendInEndNormalized, float blendOutStartNormalized)
{
    m_bVisualYawOffset = true;
    m_fVisualYawTargetDeg = targetYawDeg;

    m_fVisualYawBlendInEndNormalized = blendInEndNormalized;
    if (m_fVisualYawBlendInEndNormalized < 0.0f)
        m_fVisualYawBlendInEndNormalized = 0.0f;
    if (m_fVisualYawBlendInEndNormalized > 1.0f)
        m_fVisualYawBlendInEndNormalized = 1.0f;

    m_fVisualYawBlendOutStartNormalized = blendOutStartNormalized;
    if (m_fVisualYawBlendOutStartNormalized < 0.0f)
        m_fVisualYawBlendOutStartNormalized = 0.0f;
    if (m_fVisualYawBlendOutStartNormalized > 1.0f)
        m_fVisualYawBlendOutStartNormalized = 1.0f;

    if (m_fVisualYawBlendInEndNormalized > m_fVisualYawBlendOutStartNormalized)
    {
        const float t = m_fVisualYawBlendInEndNormalized;
        m_fVisualYawBlendInEndNormalized = m_fVisualYawBlendOutStartNormalized;
        m_fVisualYawBlendOutStartNormalized = t;
    }
}

void CAnimator::ClearVisualYawOffset()
{
    m_bVisualYawOffset = false;
    m_fVisualYawTargetDeg = 0.0f;
    m_fVisualYawBlendInEndNormalized = 0.0f;
    m_fVisualYawBlendOutStartNormalized = 1.0f;
}

void CAnimator::ApplyVisualYawOffsetToLocalPose()
{
    if (!m_bVisualYawOffset)
        return;

    const int boneCount = (int)m_Skeleton.size();
    if (boneCount <= 0)
        return;

    if ((int)m_LocalPose.size() != boneCount)
        return;

    auto FindPrimaryRoot = [&]() -> int
        {
            std::vector<int> roots;
            for (int i = 0; i < boneCount; ++i)
            {
                if (m_Skeleton[i].parentIndex < 0)
                    roots.push_back(i);
            }

            if (roots.empty()) return 0;
            if (roots.size() == 1) return roots[0];

            auto CountDesc = [&](int root) -> int
                {
                    int cnt = 0;
                    for (int i = 0; i < boneCount; ++i)
                    {
                        int p = m_Skeleton[i].parentIndex;
                        while (p >= 0)
                        {
                            if (p == root)
                            {
                                cnt++;
                                break;
                            }
                            p = m_Skeleton[p].parentIndex;
                        }
                    }
                    return cnt;
                };

            int best = roots[0];
            int bestCnt = -1;

            for (int r : roots)
            {
                const int c = CountDesc(r);
                if (c > bestCnt)
                {
                    bestCnt = c;
                    best = r;
                }
            }

            return best;
        };

    const int primaryRoot = FindPrimaryRoot();
    if (primaryRoot < 0 || primaryRoot >= boneCount)
        return;

    float alpha = 1.0f;

    const float duration = GetCurrentClipDuration();
    if (duration > 1e-6f)
    {
        float t = m_fCurrentTime / duration;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        if ((m_fVisualYawBlendInEndNormalized > 0.0f) && (t < m_fVisualYawBlendInEndNormalized))
        {
            alpha = t / m_fVisualYawBlendInEndNormalized;
        }
        else if (t > m_fVisualYawBlendOutStartNormalized)
        {
            const float denom = 1.0f - m_fVisualYawBlendOutStartNormalized;
            if (denom > 1e-6f)
                alpha = 1.0f - ((t - m_fVisualYawBlendOutStartNormalized) / denom);
            else
                alpha = 0.0f;
        }
    }

    if (alpha <= 0.0f)
        return;

    const float yawRad = XMConvertToRadians(m_fVisualYawTargetDeg * alpha);
    if (fabsf(yawRad) <= 1e-6f)
        return;

    XMFLOAT3 tRoot, sRoot;
    XMFLOAT4 rRoot;
    DecomposeTRS_M(m_LocalPose[primaryRoot], tRoot, rRoot, sRoot);

    const XMMATRIX baseR = XMMatrixRotationQuaternion(XMLoadFloat4(&rRoot));
    const XMMATRIX extraR = XMMatrixRotationY(yawRad);
    const XMMATRIX combinedR = baseR * extraR;

    XMFLOAT4 rNew{};
    XMStoreFloat4(&rNew, XMQuaternionNormalize(XMQuaternionRotationMatrix(combinedR)));

    m_LocalPose[primaryRoot] = ComposeTRS_M(tRoot, rNew, sRoot);
}

void CAnimator::BuildGlobalAndFinalFromLocal()
{
    const int boneCount = (int)m_Skeleton.size();
    if (boneCount <= 0) return;

    // 여러 루트가 존재할 경우, 가장 많은 하위 본을 가진 루트를 primary로 선택한다.
    auto FindPrimaryRoot = [&]() -> int
        {
            std::vector<int> roots;
            for (int i = 0; i < boneCount; ++i)
                if (m_Skeleton[i].parentIndex < 0) roots.push_back(i);

            if (roots.empty()) return 0;
            if (roots.size() == 1) return roots[0];

            auto CountDesc = [&](int root)->int
                {
                    int cnt = 0;
                    for (int i = 0; i < boneCount; ++i)
                    {
                        int p = m_Skeleton[i].parentIndex;
                        while (p >= 0)
                        {
                            if (p == root) { cnt++; break; }
                            p = m_Skeleton[p].parentIndex;
                        }
                    }
                    return cnt;
                };

            int best = roots[0], bestCnt = -1;
            for (int r : roots)
            {
                int c = CountDesc(r);
                if (c > bestCnt) { bestCnt = c; best = r; }
            }
            return best;
        };

    int primaryRoot = FindPrimaryRoot();

    {
        XMMATRIX localRoot = XMLoadFloat4x4(&m_LocalPose[primaryRoot]);
        XMStoreFloat4x4(&m_GlobalPose[primaryRoot], localRoot);
    }

    // 로컬 → 글로벌 누적. secondary root는 primaryRoot에 붙여 하나의 트리처럼 취급한다.
    for (int i = 0; i < boneCount; ++i)
    {
        if (i == primaryRoot) continue;

        int parent = m_Skeleton[i].parentIndex;
        XMMATRIX local = XMLoadFloat4x4(&m_LocalPose[i]);

        if (parent < 0)
        {
            XMMATRIX primaryG = XMLoadFloat4x4(&m_GlobalPose[primaryRoot]);
            XMMATRIX invPrimaryG = XMMatrixInverse(nullptr, primaryG);
            local = local * invPrimaryG;
            parent = primaryRoot;
        }

        XMMATRIX parentM = XMLoadFloat4x4(&m_GlobalPose[parent]);
        XMMATRIX global = local * parentM;
        XMStoreFloat4x4(&m_GlobalPose[i], global);
    }

    for (int i = 0; i < boneCount; ++i)
    {
        XMMATRIX global = XMLoadFloat4x4(&m_GlobalPose[i]);
        XMMATRIX offset = XMLoadFloat4x4(&m_Skeleton[i].offsetMatrix);
        XMMATRIX skin = offset * global;
        XMStoreFloat4x4(&m_FinalBoneMatrices[i], skin);
    }
}

bool CAnimator::GetGlobalBoneMatrix(int boneIndex, XMFLOAT4X4& outMatrix) const
{
    if (boneIndex < 0 || boneIndex >= (int)m_GlobalPose.size())
        return false;

    outMatrix = m_GlobalPose[(size_t)boneIndex];
    return true;
}

bool CAnimator::CrossFade(const std::string& nextClipName, float blendTimeSec, bool loop, float startTime)
{
	const AnimationClip* next = FindClipPtr(nextClipName);
    if (!next) return false;

    if (!m_bPlaying || m_CurrentClipName.empty())
        return Play(nextClipName, loop, startTime);

    // ★ 블렌딩 중이면 "현재"가 아니라 "목표(next)" 기준으로 중복 판단해야 함
    if (m_bBlending)
    {
        // 이미 그쪽으로 블렌딩 중이면 아무것도 안 함
        if (m_NextClipName == nextClipName)
            return true;

        // ★ 블렌딩 도중 "현재 클립"으로 되돌아가라 = 블렌딩 취소
        if (m_CurrentClipName == nextClipName)
        {
            m_bBlending = false;
            m_NextClipName.clear();
            m_fBlendElapsed = 0.0f;
            m_fBlendDuration = 0.0f;
            // 현재 포즈는 이미 m_LocalPose에 반영되어 있을 수 있으니,
            // 안전하게 현재 클립 재평가해서 고정해도 됨(선택)
            // FindClipPtr(m_CurrentClipName)->Evaluate(m_fCurrentTime, m_Skeleton, m_LocalPose);
            // BuildGlobalAndFinalFromLocal();
            return true;
        }
    }
    else
    {
        // 블렌딩이 아닐 때만 "현재 == 요청"이면 무시
        if (m_CurrentClipName == nextClipName)
            return true;
    }

    if (blendTimeSec <= 0.0f)
        return Play(nextClipName, loop, startTime);

    m_bBlending = true;
    m_NextClipName = nextClipName;
    m_fNextTime = startTime;
    m_bNextLoop = loop;

    m_fBlendElapsed = 0.0f;
    m_fBlendDuration = blendTimeSec;

    const int n = (int)m_Skeleton.size();
    if ((int)m_LocalPoseA.size() != n) m_LocalPoseA.resize(n);
    if ((int)m_LocalPoseB.size() != n) m_LocalPoseB.resize(n);

    return true;
}

const std::string& CAnimator::GetCurrentClipName() const
{
    return m_CurrentClipName;
}

const std::vector<XMFLOAT4X4>& CAnimator::GetFinalBoneMatrices() const
{
    return m_FinalBoneMatrices;
}

float CAnimator::GetClipDuration(const std::string& clipName) const
{
	const AnimationClip* clip = FindClipPtr(clipName);
	if ( !clip )
		return 0.0f;

	return clip->duration;
}

float CAnimator::GetCurrentClipDuration() const
{
	const AnimationClip* clip = FindClipPtr(m_CurrentClipName);
	if ( !clip )
		return 0.0f;

	return clip->duration;
}

float CAnimator::GetUpperBodyOverlayDuration() const
{
	const AnimationClip* clip = FindClipPtr(m_UpperBodyClipName);
	if ( !clip )
		return 0.0f;

	return clip->duration;
}

bool CAnimator::IsCurrentClipFinished(float eps) const
{
	if ( !m_bPlaying )
		return false;

	if ( m_bLoop )
		return false; // loop면 "끝" 개념 없음

	const AnimationClip* clip = FindClipPtr(m_CurrentClipName);
	if ( !clip )
		return false;

	const float dur = clip->duration;
	if ( dur <= 0.0f )
		return true;

	// AdvanceTime에서 non-loop는 time을 duration으로 clamp하므로,
	// duration - eps 이상이면 끝으로 본다.
	return ( m_fCurrentTime >= ( dur - eps ) );
}

bool CAnimator::IsUpperBodyOverlayFinished(float eps) const
{
	if ( !m_bUpperBodyOverlay )
		return false;

	if ( m_bUpperBodyLoop )
		return false;

	const AnimationClip* clip = FindClipPtr(m_UpperBodyClipName);
	if ( !clip )
		return false;

	const float dur = clip->duration;
	if ( dur <= 0.0f )
		return true;

	return ( m_fUpperBodyTime >= ( dur - eps ) );
}