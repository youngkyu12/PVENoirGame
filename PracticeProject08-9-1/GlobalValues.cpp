#include "stdafx.h"
#include "GlobalValues.h"

unsigned int g_myPlayerId = 0;
bool g_PlayerIdReceived = false;
bool g_GameStarted = false;
NetworkQueue g_NetworkQueue;


Atomic<bool> g_End = false;