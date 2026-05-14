#include "pch.h"
#include "Player.h"
#include "ColliderComponent.h"

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

    Move(m_velocity * ((GetAnimState() == Protocol::ANIMATION_TYPE_RUN) + 1));
    SetVelocity(GameMath::Vec3::Zero());

    if (m_animState != Protocol::ANIMATION_TYPE_IDLE)
    {
        int animDuration = 0;
        switch (m_animState)
        {
        case Protocol::ANIMATION_TYPE_WALK:   animDuration = 15; break;
        case Protocol::ANIMATION_TYPE_RUN:    animDuration = 10; break;
        case Protocol::ANIMATION_TYPE_ATTACK:
            animDuration =
                (weapon.GetWeaponState() == Protocol::WEAPON_TYPE_BOW && weapon.IsAttacking()) ? 0 : 10;
            break;
        case Protocol::ANIMATION_TYPE_ROLL:   animDuration = 1;  break;
        case Protocol::ANIMATION_TYPE_DIE:    animDuration = 25; break;
        case Protocol::ANIMATION_TYPE_HIT:    animDuration = 10; break;
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
            }
        }
    }
}

void Player::Build()
{
    SetPosition(0.0f, 0.0f, 0.0f);
    Rotate(0.0f, 0.0f, 0.0f);
    weapon.SetWeapon(Protocol::WEAPON_TYPE_SWORD, 0);
}

void Player::ApplyHit(uint32 serverTick, int damage, uint32 hitDurationTicks)
{
    if (IsDead()) return;

    weapon.CancelAttack();
    TakeDamage(damage);

    if (IsDead())
    {
        OnDeathEnter(serverTick);
        return;
    }

    SetAnimState(Protocol::ANIMATION_TYPE_HIT);
    SetAnimTick(serverTick);
}

void Player::OnDeathEnter(uint32 serverTick)
{
    m_lifeState = EPlayerLifeState::DeadAnimating;
    weapon.CancelAttack();

    SetAnimState(Protocol::ANIMATION_TYPE_DIE);
    SetAnimTick(serverTick);
    SetVelocity(GameMath::Vec3::Zero());
    ClearMoveKeyCodes();
    m_deathTick = serverTick;

    if (auto* collider = GetComponent<CColliderComponent>())
    {
        collider->OnUpdate(0.0f);
    }

    // active는 참여 상태와 분리: 여기서 SetActive(false) 하지 않음
}

void Player::OnRespawnEnter(uint32 serverTick)
{
    m_lifeState = EPlayerLifeState::Alive;

    ResetHpToMax();
    SetPosition(GameMath::Vec3(0.0f, 0.0f, -200.0f));
	SetYaw(180.0f);
    SetVelocity(GameMath::Vec3::Zero());
    ClearMoveKeyCodes();
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