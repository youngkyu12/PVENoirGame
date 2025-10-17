#include "stdafx.h"
#include "Animator.h"
#include <algorithm>

static inline XMMATRIX ComposeTRS(const XMFLOAT3& T, const XMFLOAT4& R, const XMFLOAT3& S)
{
    return XMMatrixScaling(S.x, S.y, S.z) *
        XMMatrixRotationQuaternion(XMLoadFloat4(&R)) *
        XMMatrixTranslation(T.x, T.y, T.z);
}

static inline void LerpKey(const Keyframe& k0, const Keyframe& k1, float t01,
    XMFLOAT3& T, XMFLOAT4& R, XMFLOAT3& S)
{
    XMStoreFloat3(&T, XMVectorLerp(XMLoadFloat3(&k0.T), XMLoadFloat3(&k1.T), t01));
    XMStoreFloat4(&R, XMQuaternionSlerp(XMLoadFloat4(&k0.R), XMLoadFloat4(&k1.R), t01));
    XMStoreFloat3(&S, XMVectorLerp(XMLoadFloat3(&k0.S), XMLoadFloat3(&k1.S), t01));
}

void CAnimationClip::Evaluate(float time, std::vector<BoneTransform>& out) const
{
    if (tracks.empty()) return;
    if (out.size() < tracks.size()) out.resize(tracks.size());

    // 루프 애니
    float t = duration > 0.f ? fmodf(time, duration) : time;

    for (size_t i = 0; i < tracks.size(); ++i)
    {
        const auto& tr = tracks[i];
        if (tr.keys.empty()) {
            XMStoreFloat4x4(&out[i].local, XMMatrixIdentity());
            continue;
        }
        if (tr.keys.size() == 1) {
            const auto& k = tr.keys[0];
            XMStoreFloat4x4(&out[i].local, ComposeTRS(k.T, k.R, k.S));
            continue;
        }
        // 구간 탐색
        size_t k = 0;
        while (k + 1 < tr.keys.size() && tr.keys[k + 1].t <= t) ++k;
        if (k + 1 >= tr.keys.size()) { // 마지막 구간 넘음 -> 마지막 키 유지
            const auto& last = tr.keys.back();
            XMStoreFloat4x4(&out[i].local, ComposeTRS(last.T, last.R, last.S));
            continue;
        }

        const auto& k0 = tr.keys[k];
        const auto& k1 = tr.keys[k + 1];
        float denom = (std::max)(1e-6f, (k1.t - k0.t));
        float a = (t - k0.t) / denom;  // 0..1

        XMFLOAT3 T; XMFLOAT4 R; XMFLOAT3 S;
        LerpKey(k0, k1, a, T, R, S);
        XMStoreFloat4x4(&out[i].local, ComposeTRS(T, R, S));
    }
}

void CAnimator::Update(float dt)
{
    if (!clip || bones.empty()) return;

    time += dt;
    clip->Evaluate(time, bones); // local 채움

    // 계층 누적: global = (parent==-1 ? local : local * global[parent])
    for (size_t i = 0; i < bones.size(); ++i)
    {
        XMMATRIX L = XMLoadFloat4x4(&bones[i].local);
        if (bones[i].parent < 0) {
            XMStoreFloat4x4(&bones[i].global, L);
        }
        else {
            XMMATRIX P = XMLoadFloat4x4(&bones[bones[i].parent].global);
            XMStoreFloat4x4(&bones[i].global, L * P);
        }
    }
}

std::vector<XMFLOAT4X4> CAnimator::GetSkinMatrices() const
{
    std::vector<XMFLOAT4X4> out;
    out.resize(bones.size());
    for (size_t i = 0; i < bones.size(); ++i)
    {
        XMMATRIX G = XMLoadFloat4x4(&bones[i].global);
        XMMATRIX IB = XMLoadFloat4x4(&bones[i].inverseBind);
        XMMATRIX M = G * IB;                   // skin = global * inverseBind

        // HLSL에서 mul(pos, M)을 쓰는 경우 보통 열-주도 매칭을 위해 전치가 필요
        XMStoreFloat4x4(&out[i], XMMatrixTranspose(M));
    }
    return out;
}
