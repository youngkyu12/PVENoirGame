#pragma once

#include <array>
#include "NetworkQueue.h"

extern unsigned int g_myPlayerId;
extern bool g_PlayerIdReceived;
extern bool g_GameStarted;
extern NetworkQueue g_NetworkQueue;

extern std::array<int, 4> g_waitSceneSelectedWeaponSlots;
extern std::array<bool, 4> g_waitSceneWeaponSelectionKnown;

extern Atomic<bool> g_End;