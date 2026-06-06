#pragma once

#include "pch.h"

#include <vector>

struct DrawPoint
{
	float x = 0.0f;
	float z = 0.0f;
};

struct DrawStaticObject
{
	float x = 0.0f;
	float z = 0.0f;
	int type = 0;
};

struct DrawSnapshot
{
	std::vector<DrawPoint> players;
	std::vector<DrawPoint> enemies;
	std::vector<DrawStaticObject> staticObjects;
};

void GetWorldDrawSnapshot(DrawSnapshot& outSnapshot);
int GetConnectedStressClientCount();

void RegisterStressSession(PacketSessionRef session);
void UnregisterStressSession(PacketSessionRef session);
void TickStressTest();
void SendDebugKillMega5();
void SendDebugTeleportToMegaGrid(int megaGridNumber);

int RunDrawModule();
