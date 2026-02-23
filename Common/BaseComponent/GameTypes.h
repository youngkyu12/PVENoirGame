//-----------------------------------------------------------------------------
// File: GameTypes.h
// 네트워크 동기화용 데이터 타입
// DirectX 의존성 없음
//-----------------------------------------------------------------------------
#pragma once

#include <cstdint>
#include "GameMath.h"

//-----------------------------------------------------------------------------
// NetworkTransform: 네트워크로 전송되는 위치/회전 정보
//-----------------------------------------------------------------------------
struct NetworkTransform
{
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
    float yaw = 0.f;  // degrees

    NetworkTransform() = default;
    NetworkTransform(float _x, float _y, float _z, float _yaw = 0.f)
        : x(_x), y(_y), z(_z), yaw(_yaw) {}

    // GameMath::Vec3 변환
    GameMath::Vec3 ToVec3() const { return { x, y, z }; }
    void FromVec3(const GameMath::Vec3& v) { x = v.x; y = v.y; z = v.z; }

    // 편의 함수
    void SetPosition(float _x, float _y, float _z) { x = _x; y = _y; z = _z; }
    void SetYaw(float _yaw) { yaw = GameMath::NormalizeYaw(_yaw); }
};

//-----------------------------------------------------------------------------
// NetworkAnimState: 애니메이션 상태 동기화용
//-----------------------------------------------------------------------------
struct NetworkAnimState
{
    uint8_t state = 0;    // 0=Idle, 1=Run, 2=Attack, 3=Hit, 4=Die...
    float   time = 0.f;   // 애니메이션 진행 시간

    NetworkAnimState() = default;
    NetworkAnimState(uint8_t _state, float _time = 0.f)
        : state(_state), time(_time) {}
};

//-----------------------------------------------------------------------------
// NetworkPlayerState: 플레이어 전체 상태 (패킷용)
//-----------------------------------------------------------------------------
struct NetworkPlayerState
{
    uint64_t         playerId = 0;
    NetworkTransform transform;
    NetworkAnimState anim;
    int32_t          inputKeys = 0;  // 현재 입력 상태 (옵션)

    NetworkPlayerState() = default;

    // GameMath 타입에서 생성
    static NetworkPlayerState FromGameMath(
        uint64_t id,
        const GameMath::Vec3& pos,
        float yaw,
        uint8_t animState = 0,
        float animTime = 0.f)
    {
        NetworkPlayerState state;
        state.playerId = id;
        state.transform.FromVec3(pos);
        state.transform.yaw = yaw;
        state.anim.state = animState;
        state.anim.time = animTime;
        return state;
    }
};

//-----------------------------------------------------------------------------
// NetworkEnemyState: 적 상태 (패킷용)
//-----------------------------------------------------------------------------
struct NetworkEnemyState
{
    uint64_t         enemyId = 0;
    uint32_t         enemyType = 0;
    NetworkTransform transform;
    NetworkAnimState anim;
    float            health = 100.f;

    NetworkEnemyState() = default;
};

//-----------------------------------------------------------------------------
// 애니메이션 상태 enum (참조용)
//-----------------------------------------------------------------------------
namespace AnimStateType
{
    constexpr uint8_t Idle   = 0;
    constexpr uint8_t Run    = 1;
    constexpr uint8_t Attack = 2;
    constexpr uint8_t Hit    = 3;
    constexpr uint8_t Die    = 4;
}
