#pragma once
#include <queue>
#include <mutex>
#include <variant>
#include <string>
#include <cstdint>
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
// 아이템 상태
// ============================================================
struct InventoryEntryState
{
    uint32_t kind  = 0;
    int      count = 0;
};

struct ItemSpawnState
{
    uint64_t id   = 0;
    uint32_t kind = 0;
    XMFLOAT3 position{};
    bool active = false;
};

// ============================================================
// Actor 상태
// ============================================================
struct PlayerState
{
    uint64_t    id = 0;
    uint32_t    playerType = 0;
    uint32_t    hp = 0;
    XMFLOAT3    position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    float       yaw = 0.0f;
    AnimationState animation{};
    EWeaponType weaponType = EWeaponType::None;
    std::vector<InventoryEntryState> inventory;
};

struct EnemyState
{
    uint64_t    id = 0;
    uint32_t    enemyType = 0;
    uint32_t    hp = 0;
    XMFLOAT3    position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    float       yaw = 0.0f;
    AnimationState animation{};
    EWeaponType weaponType = EWeaponType::None;
	uint32_t    spawnFxType = 0;
	uint32_t    spawnFxTick = 0;
	uint32_t    spawnFxSerial = 0;
	uint32_t    lifecycleState = 0;
};

// ============================================================
// 총알 상태
// ============================================================

struct BulletState
{
    uint64_t    id;
    uint64_t    ownerId;
    uint32_t    bulletType = 0;
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
    std::vector<PlayerState>   players;
    std::vector<EnemyState>    enemies;
    std::vector<ItemSpawnState> items;
    std::string mapId;
};

// ============================================================
// S_FRAME_STATE용 프레임 스냅샷
// ============================================================
struct FrameSnapshot
{
    uint64_t frameId; // serverTick과 동일한 값
    std::vector<PlayerState>  players;
    std::vector<EnemyState>   enemies;
    std::vector<BulletState>  bullets;
    std::vector<ItemSpawnState> items;
    uint32_t bossRoomState = 0;
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

struct ForcedTransformEvent
{
    uint64_t playerId = 0;
    float yawDelta = 0.0f;
    uint32_t reason = 0;
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

    void PushForcedTransform(ForcedTransformEvent&& event)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_forcedTransforms.push(std::move(event));
    }


    void Clear()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::queue<NetworkMessage> empty;
        std::swap(m_messages, empty);
        std::queue<ForcedTransformEvent> emptyForcedTransforms;
        std::swap(m_forcedTransforms, emptyForcedTransforms);
	}

    // 게임 스레드에서 호출
    bool TryPop(NetworkMessage& out)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_messages.empty()) return false;

		// 큐가 빌 때까지 전부 뽑아서, 마지막 값을 적용하는 방식으로 변경함
		// 만약 이전 시기의 데이터를 사용해야 하는 상황을 위해 큐 형태 자체는 남겨놓는다
		while ( !m_messages.empty() )
		{
			out = std::move(m_messages.front());
			m_messages.pop();
        }
        return true;
    }

    bool TryPopForcedTransform(ForcedTransformEvent& out)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_forcedTransforms.empty())
            return false;

        out = std::move(m_forcedTransforms.front());
        m_forcedTransforms.pop();
        return true;
    }

    bool IsEmpty() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_messages.empty() && m_forcedTransforms.empty();
    }

private:
    std::queue<NetworkMessage> m_messages;
    std::queue<ForcedTransformEvent> m_forcedTransforms;
    mutable std::mutex m_mutex;
};
