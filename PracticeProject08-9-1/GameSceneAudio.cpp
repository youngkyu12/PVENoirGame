//-----------------------------------------------------------------------------
// File: GameSceneAudio.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"

namespace
{
	static constexpr float kFootstepSfxVolume = 0.04f;

	static bool IsWalkClipName(const std::string& clipName)
	{
		return clipName.rfind("Walk_", 0) == 0;
	}

	static bool IsRunClipName(const std::string& clipName)
	{
		return clipName.rfind("Run_", 0) == 0;
	}

	static int GetFootstepModeFromClipName(const std::string& clipName)
	{
		if ( IsWalkClipName(clipName) )
			return 1;

		if ( IsRunClipName(clipName) )
			return 2;

		return 0;
	}

	static bool CrossedNormalizedEvent(
		float prevNormalized,
		float curNormalized,
		float eventNormalized)
	{
		if ( prevNormalized < 0.0f ) prevNormalized = 0.0f;
		if ( prevNormalized > 1.0f ) prevNormalized = 1.0f;

		if ( curNormalized < 0.0f ) curNormalized = 0.0f;
		if ( curNormalized > 1.0f ) curNormalized = 1.0f;

		if ( curNormalized >= prevNormalized )
			return prevNormalized < eventNormalized && eventNormalized <= curNormalized;

		return eventNormalized > prevNormalized || eventNormalized <= curNormalized;
	}

	static const char* SelectRandomFootstepGrassSfxPath()
	{
		static std::mt19937 rng{ std::random_device{}( ) };
		static std::uniform_int_distribution<int> dist(1, 3);

		switch ( dist(rng) )
		{
		case 1: return "Assets/Audio/Walk_Grass1.wav";
		case 2: return "Assets/Audio/Walk_Grass2.wav";
		case 3: return "Assets/Audio/Walk_Grass3.wav";
		default: break;
		}

		return "Assets/Audio/Walk_Grass1.wav";
	}

	static const char* SelectRandomFootstepBlockSfxPath()
	{
		static std::mt19937 rng{ std::random_device{}( ) };
		static std::uniform_int_distribution<int> dist(1, 3);

		switch ( dist(rng) )
		{
		case 1: return "Assets/Audio/Walk_Block1.wav";
		case 2: return "Assets/Audio/Walk_Block2.wav";
		case 3: return "Assets/Audio/Walk_Block3.wav";
		default: break;
		}

		return "Assets/Audio/Walk_Block1.wav";
	}
}

void CGameScene::RequestPlayerAttackSfx(CGameObject* player)
{
	if ( !player )
		return;

	auto* equip = player->GetComponent<CPlayerEquipmentComponent>();
	if ( !equip )
		return;

	switch ( equip->GetEquippedWeapon() )
	{
	case EWeaponType::Sword:
		equip->RequestSwordAttackWhoosh();
		break;

	case EWeaponType::Axe:
		equip->RequestAxeAttackWhoosh();
		break;

	case EWeaponType::Gun:
		equip->RequestGunShotSfx();
		break;

	case EWeaponType::Bow:
		// 활은 UpdatePreparedBowArrows()에서 Bow_Load / Bow_Release phase 기준으로 처리한다.
		break;

	default:
		break;
	}
}

void CGameScene::ResetPlayerFootstepSfxState()
{
	m_playerFootstepTrackingValid = { false, false, false, false };
	m_playerFootstepMode = { 0, 0, 0, 0 };
	m_playerFootstepPrevNormalizedTime = { 0.0f, 0.0f, 0.0f, 0.0f };
}

void CGameScene::PlayPlayerFootstepSfx(CGameObject* player)
{
	if ( !m_pAudioManager )
		return;

	if ( !player )
		return;

	const bool useBlockFootstep = IsLocalPlayerInsideMegaGridCenter();

	const char* path =
		useBlockFootstep
		? SelectRandomFootstepBlockSfxPath()
		: SelectRandomFootstepGrassSfxPath();

	if ( !path || !path[0] )
		return;

	const XMFLOAT3 pos = player->GetPosition();

	m_pAudioManager->PlaySound3D(
		path,
		pos,
		false,                 // loop
		false,                 // stream
		kFootstepSfxVolume,
		false                  // startPaused
	);

#if defined(_DEBUG)
	char buf[512];
	sprintf_s(
		buf,
		"[FootstepSfx] sound=\"%s\" volume=%.2f owner=%p pos=(%.3f, %.3f, %.3f)\n",
		path,
		kFootstepSfxVolume,
		static_cast< void* >( player ),
		pos.x,
		pos.y,
		pos.z
	);
	OutputDebugStringA(buf);
#endif
}

void CGameScene::UpdatePlayerFootstepSfx()
{
	for ( int slot = 0; slot < 4; ++slot )
	{
		CGameObject* player = GetPlayerBySlot(slot);
		const size_t slotIndex = static_cast< size_t >(slot);

		if ( !player )
		{
			m_playerFootstepTrackingValid[slotIndex] = false;
			m_playerFootstepMode[slotIndex] = 0;
			m_playerFootstepPrevNormalizedTime[slotIndex] = 0.0f;
			continue;
		}

		CAnimator* anim = nullptr;

		if ( auto* animComp = player->GetComponent<CAnimatorComponent>() )
			anim = animComp->GetAnimator();

		if ( !anim )
			anim = player->GetAnimator();

		if ( !anim || !anim->IsPlaying() )
		{
			m_playerFootstepTrackingValid[slotIndex] = false;
			m_playerFootstepMode[slotIndex] = 0;
			m_playerFootstepPrevNormalizedTime[slotIndex] = 0.0f;
			continue;
		}

		const std::string& clipName = anim->GetCurrentClipName();
		const int mode = GetFootstepModeFromClipName(clipName);

		// Walk_/Run_이 아니면 발소리 추적 중단.
		if ( mode == 0 )
		{
			m_playerFootstepTrackingValid[slotIndex] = false;
			m_playerFootstepMode[slotIndex] = 0;
			m_playerFootstepPrevNormalizedTime[slotIndex] = 0.0f;
			continue;
		}

		const float duration = anim->GetCurrentClipDuration();
		if ( duration <= 1.0e-6f )
		{
			m_playerFootstepTrackingValid[slotIndex] = false;
			m_playerFootstepMode[slotIndex] = 0;
			m_playerFootstepPrevNormalizedTime[slotIndex] = 0.0f;
			continue;
		}

		float curNormalized = anim->GetCurrentTime() / duration;

		if ( curNormalized < 0.0f ) curNormalized = 0.0f;
		if ( curNormalized > 1.0f ) curNormalized = 1.0f;

		// 처음 Walk/Run에 진입한 프레임에는 소리 내지 않고 기준점만 잡는다.
		if ( !m_playerFootstepTrackingValid[slotIndex] ||
			m_playerFootstepMode[slotIndex] != mode )
		{
			m_playerFootstepTrackingValid[slotIndex] = true;
			m_playerFootstepMode[slotIndex] = mode;
			m_playerFootstepPrevNormalizedTime[slotIndex] = curNormalized;
			continue;
		}

		const float prevNormalized = m_playerFootstepPrevNormalizedTime[slotIndex];

		bool shouldPlayFootstep = false;

		if ( mode == 1 ) // Walk: 21 keyframes, foot contact at 2, 11
		{
			constexpr float kWalkFootstep0 = ( 2.0f - 1.0f ) / ( 21.0f - 1.0f );
			constexpr float kWalkFootstep1 = ( 11.0f - 1.0f ) / ( 21.0f - 1.0f );

			shouldPlayFootstep =
				CrossedNormalizedEvent(prevNormalized, curNormalized, kWalkFootstep0) ||
				CrossedNormalizedEvent(prevNormalized, curNormalized, kWalkFootstep1);
		}
		else if ( mode == 2 ) // Run: 16 keyframes, foot contact at 2, 10
		{
			constexpr float kRunFootstep0 = ( 2.0f - 1.0f ) / ( 16.0f - 1.0f );
			constexpr float kRunFootstep1 = ( 10.0f - 1.0f ) / ( 16.0f - 1.0f );

			shouldPlayFootstep =
				CrossedNormalizedEvent(prevNormalized, curNormalized, kRunFootstep0) ||
				CrossedNormalizedEvent(prevNormalized, curNormalized, kRunFootstep1);
		}

		if ( shouldPlayFootstep )
			PlayPlayerFootstepSfx(player);

		m_playerFootstepPrevNormalizedTime[slotIndex] = curNormalized;
	}
}



void CGameScene::UpdatePlayerBowSfxOnly()
{
	for ( int slot = 0; slot < 4; ++slot )
	{
		CGameObject* player = GetPlayerBySlot(slot);
		const size_t slotIndex = static_cast< size_t >(slot);

		bool isBowLoad = false;
		bool isBowRelease = false;
		bool hasBowEquipped = false;

		if ( player )
		{
			if ( auto* equip = player->GetComponent<CPlayerEquipmentComponent>() )
			{
				hasBowEquipped = ( equip->GetEquippedWeapon() == EWeaponType::Bow );
			}

			if ( auto* animComp = player->GetComponent<CAnimatorComponent>() )
			{
				if ( auto* ctrl = animComp->EnsureController() )
				{
					isBowLoad = ctrl->IsBowLoadPhase();
					isBowRelease = ctrl->IsBowReleasePhase();
				}
			}
			else if ( auto* ctrl = player->GetAnimController() )
			{
				isBowLoad = ctrl->IsBowLoadPhase();
				isBowRelease = ctrl->IsBowReleasePhase();
			}
		}

		// Bow_Load 진입 순간에만 로딩 사운드 + 릴리즈 사운드 예약.
		// 릴리즈 사운드는 기존 구조대로 Bow_Load 시작 기준 0.4333초 후 재생된다.
		if ( hasBowEquipped && isBowLoad && !m_prevBowLoadPhase[slotIndex] )
		{
			if ( auto* equip = player ? player->GetComponent<CPlayerEquipmentComponent>() : nullptr )
			{
				equip->RequestBowLoadingSfx();
				equip->RequestBowReleaseSfxFromLoadPhase();
			}
		}

		// 네트워크 모드에서는 화살 준비/발사는 서버 snapshot.bullets가 담당한다.
		// 따라서 여기서는 RequestPrepareArrow(), RequestReleasePreparedArrow()를 호출하지 않는다.

		if ( !hasBowEquipped || ( !isBowLoad && !isBowRelease ) )
		{
			m_prevBowLoadPhase[slotIndex] = false;
			m_prevBowReleasePhase[slotIndex] = false;
			continue;
		}

		m_prevBowLoadPhase[slotIndex] = isBowLoad;
		m_prevBowReleasePhase[slotIndex] = isBowRelease;
	}
}