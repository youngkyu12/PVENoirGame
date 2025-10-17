#pragma once
#include <vector>
#include <DirectXMath.h>

using namespace DirectX;

struct BoneTransform {
    int        parent = -1;           // 부모 인덱스(-1이면 루트)
    XMFLOAT4X4 inverseBind{};         // inverse(bindPose)
    XMFLOAT4X4 local{};               // 현재 프레임 로컬(S*R*T)
    XMFLOAT4X4 global{};              // 계층 누적 월드
};

struct Keyframe {
    float     t = 0.f;
    XMFLOAT3  T = { 0,0,0 };
    XMFLOAT4  R = { 0,0,0,1 };          // quaternion(x,y,z,w)
    XMFLOAT3  S = { 1,1,1 };
};
struct BoneTrack {
    std::vector<Keyframe> keys;       // 시간 오름차순
};

class CAnimationClip {
public:
    float duration = 0.f;
    std::vector<BoneTrack> tracks;    // [boneIndex]

    // time에 대한 본별 local(S*R*T) 행렬을 out[i].local에 기록
    void Evaluate(float time, std::vector<BoneTransform>& out) const;
};

class CAnimator {
public:
    std::vector<BoneTransform> bones; // skeleton + 런타임 값
    CAnimationClip* clip = nullptr;
    float time = 0.f;

    void  SetSkeleton(const std::vector<BoneTransform>& s) { bones = s; }
    void  SetClip(CAnimationClip* c) { clip = c; time = 0.f; }
    void  Update(float dt);                      // time 누적 + Evaluate + 계층 전파
    std::vector<XMFLOAT4X4> GetSkinMatrices() const; // skin = global * inverseBind (셰이더용)

    size_t GetBoneCount() const { return bones.size(); }
};
