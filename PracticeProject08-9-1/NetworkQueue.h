#pragma once
#include <queue>
#include <mutex>
#include <variant>
#include <DirectXMath.h>

using namespace DirectX;

// ============================================================
// 공통 Actor 상태
// ============================================================
struct ActorState
{
    uint64_t    id;
    XMFLOAT3    position;
    float       yaw;
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
    uint64_t frameId;
    std::vector<ActorState> players;
    std::vector<ActorState> enemies;
    std::vector<BulletState> bullets;
};

// ============================================================
// 네트워크 메시지 (variant로 타입 구분)
// ============================================================
enum class NetworkMessageType
{
    GameStart,
    FrameState
};

struct NetworkMessage
{
    NetworkMessageType type;
    std::variant<GameStartData, FrameSnapshot> data;
};

// ============================================================
// NetworkQueue
// ============================================================
class NetworkQueue
{
public:
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