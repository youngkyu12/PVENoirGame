#include "stdafx.h"
#include "HealthComponent.h"
#include "AudioManager.h"
#include "Object.h"

#include <random>

namespace
{
	constexpr float kHitSfxVolume = 2.0f;

	int RandomHitSfxIndex()
	{
		static std::mt19937 rng{ std::random_device{}( ) };
		static std::uniform_int_distribution<int> dist(1, 2);
		return dist(rng);
	}
}

const char* CHealthComponent::SelectHitSfxPath()
{
	switch ( RandomHitSfxIndex() )
	{
	case 1: return "Assets/Audio/Hit1.wav";
	case 2: return "Assets/Audio/Hit2.wav";
	default: break;
	}

	return "Assets/Audio/Hit1.wav";
}

void CHealthComponent::PlayHitSfx()
{
	if ( !m_audioManager )
		return;

	if ( !m_pOwner )
		return;

	const char* path = SelectHitSfxPath();

	if ( !path || !path[0] )
		return;

	const XMFLOAT3 pos = m_pOwner->GetPosition();

	m_audioManager->PlaySound3D(
		path,
		pos,
		false,        // loop
		false,        // stream
		kHitSfxVolume,
		false         // startPaused
	);

#if defined(_DEBUG)
	char buf[512];
	sprintf_s(
		buf,
		"[HitSfx] sound=\"%s\" delay=0.0000 sec / 0.00 ms volume=%.2f owner=%p pos=(%.3f, %.3f, %.3f)\n",
		path,
		kHitSfxVolume,
		static_cast< void* >( m_pOwner ),
		pos.x,
		pos.y,
		pos.z
	);
	OutputDebugStringA(buf);
#endif
}