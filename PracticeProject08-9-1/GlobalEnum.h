#pragma once
#include <stdint.h>

enum class EAnimState : uint8_t
{
    Idle = 0,
    Move = 1,
    Attack = 2,
    Hit = 3,
	Die = 4,
};
