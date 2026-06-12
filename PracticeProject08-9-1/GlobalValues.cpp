#include "stdafx.h"
#include "GlobalValues.h"

unsigned int g_myPlayerId = 0;
bool g_PlayerIdReceived = false;
bool g_GameStarted = false;
NetworkQueue g_NetworkQueue;

std::array<int, 4> g_waitSceneSelectedWeaponSlots = { -1, -1, -1, -1 };
std::array<bool, 4> g_waitSceneWeaponSelectionKnown = { false, false, false, false };

Atomic<bool> g_End = false;