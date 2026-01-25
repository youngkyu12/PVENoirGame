#include "stdafx.h"
#include "Animator.h"

// Animator.cpp 상단(또는 익명 namespace)에 추가
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
    auto it = m_Clips.find(clipName);
    if (it == m_Clips.end())
        return false;

    m_pCurrentClip = &it->second;
    m_fCurrentTime = startTime;
    m_bLoop = loop;
    m_bPlaying = true;

    // =====================================================
    // ★ 첫 프레임 포즈 즉시 적용 (T-포즈 → 첫 키프레임 보간 제거)
    // =====================================================
    if (m_pCurrentClip)
    {
        // 1) Local pose 계산
        m_pCurrentClip->Evaluate(
            m_fCurrentTime,
            m_Skeleton,
            m_LocalPose
        );

        // 2) Global pose 계산
        const int boneCount = (int)m_Skeleton.size();
        for (int i = 0; i < boneCount; ++i)
        {
            int parent = m_Skeleton[i].parentIndex;
            XMMATRIX local = XMLoadFloat4x4(&m_LocalPose[i]);

            if (parent < 0)
            {
                XMStoreFloat4x4(&m_GlobalPose[i], local);
            }
            else
            {
                XMMATRIX parentM = XMLoadFloat4x4(&m_GlobalPose[parent]);
                XMMATRIX global = local * parentM;

                XMStoreFloat4x4(&m_GlobalPose[i], global);
            }
        }

        // 3) Final bone matrices
        for (int i = 0; i < boneCount; ++i)
        {
            XMMATRIX global = XMLoadFloat4x4(&m_GlobalPose[i]);
            XMMATRIX offset = XMLoadFloat4x4(&m_Skeleton[i].offsetMatrix);
            XMMATRIX skin = offset * global;
            XMStoreFloat4x4(&m_FinalBoneMatrices[i], skin);
        }
    }
    m_NextClipAfterEnd.clear();

    /*
    {
        const int boneCount = (int)m_Skeleton.size();

        for (int i = 0; i < boneCount; ++i)
        {
            const Bone& b = m_Skeleton[i];
            const XMFLOAT4X4& g = m_GlobalPose[i];

            char buf[256];
            sprintf_s(buf,
                "[Animator::Play] Bone[%d] '%s'\n"
                "  Row0 = %.3f %.3f %.3f %.3f\n"
                "  Row1 = %.3f %.3f %.3f %.3f\n"
                "  Row2 = %.3f %.3f %.3f %.3f\n"
                "  Row3 = %.3f %.3f %.3f %.3f\n",
                i, b.name.c_str(),
                g.m[0][0], g.m[0][1], g.m[0][2], g.m[0][3],
                g.m[1][0], g.m[1][1], g.m[1][2], g.m[1][3],
                g.m[2][0], g.m[2][1], g.m[2][2], g.m[2][3],
                g.m[3][0], g.m[3][1], g.m[3][2], g.m[3][3]
            );

            OutputDebugStringA(buf);
        }
    }
    */

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
    if (!m_bPlaying || !m_pCurrentClip)
        return;

    // 시간 증가
    m_fCurrentTime += dt;
    //m_fCurrentTime += 0.001f;


    // 1) 현재 클립이 끝났는지 검사
    if (m_fCurrentTime >= m_pCurrentClip->duration)
    {
        if (m_bLoop)
        {
            // 루프 재생이면 0으로 되감기
            m_fCurrentTime = fmodf(m_fCurrentTime, m_pCurrentClip->duration);
        }
        else
        {
            // loop = false → 더 이상 계속되지 않음
            if (!m_NextClipAfterEnd.empty())
            {
                // 다음 클립으로 전환
                Play(m_NextClipAfterEnd, true, 0.0f);
                return;
            }
            else
            {
                // 아무것도 설정 안 했으면 끝 프레임 유지
                m_fCurrentTime = m_pCurrentClip->duration;
                return;
            }
        }
    }

    const int boneCount = (int)m_Skeleton.size();
    if (boneCount <= 0) return;

    DBG_PrintF("[Animator::Update] clip=%s bones=%d time=%.3f\n",
        (m_pCurrentClip ? m_pCurrentClip->name.c_str() : "null"),
        boneCount, m_fCurrentTime);

    static bool once = false;
    if (!once)
    {
        once = true;
        for (int i = 0; i < boneCount && i < 40; ++i)
            DBG_PrintF("  Bone[%d] '%s' parent=%d\n", i, m_Skeleton[i].name.c_str(), m_Skeleton[i].parentIndex);
    }



    // 1) 로컬 포즈 계산
    m_pCurrentClip->Evaluate(
        m_fCurrentTime,
        m_Skeleton,
        m_LocalPose
    );


    // 2) 글로벌 포즈 계산 (부모-자식 연결)
    // Evaluate 직후, GlobalPose 계산 직전에 추가:
    auto FindPrimaryRoot = [&]() -> int
        {
            // 루트 후보 수집
            std::vector<int> roots;
            for (int i = 0; i < boneCount; ++i)
                if (m_Skeleton[i].parentIndex < 0)
                    roots.push_back(i);

            if (roots.empty()) return 0;
            if (roots.size() == 1) return roots[0];

            // 가장 “자식이 많은” 루트를 primary로 (이름 하드코딩 회피)
            auto CountDesc = [&](int root) -> int
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

    // (선택) RootDeltaFix는 “primaryRoot”에만 적용 (당신이 이미 해둔 방향 유지)
    // -> 이 부분은 당신의 현재 코드 유지하되, 조건을 boneName==Bind_Hips가 아니라 i==primaryRoot로 바꾸면 됨.

    // ---- GlobalPose 계산을 이 루프로 통일 ----
    for (int i = 0; i < boneCount; ++i)
    {
        int parent = m_Skeleton[i].parentIndex;
        XMMATRIX local = XMLoadFloat4x4(&m_LocalPose[i]);

        if (i == primaryRoot)
        {
            XMStoreFloat4x4(&m_GlobalPose[i], local);
            continue;
        }

        // secondary root면 primary에 붙이기
        if (parent < 0)
        {
            // local(현재는 absolute처럼 들어온 것)을 primary 기준 상대 로컬로 변환
            XMMATRIX primaryG = XMLoadFloat4x4(&m_GlobalPose[primaryRoot]);
            XMMATRIX invPrimaryG = XMMatrixInverse(nullptr, primaryG);
            local = local * invPrimaryG;
            parent = primaryRoot;
        }

        XMMATRIX parentM = XMLoadFloat4x4(&m_GlobalPose[parent]);
        XMMATRIX global = local * parentM;
        XMStoreFloat4x4(&m_GlobalPose[i], global);
    }

    // 3) 최종 본 행렬 = offsetMatrix * globalTransform
    for (int i = 0; i < boneCount; ++i)
    {
        XMMATRIX global = XMLoadFloat4x4(&m_GlobalPose[i]);
        XMMATRIX offset = XMLoadFloat4x4(&m_Skeleton[i].offsetMatrix);

        XMMATRIX skin = offset * global;
        XMStoreFloat4x4(&m_FinalBoneMatrices[i], skin);
    }


}


// ============================================================
// GetCurrentClipName
// ============================================================
const std::string& CAnimator::GetCurrentClipName() const
{
    static std::string empty = "";
    if (!m_pCurrentClip) return empty;
    return m_pCurrentClip->name;
}

// ============================================================
// GetFinalBoneMatrices
//   - CMesh가 GPU CBV 업데이트에 사용
// ============================================================
const std::vector<XMFLOAT4X4>& CAnimator::GetFinalBoneMatrices() const
{
    return m_FinalBoneMatrices;
}
