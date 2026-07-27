#include "pch.h"
#include "Player.h"
#include "ColliderComponent.h"

namespace
{
    constexpr int kSwordAttackAnimTicks = 19; // ceil(1.383333s / 0.060s)
    constexpr int kAxeAttackAnimTicks = 27;   // ceil(1.583333s / 0.060s)
    constexpr int kDefaultAttackAnimTicks = 10;
    constexpr int kPlayerHitAnimTicks = 4;     // short hit-stun; long values make damage lock feel excessive
    constexpr int kRollAnimTicks = 12; // ceil((1.516667s * (0.55 - 0.08)) / 0.060s)

    int GetPlayerAttackAnimTicks(Protocol::WeaponType weaponType, const CWeapon& weapon)
    {
        switch (weaponType)
        {
        case Protocol::WEAPON_TYPE_SWORD:
            return kSwordAttackAnimTicks;
        case Protocol::WEAPON_TYPE_AXE:
            return kAxeAttackAnimTicks;
        case Protocol::WEAPON_TYPE_BOW:
            return weapon.IsAttacking() ? 0 : kDefaultAttackAnimTicks;
        default:
            return kDefaultAttackAnimTicks;
        }
    }
}

void Player::Update(uint32 serverTick)
{
    if (m_lifeState == EPlayerLifeState::DeadAnimating)
    {
        SetVelocity(GameMath::Vec3::Zero());

        constexpr uint32 kRespawnDelayTicks = 32; // ~5s (160ms * 32)
        if (serverTick >= m_deathTick + kRespawnDelayTicks)
        {
            OnRespawnEnter(serverTick);
        }
        return;
    }

    const bool keepVelocity = (m_animState == Protocol::ANIMATION_TYPE_ROLL);
    Move(m_velocity);
    if (!keepVelocity)
        SetVelocity(GameMath::Vec3::Zero());

    if (m_animState != Protocol::ANIMATION_TYPE_IDLE)
    {
        int animDuration = 0;
        switch (m_animState)
        {
        case Protocol::ANIMATION_TYPE_WALK:   animDuration = 15; break;
        case Protocol::ANIMATION_TYPE_RUN:    animDuration = 10; break;
        case Protocol::ANIMATION_TYPE_ATTACK:
            animDuration = GetPlayerAttackAnimTicks(weapon.GetWeaponState(), weapon);
            break;
        case Protocol::ANIMATION_TYPE_ROLL:   animDuration = kRollAnimTicks;  break;
        case Protocol::ANIMATION_TYPE_DIE:    animDuration = 25; break;
        case Protocol::ANIMATION_TYPE_HIT:
            animDuration = (m_hitEndTick > GetAnimTick())
                ? static_cast<int>(m_hitEndTick - GetAnimTick())
                : kPlayerHitAnimTicks;
            break;
        default: animDuration = 0; break;
        }

        if (animDuration > 0)
        {
            int elapsedTicks = serverTick - GetAnimTick();
            if (elapsedTicks >= animDuration &&
                m_animState != Protocol::ANIMATION_TYPE_DIE &&
                m_animState != Protocol::ANIMATION_TYPE_WALK &&
                m_animState != Protocol::ANIMATION_TYPE_RUN)
            {
                SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
                SetAnimTick(serverTick);
                SetVelocity(GameMath::Vec3::Zero());
            }
        }
    }
}

void Player::Build()
{
    ClearPendingPortalTeleport();
	ClearPendingForcedTransform();
	SetTerrainSnapSuppressed(false);
    SetPosition(0.0f, 0.0f, 0.0f);
    Rotate(0.0f, 0.0f, 0.0f);
    weapon.SetWeapon(Protocol::WEAPON_TYPE_SWORD, 0);
}

void Player::ApplyHit(uint32 serverTick, int damage, uint32 hitDurationTicks, uint64 serverMs)
{
    if (IsDead()) return;
    if (IsRollInvincible()) return;

    weapon.CancelAttack();
    TakeDamage(ApplyDefenseBuffToIncomingDamage(damage, serverMs));

    if (IsDead())
    {
        OnDeathEnter(serverTick);
        return;
    }

    const uint32 clampedHitDurationTicks =
        std::min<uint32>(hitDurationTicks, kPlayerHitAnimTicks);
    SetAnimState(Protocol::ANIMATION_TYPE_HIT);
    SetAnimTick(serverTick);
    m_hitEndTick = serverTick + clampedHitDurationTicks;
}

void Player::ApplyEnvironmentalDamage(uint32 serverTick, int damage, uint64 serverMs)
{
    if (IsDead() || damage <= 0) return;

    TakeDamage(ApplyDefenseBuffToIncomingDamage(damage, serverMs));
    if (IsDead())
        OnDeathEnter(serverTick);
}

void Player::OnDeathEnter(uint32 serverTick)
{
    m_lifeState = EPlayerLifeState::DeadAnimating;
    weapon.CancelAttack();

    SetAnimState(Protocol::ANIMATION_TYPE_DIE);
    SetAnimTick(serverTick);
    SetVelocity(GameMath::Vec3::Zero());
    ClearMoveKeyCodes();
    ClearPendingPortalTeleport();
	ClearPendingForcedTransform();
	SetTerrainSnapSuppressed(false);
    m_deathTick = serverTick;

    if (auto* collider = GetComponent<CColliderComponent>())
    {
        collider->OnUpdate(0.0f);
    }

    // active는 참여 상태와 분리: 여기서 SetActive(false) 하지 않음
}

void Player::OnRespawnEnter(uint32 serverTick)
{
	const bool isActualRespawn = (m_lifeState == EPlayerLifeState::DeadAnimating);
    m_lifeState = EPlayerLifeState::Alive;

    ResetHpToMax();
	const float prevYaw = GetYaw();
    SetPosition(m_initialSpawnPosition);
	SetYaw(180.0f);
	if (isActualRespawn)
	{
		QueueForcedTransformYawDelta(
			GameMath::NormalizeYaw(GetYaw() - prevYaw),
			Protocol::FORCED_TRANSFORM_REASON_RESPAWN);
	}
    SetVelocity(GameMath::Vec3::Zero());
    ClearMoveKeyCodes();
    ClearPendingPortalTeleport();
	SetTerrainSnapSuppressed(false);
    SetAnimState(Protocol::ANIMATION_TYPE_IDLE);
    SetAnimTick(serverTick);

    if (auto* collider = GetComponent<CColliderComponent>())
    {
        collider->OnUpdate(0.0f);
    }
}

// 기존 Respawn 호출부와 호환하려면 래퍼 유지
void Player::Respawn(uint32 serverTick)
{
    OnRespawnEnter(serverTick);
}

int Player::GetInventoryCount(Protocol::ItemType kind) const
{
    const int slot = static_cast<int>(kind) - 1;
    if (slot < 0 || slot >= kInventorySlotCount) return 0;
    return m_inventoryCounts[static_cast<size_t>(slot)];
}

void Player::AddInventoryItem(Protocol::ItemType kind)
{
    const int slot = static_cast<int>(kind) - 1;
    if (slot < 0 || slot >= kInventorySlotCount) return;
    ++m_inventoryCounts[static_cast<size_t>(slot)];
}

bool Player::UseInventoryItem(Protocol::ItemType kind, uint64 serverMs)
{
    const int slot = static_cast<int>(kind) - 1;
    if (slot < 0 || slot >= kInventorySlotCount) return false;
    if (m_inventoryCounts[static_cast<size_t>(slot)] <= 0) return false;

    switch (kind)
    {
    case Protocol::ITEM_TYPE_HEAL_POTION:
        if (GetCurrentHp() >= GetMaxHp()) return false;
        --m_inventoryCounts[static_cast<size_t>(slot)];
        Heal(kHealPotionAmount);
        break;
    case Protocol::ITEM_TYPE_ATTACK_POWER_POTION:
        if (IsAttackBuffActive(serverMs)) return false;
        --m_inventoryCounts[static_cast<size_t>(slot)];
        m_attackBuffEndMs = serverMs + kBuffDurationMs;
        break;
    case Protocol::ITEM_TYPE_DEFENSE_POTION:
        if (IsDefenseBuffActive(serverMs)) return false;
        --m_inventoryCounts[static_cast<size_t>(slot)];
        m_defenseBuffEndMs = serverMs + kBuffDurationMs;
        break;
    case Protocol::ITEM_TYPE_MOVE_SPEED_POTION:
        if (IsSpeedBuffActive(serverMs)) return false;
        --m_inventoryCounts[static_cast<size_t>(slot)];
        m_speedBuffEndMs = serverMs + kBuffDurationMs;
        break;
    default:
        return false;
    }
    return true;
}

int Player::ApplyAttackBuffToDamage(int damage, uint64 serverMs) const
{
    if (damage <= 0) return damage;
    return IsAttackBuffActive(serverMs) ? damage * kAttackBuffDamageMultiplier : damage;
}

int Player::ApplyDefenseBuffToIncomingDamage(int damage, uint64 serverMs) const
{
    if (damage <= 0) return damage;
    if (!IsDefenseBuffActive(serverMs)) return damage;

    const int scaledDamage = static_cast<int>(damage * kDefenseBuffIncomingDamageScale);
    return scaledDamage < 1 ? 1 : scaledDamage;
}

float Player::GetMoveSpeedMultiplier(uint64 serverMs) const
{
    return IsSpeedBuffActive(serverMs) ? kSpeedBuffMoveMultiplier : 1.0f;
}
