//-----------------------------------------------------------------------------
// File: ActorTagComponent.h
//-----------------------------------------------------------------------------

#pragma once

#include <cstdint>
#include "BaseComponent.h"

enum class EActorKind : uint8_t
{
    Static = 0,
    NPC,
    Player,
};

enum class EPlayerControl : uint8_t
{
    None = 0,
    Local,
    Remote,
};

class CActorTagComponent final : public CComponentT<CActorTagComponent>
{
public:
    using OwnerT = CComponent::OwnerT;
    explicit CActorTagComponent(OwnerT* owner)
        : CComponentT<CActorTagComponent>(owner)
    {
    }

public:
    EActorKind      kind = EActorKind::Static;
    EPlayerControl  control = EPlayerControl::None;

    // Local = 0, Remote = 1..N (프로젝트 규칙)
    int32_t         playerSlot = -1;
};