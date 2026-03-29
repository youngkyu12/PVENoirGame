#pragma once
#include <queue>
#include <mutex>
#include <variant>
#include <DirectXMath.h>

#include "GlobalEnum.h"

#include "PlayerEquipmentComponent.h"

using namespace DirectX;

// ============================================================
// Animation 상태
// ============================================================


struct AnimationState
{
    uint32_t      stateCode = 0;
    int           animTick; // 애니메이션 현재 틱 (0 ~ maxTick)
};

// ============================================================
// 공통 Actor 상태
// ============================================================
struct ActorState
{
    uint64_t    id;
    XMFLOAT3    position;
    float       yaw;
	AnimationState animation;
	EWeaponType weaponType;
    // 필요시 HP, 상태 등 추가
};

// ============================================================
// 총알 상태
// ============================================================

struct BulletState
{
    uint64_t    id;
    XMFLOAT3    position;
    XMFLOAT3    velocity;
    // 필요시 추가 정보
};

// ============================================================
// 캐릭터 무기 선택용 초기화 데이터
// ============================================================

struct LoadoutData
{
    uint64_t playerId;
    EWeaponType weaponType;
};

// ============================================================
// S_GAME_START용 초기화 데이터
// ============================================================
struct GameStartData
{
    std::vector<ActorState> players;
    std::vector<ActorState> enemies;
	std::vector<ActorState> buildings; // 필요시 추가
};

// ============================================================
// S_FRAME_STATE용 프레임 스냅샷
// ============================================================
struct FrameSnapshot
{
	uint64_t frameId; // serverTick과 동일한 값
    std::vector<ActorState> players;
    std::vector<ActorState> enemies;
    std::vector<BulletState> bullets;
};

// ============================================================
// 네트워크 메시지 (variant로 타입 구분)
// ============================================================
enum class NetworkMessageType
{
    Loadout,
    GameStart,
    FrameState
};

struct NetworkMessage
{
    NetworkMessageType type;
    std::variant<LoadoutData, GameStartData, FrameSnapshot> data;
};

// ============================================================
// NetworkQueue
// ============================================================
class NetworkQueue
{
public:

    void PushLoadout(LoadoutData&& loadout)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messages.push({ NetworkMessageType::Loadout, std::move(loadout) });
    }

    // 네트워크 스레드에서 호출
    void PushGameStart(GameStartData&& data)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messages.push({ NetworkMessageType::GameStart, std::move(data) });
    }

    void PushFrameState(FrameSnapshot&& snapshot)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_messages.push({ NetworkMessageType::FrameState, std::move(snapshot) });
    }


    void Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::queue<NetworkMessage> empty;
        std::swap(m_messages, empty);
	}

    // 게임 스레드에서 호출
    bool TryPop(NetworkMessage& out)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_messages.empty()) return false;
        out = std::move(m_messages.front());
        m_messages.pop();
        return true;
    }

    bool IsEmpty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_messages.empty();
    }

private:
    std::queue<NetworkMessage> m_messages;
    mutable std::mutex m_mutex;
};