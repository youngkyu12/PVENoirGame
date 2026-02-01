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

    // (pelvis/spine 캐시 및 로그 제거)
}

void CAnimator::AddClip(const AnimationClip& clip)
{
    m_Clips[clip.name] = clip;
}

bool CAnimator::HasClip(const std::string& name) const
{
    return (m_Clips.find(name) != m_Clips.end());
}

bool CAnimator::Play(const std::string& clipName, bool loop, float startTime)
{
    AnimationClip* clip = FindClipPtr(clipName);
    if (!clip) return false;

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
}

void CAnimator::SetTime(float timeSec)
{
    m_fCurrentTime = timeSec;
}

void CAnimator::Update(float dt)
{
    if (!m_bPlaying) return;

    AnimationClip* cur = FindClipPtr(m_CurrentClipName);
    if (!cur) return;

    const int boneCount = (int)m_Skeleton.size();
    if (boneCount <= 0) return;

    if (!m_bBlending)
    {
        AdvanceTime(cur, m_fCurrentTime, dt, m_bLoop);

        cur->Evaluate(m_fCurrentTime, m_Skeleton, m_LocalPose);
        BuildGlobalAndFinalFromLocal();
        return;
    }

    AnimationClip* nxt = FindClipPtr(m_NextClipName);
    if (!nxt)
    {
        m_bBlending = false;
        return;
    }

    // 블렌딩 구간에서는 두 클립을 각각 진행시키고, 로컬 포즈를 평가 후 혼합한다.
    AdvanceTime(cur, m_fCurrentTime, dt, m_bLoop);
    AdvanceTime(nxt, m_fNextTime, dt, m_bNextLoop);

    cur->Evaluate(m_fCurrentTime, m_Skeleton, m_LocalPoseA);
    nxt->Evaluate(m_fNextTime, m_Skeleton, m_LocalPoseB);

    m_fBlendElapsed += dt;
    float alpha = (m_fBlendDuration > 0.0f) ? (m_fBlendElapsed / m_fBlendDuration) : 1.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    BlendLocalPosesTRS(m_LocalPoseA, m_LocalPoseB, alpha, m_LocalPose);

    BuildGlobalAndFinalFromLocal();

    if (alpha >= 1.0f)
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

AnimationClip* CAnimator::FindClipPtr(const std::string& name)
{
    auto it = m_Clips.find(name);
    if (it == m_Clips.end()) return nullptr;
    return &it->second;
}

void CAnimator::AdvanceTime(AnimationClip* clip, float& time, float dt, bool loop)
{
    if (!clip) return;
    time += dt;

    if (clip->duration <= 0.0f) { time = 0.0f; return; }

    if (time >= clip->duration)
    {
        if (loop) time = fmodf(time, clip->duration);
        else      time = clip->duration;
    }
    if (time < 0.0f) time = 0.0f;
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

bool CAnimator::CrossFade(const std::string& nextClipName, float blendTimeSec, bool loop, float startTime)
{
    AnimationClip* next = FindClipPtr(nextClipName);
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