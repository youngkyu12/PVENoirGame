//------------------------------------------------------- ----------------------
// File: Animator.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Animator.h"

static int FindByNameCandidates(
    const std::unordered_map<std::string, int>& map,
    std::initializer_list<const char*> names)
{
    for (auto n : names)
    {
        auto it = map.find(n);
        if (it != map.end()) return it->second;
    }
    return -1;
}

static int FindPelvisFallbackByTopology(const std::vector<Bone>& skel)
{
    // parent=-1인 루트 후보 중, 자식/손자에 "Leg/Thigh/UpLeg" 같은 패턴이 있는 쪽을 골라본다.
    auto hasLegHint = [&](int idx)->bool
        {
            const std::string& nm = skel[idx].name;
            return (nm.find("Leg") != std::string::npos) ||
                (nm.find("Thigh") != std::string::npos) ||
                (nm.find("UpLeg") != std::string::npos);
        };

    for (int root = 0; root < (int)skel.size(); ++root)
    {
        if (skel[root].parentIndex != -1) continue;

        // root의 직계 자식 중 다리 힌트가 2개 이상이면 pelvis로 간주
        int legChildCount = 0;
        for (int i = 0; i < (int)skel.size(); ++i)
            if (skel[i].parentIndex == root && hasLegHint(i))
                legChildCount++;

        if (legChildCount >= 2) return root;
    }
    return -1;
}

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

    // 너 Evaluate가 쓰는 규칙과 동일: S * R * T
    XMMATRIX M = mS * mR * mT;

    XMFLOAT4X4 out{};
    XMStoreFloat4x4(&out, M);
    return out;
}



// ============================================================
// SetSkeleton
//   - 메시에서 본 계층과 offsetMatrix를 가져와 저장
//   - 본 개수에 맞춰 내부 버퍼 크기 초기화
// ============================================================
void CAnimator::SetSkeleton(const std::vector<Bone>& bones,
    const std::unordered_map<std::string, int>& boneNameToIndex)
{
    m_Skeleton = bones;
    m_BoneNameToIndex = boneNameToIndex;

    int boneCount = (int)bones.size();

    // 내부 포즈/최종 행렬 버퍼 크기 재설정
    m_LocalPose.resize(boneCount);
    m_GlobalPose.resize(boneCount);
    m_FinalBoneMatrices.resize(boneCount);

    // 기본값: identity
    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());

    for (int i = 0; i < boneCount; ++i)
    {
        m_LocalPose[i] = identity;
        m_GlobalPose[i] = identity;
        m_FinalBoneMatrices[i] = identity;
    }

    // 1) Pelvis(=Hips) 찾기
    m_iPelvis = FindByNameCandidates(m_BoneNameToIndex, {
        "Bind_Hips", "Hips", "Pelvis",
        "mixamorig:Hips",
        "Base_HumanPelvis" // 너의 좀비 계열
        });
    if (m_iPelvis < 0)
        m_iPelvis = FindPelvisFallbackByTopology(m_Skeleton);

    // 2) Spine root 찾기(끊겨있는 루트 스파인)
    m_iSpineRoot = FindByNameCandidates(m_BoneNameToIndex, {
        "Bind_Spine", "Spine", "Spine1",
        "Base_HumanSpine1"
        });

    // (선택) 로그
    DBG_PrintF("[SetSkeleton] pelvis=%d spineRoot=%d\n", m_iPelvis, m_iSpineRoot);

}

// ============================================================
// AddClip
//   - 애니메이션 클립 등록
// ============================================================
void CAnimator::AddClip(const AnimationClip& clip)
{
    m_Clips[clip.name] = clip;
}

// ============================================================
// HasClip
// ============================================================
bool CAnimator::HasClip(const std::string& name) const
{
    return (m_Clips.find(name) != m_Clips.end());
}

// ============================================================
// Play
//   - 클립 재생 시작
// ============================================================
bool CAnimator::Play(const std::string& clipName, bool loop, float startTime)
{
    AnimationClip* clip = FindClipPtr(clipName);
    if (!clip) return false;

    m_CurrentClipName = clipName;
    m_fCurrentTime = startTime;
    m_bLoop = loop;
    m_bPlaying = true;

    // 블렌딩 중이면 취소
    m_bBlending = false;
    m_NextClipName.clear();
    m_fBlendElapsed = 0.0f;
    m_fBlendDuration = 0.0f;

    // 첫 포즈 즉시 적용
    clip->Evaluate(m_fCurrentTime, m_Skeleton, m_LocalPose);
    BuildGlobalAndFinalFromLocal();

    m_NextClipAfterEnd.clear();
    return true;
}


// ============================================================
// Stop
// ============================================================
void CAnimator::Stop()
{
    m_bPlaying = false;
    m_fCurrentTime = 0.0f;
}

// ============================================================
// SetTime
//   - 외부에서 강제로 재생 시간을 지정
// ============================================================
void CAnimator::SetTime(float timeSec)
{
    m_fCurrentTime = timeSec;
}

// ============================================================
// Update
//   - dt만큼 시간 증가
//   - 클립 범위 벗어나면 loop 처리
//   - 본 행렬 계산
// ============================================================
void CAnimator::Update(float dt)
{
    if (!m_bPlaying) return;

    AnimationClip* cur = FindClipPtr(m_CurrentClipName);
    if (!cur) return;

    const int boneCount = (int)m_Skeleton.size();
    if (boneCount <= 0) return;

    if (!m_bBlending)
    {
        // 기존 단일 재생
        AdvanceTime(cur, m_fCurrentTime, dt, m_bLoop);

        // (기존의 "끝 처리 + NextClipAfterEnd"는 유지하려면 여기서 따로 처리)
        // 지금은 최소 패치라 loop=false면 끝 프레임 유지로만 둠.

        cur->Evaluate(m_fCurrentTime, m_Skeleton, m_LocalPose);
        BuildGlobalAndFinalFromLocal();
        return;
    }

    // ===== 블렌딩 중 =====
    AnimationClip* nxt = FindClipPtr(m_NextClipName);
    if (!nxt)
    {
        // next가 사라졌으면 블렌딩 취소
        m_bBlending = false;
        return;
    }

    // 두 클립 모두 시간 진행
    AdvanceTime(cur, m_fCurrentTime, dt, m_bLoop);
    AdvanceTime(nxt, m_fNextTime, dt, m_bNextLoop);

    // A/B 포즈 평가
    cur->Evaluate(m_fCurrentTime, m_Skeleton, m_LocalPoseA);
    nxt->Evaluate(m_fNextTime, m_Skeleton, m_LocalPoseB);

    // alpha 계산
    m_fBlendElapsed += dt;
    float alpha = (m_fBlendDuration > 0.0f) ? (m_fBlendElapsed / m_fBlendDuration) : 1.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    // 로컬 포즈 블렌딩
    BlendLocalPosesTRS(m_LocalPoseA, m_LocalPoseB, alpha, m_LocalPose);

    // 글로벌/스킨 계산
    BuildGlobalAndFinalFromLocal();

    // 블렌딩 종료 -> next를 current로 승격
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
        else      time = clip->duration; // 끝 프레임 유지
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

    // --- 너의 primaryRoot 찾기 그대로 ---
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

    // ---- GlobalPose ----
    for (int i = 0; i < boneCount; ++i)
    {
        int parent = m_Skeleton[i].parentIndex;
        XMMATRIX local = XMLoadFloat4x4(&m_LocalPose[i]);

        if (i == primaryRoot)
        {
            XMStoreFloat4x4(&m_GlobalPose[i], local);
            continue;
        }

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

    // ---- Final ----
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

    // current가 없으면 그냥 Play
    if (!m_bPlaying || m_CurrentClipName.empty())
        return Play(nextClipName, loop, startTime);

    // 같은 클립이면 무시
    if (m_CurrentClipName == nextClipName)
        return true;

    // blendTime <= 0 이면 스냅 전환
    if (blendTimeSec <= 0.0f)
        return Play(nextClipName, loop, startTime);

    m_bBlending = true;
    m_NextClipName = nextClipName;
    m_fNextTime = startTime;
    m_bNextLoop = loop;

    m_fBlendElapsed = 0.0f;
    m_fBlendDuration = blendTimeSec;

    // 버퍼 크기 보장
    const int n = (int)m_Skeleton.size();
    if ((int)m_LocalPoseA.size() != n) m_LocalPoseA.resize(n);
    if ((int)m_LocalPoseB.size() != n) m_LocalPoseB.resize(n);

    return true;
}



// ============================================================
// GetCurrentClipName
// ============================================================
const std::string& CAnimator::GetCurrentClipName() const
{
    return m_CurrentClipName;
}

// ============================================================
// GetFinalBoneMatrices
//   - CMesh가 GPU CBV 업데이트에 사용
// ============================================================
const std::vector<XMFLOAT4X4>& CAnimator::GetFinalBoneMatrices() const
{
    return m_FinalBoneMatrices;
}
