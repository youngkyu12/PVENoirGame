#pragma once

#include "pch.h"

#include <vector>

struct DrawPoint
{
	float x = 0.0f;
	float z = 0.0f;
};

struct DrawSnapshot
{
	std::vector<DrawPoint> players;
	std::vector<DrawPoint> enemies;
};

void GetWorldDrawSnapshot(DrawSnapshot& outSnapshot);
int GetConnectedStressClientCount();

void RegisterStressSession(PacketSessionRef session);
void UnregisterStressSession(PacketSessionRef session);
void TickStressTest();

int RunDrawModule();
