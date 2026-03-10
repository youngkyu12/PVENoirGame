#pragma once
#include <stdint.h>

enum class EAnimState : uint8_t
{
    Idle = 0,
    Move,
    Attack,
};
