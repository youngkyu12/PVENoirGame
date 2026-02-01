//------------------------------------------------------- ----------------------
// File: AnimatorData.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "AnimatorData.h"

using namespace DirectX;

static void BuildTRSMatrix(
    const XMFLOAT3& t,
    const XMFLOAT4& r,
    const XMFLOAT3& s,
    XMFLOAT4X4& outM)
{
    XMVECTOR trans = XMLoadFloat3(&t);
    XMVECTOR rot = XMLoadFloat4(&r);
    XMVECTOR scale = XMLoadFloat3(&s);

    XMMATRIX mS = XMMatrixScalingFromVector(scale);
    XMMATRIX mR = XMMatrixRotationQuaternion(rot);
    XMMATRIX mT = XMMatrixTranslationFromVector(trans);

    // (Scale * Rotate * Translate) 순서
    XMMATRIX M = mS * mR * mT;
    XMStoreFloat4x4(&outM, M);
}

static void DecomposeTRS(const XMFLOAT4X4& M, XMFLOAT3& outT, XMFLOAT4& outR, XMFLOAT3& outS)
{
    XMMATRIX m = XMLoadFloat4x4(&M);

    XMVECTOR S, R, T;
    if (!XMMatrixDecompose(&S, &R, &T, m))
    {
        // 실패 시 안전값
        outT = XMFLOAT3(0, 0, 0);
        outR = XMFLOAT4(0, 0, 0, 1);
        outS = XMFLOAT3(1, 1, 1);
        return;
    }
    XMStoreFloat3(&outS, S);
    XMStoreFloat4(&outR, XMQuaternionNormalize(R));
    XMStoreFloat3(&outT, T);
}


// 한 본의 키프레임 리스트에서 t에 해당하는 TRS를 보간해서 구함
static void SampleBoneTrack(
    const std::vector<Keyframe>& keys,
    float timeSec,
    XMFLOAT3& outT,
    XMFLOAT4& outR,
    XMFLOAT3& outS)
{
    const size_t keyCount = keys.size();
    if (keyCount == 0)
    {
        // 키가 없으면 단위 TRS
        outT = XMFLOAT3(0.f, 0.f, 0.f);
        outR = XMFLOAT4(0.f, 0.f, 0.f, 1.f);
        outS = XMFLOAT3(1.f, 1.f, 1.f);
        return;
    }

    if (keyCount == 1)
    {
        // 키가 하나면 그대로 사용
        outT = keys[0].translation;
        outR = keys[0].rotationQuat;
        outS = keys[0].scale;
        return;
    }

    // timeSec을 키 범위 안으로 clamp
    float startTime = keys.front().timeSec;
    float endTime = keys.back().timeSec;
    if (timeSec <= startTime) timeSec = startTime;
    if (timeSec >= endTime)   timeSec = endTime;

    // timeSec이 들어갈 구간 [k0, k1]을 찾기
    size_t k1 = 1;
    for (; k1 < keyCount; ++k1)
    {
        if (keys[k1].timeSec >= timeSec)
            break;
    }
    if (k1 >= keyCount)
    {
        // safety: 마지막 키 사용
        outT = keys.back().translation;
        outR = keys.back().rotationQuat;
        outS = keys.back().scale;
        return;
    }

    size_t k0 = k1 - 1;
    const Keyframe& key0 = keys[k0];
    const Keyframe& key1 = keys[k1];

    float t0 = key0.timeSec;
    float t1 = key1.timeSec;
    float span = (t1 - t0);
    float alpha = (span > 0.0f) ? ((timeSec - t0) / span) : 0.0f;

    // 위치 / 스케일: 선형보간
    XMVECTOR T0 = XMLoadFloat3(&key0.translation);
    XMVECTOR T1 = XMLoadFloat3(&key1.translation);
    XMVECTOR S0 = XMLoadFloat3(&key0.scale);
    XMVECTOR S1 = XMLoadFloat3(&key1.scale);

    XMVECTOR T = XMVectorLerp(T0, T1, alpha);
    XMVECTOR S = XMVectorLerp(S0, S1, alpha);

    XMStoreFloat3(&outT, T);
    XMStoreFloat3(&outS, S);

    // 회전: 쿼터니언 SLERP
    XMVECTOR R0 = XMLoadFloat4(&key0.rotationQuat);
    XMVECTOR R1 = XMLoadFloat4(&key1.rotationQuat);
    XMVECTOR R = XMQuaternionSlerp(R0, R1, alpha);
    R = XMQuaternionNormalize(R);
    XMStoreFloat4(&outR, R);
}

// ============================================================
// AnimationClip::Evaluate
//   - timeSec 시각에서 각 본의 "로컬 행렬(animLocal)"을 outLocalTransforms 에 채움
//   - 키프레임이 없는 본은 skeleton[i].bindLocal 사용
//   - 수정: 더 이상 corrected = bindInv * anim * bind 형식 사용 안 함
//            SampleBoneTrack 의 TRS를 "그대로 로컬 변환"으로 사용.
// ============================================================
void AnimationClip::Evaluate(
    float timeSec,
    const std::vector<Bone>& skeleton,
    std::vector<XMFLOAT4X4>& outLocalTransforms) const
{
    const size_t skeletonCount = skeleton.size();
    if (skeletonCount == 0) { outLocalTransforms.clear(); return; }
    if (outLocalTransforms.size() < skeletonCount) outLocalTransforms.resize(skeletonCount);

    // 0) Bind_Root 트랙 샘플(있으면)
    bool hasRoot = hasBindRootTrack && !bindRootTrack.keyframes.empty();
    XMMATRIX rootM = XMMatrixIdentity();
    if (hasRoot)
    {
        XMFLOAT3 t; XMFLOAT4 r; XMFLOAT3 s;
        SampleBoneTrack(bindRootTrack.keyframes, timeSec, t, r, s);
        XMFLOAT4X4 rootLocal;
        BuildTRSMatrix(t, r, s, rootLocal);
        rootM = XMLoadFloat4x4(&rootLocal);
    }

    // 1) 본별 로컬 포즈
    for (size_t i = 0; i < skeletonCount; ++i)
    {
        const std::string& boneName = skeleton[i].name;

        auto it = boneNameToTrack.find(boneName);
        if (it == boneNameToTrack.end())
        {
            outLocalTransforms[i] = skeleton[i].bindLocal;
            continue;
        }

        const int trackIndex = it->second;
        const auto& track = boneTracks[trackIndex];

        if (track.keyframes.empty())
        {
            outLocalTransforms[i] = skeleton[i].bindLocal;
            continue;
        }

        XMFLOAT3 t; XMFLOAT4 r; XMFLOAT3 s;
        SampleBoneTrack(track.keyframes, timeSec, t, r, s);

        XMFLOAT4X4 localM;
        BuildTRSMatrix(t, r, s, localM);

        // Bind_Root를 “부모”처럼 적용: 루트 본들(parent=-1)에만
        if (hasRoot && skeleton[i].parentIndex < 0)
        {
            XMMATRIX L = XMLoadFloat4x4(&localM);
            L = L * rootM; // (엔진 규칙: global = local * parent)
            XMStoreFloat4x4(&localM, L);
        }

        outLocalTransforms[i] = localM;
    }
}
