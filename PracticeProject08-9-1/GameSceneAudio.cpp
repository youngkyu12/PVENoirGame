//-----------------------------------------------------------------------------
// File: GameSceneAudio.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "GameScenePrivate.h"

namespace
{
	static constexpr float kFootstepSfxVolume = 0.04f;
	static constexpr float kPlayerDeathSfxVolume = 1.0f;

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

	static constexpr float kMonsterFootstepSfxVolume = 0.04f;

	static constexpr float kMonsterSwordWhooshDelaySeconds = 13.0f / 60.0f;
	static constexpr float kMonsterBowLoadingSfxDelaySeconds = 5.0f / 60.0f;
	static constexpr float kMonsterBowReleaseSfxDelayFromLoadSeconds = 26.0f / 60.0f;

	static constexpr float kMonsterSwordWhooshVolume = 0.5f;
	static constexpr float kMonsterBowLoadingSfxVolume = 2.0f;
	static constexpr float kMonsterBowReleaseSfxVolume = 2.0f;
	static constexpr float kMonsterMutantWhooshDelaySeconds = 0.2324f;
	static constexpr float kMonsterMutantWhooshVolume = 0.5f;

	static constexpr float kMonsterGhoulWhooshDelaySeconds = 0.1494f;
	static constexpr float kMonsterGhoulWhooshVolume = 0.5f;

	static constexpr float kBossSummonSfxVolume = 5.0f;
	static constexpr float kBossSummonCircleSfxVolume = 1.0f;

	static constexpr float kBossCallSummonCircleSfxVolume = 1.0f;
	static constexpr float kBossCallMonsterSpawnSfxVolume = 1.0f;

	static constexpr float kEnemySpawnerSirenSfxVolume = 1.0f;

	static constexpr float kBossAttackSfxDelaySeconds = 0.3320f;
	static constexpr float kBossAttackSfxVolume = 1.0f;

	static constexpr float kBossSpellSfxVolume = 1.0f;
	static constexpr float kBossDeathSfxVolume = 1.0f;

	static constexpr float kBossShockwaveWindSfxVolume = 1.0f;

	static const char* GetBossSummonSfxPath()
	{
		return "Assets/Audio/BossSummon.wav";
	}

	static const char* GetBossCallMonsterSpawnSfxPath()
	{
		return "Assets/Audio/Summon.wav";
	}

	static const char* GetEnemySpawnerSirenSfxPath()
	{
		return "Assets/Audio/siren.wav";
	}

	static const char* GetBossAttackSfxPath()
	{
		return "Assets/Audio/BossAttack.wav";
	}

	static const char* GetBossSpellSfxPath()
	{
		return "Assets/Audio/BossSpell.wav";
	}

	static const char* GetBossDeathSfxPath()
	{
		return "Assets/Audio/BossDeath.wav";
	}

	static const char* GetBossShockwaveWindSfxPath()
	{
		return "Assets/Audio/BossSummonWind.wav";
	}

	static const char* GetBossSummonCircleSfxPath1()
	{
		return "Assets/Audio/mhj.wav";
	}

	static const char* GetBossCallSummonCircleSfxPath()
	{
		return "Assets/Audio/mhj2.wav";
	}

	static const char* GetMonsterMutantWhooshPath()
	{
		return "Assets/Audio/Whoosh_Sword2.wav";
	}

	static const char* GetMonsterGhoulWhooshPath()
	{
		return "Assets/Audio/Whoosh_Sword3.wav";
	}

	static bool IsMonsterWalkClipName(const std::string& clipName)
	{
		return clipName == "Walk" || clipName.rfind("Walk_", 0) == 0;
	}

	static bool IsMonsterRunClipName(const std::string& clipName)
	{
		return clipName == "Run" || clipName.rfind("Run_", 0) == 0;
	}

	static int GetMonsterFootstepModeFromClipName(const std::string& clipName)
	{
		if ( IsMonsterWalkClipName(clipName) )
			return 1;

		if ( IsMonsterRunClipName(clipName) )
			return 2;

		return 0;
	}

	static const char* SelectRandomMonsterFootstepBlockSfxPath()
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

	static const char* GetMonsterSwordManWhooshPath()
	{
		return "Assets/Audio/Whoosh_Sword2.wav";
	}

	static const char* GetMonsterBowLoadingSfxPath()
	{
		return "Assets/Audio/Bow_Loading.mp3";
	}

	static const char* GetMonsterBowReleaseSfxPath()
	{
		return "Assets/Audio/Bow_Release.mp3";
	}

	static const char* GetPlayerDeathSfxPath()
	{
		return "Assets/Audio/death.wav";
	}
}

void CGameScene::PlayBossSummonSfxAt(const XMFLOAT3& position)
{
	if ( !m_pAudioManager )
		return;

	const char* path = GetBossSummonSfxPath();

	if ( !path || !path[0] )
		return;

	m_pAudioManager->PlaySound3D(
		path,
		position,
		false,                 // loop: 1회 재생
		false,                 // stream
		kBossSummonSfxVolume,
		false                  // startPaused
	);
}

void CGameScene::PlayBossSummonCircleSfxAt(const XMFLOAT3& position)
{
	if ( !m_pAudioManager )
		return;

	const char* path = GetBossSummonCircleSfxPath1();

	if ( path && path[0] )
	{
		m_pAudioManager->PlaySound3D(
			path,
			position,
			false,                         // loop: 1회 재생
			false,                         // stream
			kBossSummonCircleSfxVolume,
			false                          // startPaused
		);
	}
}

void CGameScene::PlayBossCallSummonCircleSfxAt(const XMFLOAT3& position)
{
	if ( !m_pAudioManager )
		return;

	const char* path = GetBossCallSummonCircleSfxPath();

	if ( !path || !path[0] )
		return;

	m_pAudioManager->PlaySound3D(
		path,
		position,
		false,                         // loop: 1회 재생
		false,                         // stream
		kBossCallSummonCircleSfxVolume,
		false                          // startPaused
	);
}

void CGameScene::PlayBossCallMonsterSpawnSfxAt(const XMFLOAT3& position)
{
	if ( !m_pAudioManager )
		return;

	const char* path = GetBossCallMonsterSpawnSfxPath();

	if ( !path || !path[0] )
		return;

	m_pAudioManager->PlaySound3D(
		path,
		position,
		false,                         // loop: 1회 재생
		false,                         // stream
		kBossCallMonsterSpawnSfxVolume,
		false                          // startPaused
	);
}

void CGameScene::PlayEnemySpawnerSirenSfxAt(const XMFLOAT3& position)
{
	if ( !m_pAudioManager )
		return;

	const char* path = GetEnemySpawnerSirenSfxPath();

	if ( !path || !path[0] )
		return;

	m_pAudioManager->PlaySound3D(
		path,
		position,
		false,                         // loop: 1회 재생
		false,                         // stream
		kEnemySpawnerSirenSfxVolume,
		false                          // startPaused
	);
}

void CGameScene::PlayBossSpellSfxAt(const XMFLOAT3& position)
{
	if ( !m_pAudioManager )
		return;

	const char* path = GetBossSpellSfxPath();

	if ( !path || !path[0] )
		return;

	m_pAudioManager->PlaySound3D(
		path,
		position,
		false,                 // loop: 1회 재생
		false,                 // stream
		kBossSpellSfxVolume,
		false                  // startPaused
	);
}

void CGameScene::PlayBossDeathSfxAt(const XMFLOAT3& position)
{
	if ( !m_pAudioManager )
		return;

	const char* path = GetBossDeathSfxPath();

	if ( !path || !path[0] )
		return;

	m_pAudioManager->PlaySound3D(
		path,
		position,
		false,                 // loop: 1회 재생
		false,                 // stream
		kBossDeathSfxVolume,
		false                  // startPaused
	);
}

void CGameScene::PlayBossShockwaveWindSfxAt(const XMFLOAT3& position)
{
	ResetBossShockwaveWindSfxTracking();

	if ( !m_pAudioManager )
		return;

	const char* path = GetBossShockwaveWindSfxPath();

	if ( !path || !path[0] )
		return;

	FMOD::Channel* channel = m_pAudioManager->PlaySound3D(
		path,
		position,
		false,                         // loop: 1회 재생
		false,                         // stream
		kBossShockwaveWindSfxVolume,
		false                          // startPaused
	);

	if ( !channel )
		return;

	m_bossShockwaveWindSfxChannel = channel;
	m_bossShockwaveWindSfxPrevPosition = position;
	m_bossShockwaveWindSfxHasPrevPosition = true;
	m_bBossShockwaveWindSfxTrackingActive = true;

}

void CGameScene::UpdateBossShockwaveWindSfx(float currentRadius)
{
	if ( !m_bBossShockwaveWindSfxTrackingActive )
		return;

	if ( !m_pAudioManager || !m_bossShockwaveWindSfxChannel )
	{
		ResetBossShockwaveWindSfxTracking();
		return;
	}

	if ( !m_pAudioManager->IsChannelPlaying(m_bossShockwaveWindSfxChannel) )
	{
		ResetBossShockwaveWindSfxTracking();
		return;
	}

	float radius = std::clamp(currentRadius, 0.0f, kBossShockwaveMaxRadius);

	XMFLOAT3 dir = m_bossShockwaveWindSfxDirection;

	// 로컬 플레이어 기준으로 충격파 원 위의 가장 가까운 지점을 사운드 발생점으로 쓴다.
	// 즉, 사운드가 중앙에 고정되지 않고 플레이어 쪽으로 퍼지는 충격파 전면을 따라간다.
	CGameObject* localPlayer = GetPlayer();

	if ( localPlayer && !m_bLocalPlayerDead )
	{
		const XMFLOAT3 playerPos = localPlayer->GetPosition();

		const float dx = playerPos.x - m_bossShockwaveCenter.x;
		const float dz = playerPos.z - m_bossShockwaveCenter.z;
		const float distSq = dx * dx + dz * dz;

		const float minDirDistSq =
			kBossShockwavePlayerMinDirectionDistance *
			kBossShockwavePlayerMinDirectionDistance;

		if ( distSq > minDirDistSq )
		{
			const float invDist = 1.0f / sqrtf(distSq);

			dir.x = dx * invDist;
			dir.y = 0.0f;
			dir.z = dz * invDist;

			m_bossShockwaveWindSfxDirection = dir;
		}
	}

	XMFLOAT3 sourcePos = m_bossShockwaveCenter;
	sourcePos.x += dir.x * radius;
	sourcePos.y = 0.0f;
	sourcePos.z += dir.z * radius;

	XMFLOAT3 velocity(0.0f, 0.0f, 0.0f);

	if ( m_bossShockwaveWindSfxHasPrevPosition )
	{
		velocity.x = sourcePos.x - m_bossShockwaveWindSfxPrevPosition.x;
		velocity.y = sourcePos.y - m_bossShockwaveWindSfxPrevPosition.y;
		velocity.z = sourcePos.z - m_bossShockwaveWindSfxPrevPosition.z;
	}

	m_pAudioManager->SetChannel3DAttributes(
		m_bossShockwaveWindSfxChannel,
		sourcePos,
		velocity
	);

	m_bossShockwaveWindSfxPrevPosition = sourcePos;
	m_bossShockwaveWindSfxHasPrevPosition = true;
}

void CGameScene::ResetBossShockwaveWindSfxTracking()
{
	m_bBossShockwaveWindSfxTrackingActive = false;
	m_bossShockwaveWindSfxChannel = nullptr;
	m_bossShockwaveWindSfxDirection = XMFLOAT3(0.0f, 0.0f, 1.0f);
	m_bossShockwaveWindSfxPrevPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_bossShockwaveWindSfxHasPrevPosition = false;
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

void CGameScene::ResetMonsterSfxState()
{
	m_monsterFootstepSfxStates.clear();

	m_prevGhoulAttackPhase.clear();
	m_prevSwordManAttackPhase.clear();
	m_prevMutantAttackPhase.clear();
	m_prevBowManSfxLoadPhase.clear();

	m_pendingMonsterSfxList.clear();
	m_activeMonsterSfxList.clear();
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
}

void CGameScene::PlayPlayerDeathSfxAt(const XMFLOAT3& position)
{
	if ( !m_pAudioManager )
		return;

	const char* path = GetPlayerDeathSfxPath();

	if ( !path || !path[0] )
		return;

	m_pAudioManager->PlaySound3D(
		path,
		position,
		false,                 // loop: 1회 재생
		false,                 // stream
		kPlayerDeathSfxVolume,
		false                  // startPaused
	);
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

void CGameScene::UpdateMonsterSfx(float dt)
{
	UpdateActiveMonsterSfx();

	UpdateMonsterFootstepSfx();
	UpdateMonsterAttackSfx();

	UpdatePendingMonsterSfx(dt);

	UpdateActiveMonsterSfx();
}

void CGameScene::UpdateMonsterFootstepSfx()
{
	const size_t requiredStateCount =
		m_ghoulRefs.size() +
		m_swordManRefs.size() +
		m_bowManRefs.size() +
		m_MutantRefs.size();

	if ( m_monsterFootstepSfxStates.size() < requiredStateCount )
	{
		m_monsterFootstepSfxStates.resize(requiredStateCount);
	}

	size_t stateIndex = 0;

	auto TickGroup =
		[ this, &stateIndex ](
			const std::vector<CGameObject*>& monsters,
			EMonsterFootstepProfile profile)
		{
			for ( CGameObject* monster : monsters )
			{
				if ( stateIndex >= m_monsterFootstepSfxStates.size() )
					return;

				TrackMonsterFootstepSfx(
					monster,
					m_monsterFootstepSfxStates[stateIndex],
					profile
				);

				++stateIndex;
			}
		};

	TickGroup(m_ghoulRefs, EMonsterFootstepProfile::Ghoul);
	TickGroup(m_swordManRefs, EMonsterFootstepProfile::Humanoid);
	TickGroup(m_bowManRefs, EMonsterFootstepProfile::Humanoid);
	TickGroup(m_MutantRefs, EMonsterFootstepProfile::Mutant);
}

void CGameScene::TrackMonsterFootstepSfx(
	CGameObject* monster,
	MonsterFootstepSfxState& state,
	EMonsterFootstepProfile profile)
{
	if ( !monster || !monster->GetActive() || IsMonsterDead(monster) )
	{
		state = MonsterFootstepSfxState{};
		return;
	}

	CAnimator* anim = nullptr;

	if ( auto* animComp = monster->GetComponent<CAnimatorComponent>() )
		anim = animComp->GetAnimator();

	if ( !anim )
		anim = monster->GetAnimator();

	if ( !anim || !anim->IsPlaying() )
	{
		state = MonsterFootstepSfxState{};
		return;
	}

	const std::string& clipName = anim->GetCurrentClipName();
	const int mode = GetMonsterFootstepModeFromClipName(clipName);

	if ( mode == 0 )
	{
		state = MonsterFootstepSfxState{};
		return;
	}

	const float duration = anim->GetCurrentClipDuration();
	if ( duration <= 1.0e-6f )
	{
		state = MonsterFootstepSfxState{};
		return;
	}

	float curNormalized = anim->GetCurrentTime() / duration;

	if ( curNormalized < 0.0f ) curNormalized = 0.0f;
	if ( curNormalized > 1.0f ) curNormalized = 1.0f;

	// Walk/Run 진입 첫 프레임에는 기준점만 잡고 발소리는 내지 않는다.
	if ( !state.valid || state.mode != mode )
	{
		state.valid = true;
		state.mode = mode;
		state.prevNormalizedTime = curNormalized;
		return;
	}

	const float prevNormalized = state.prevNormalizedTime;

	bool shouldPlayFootstep = false;

	if ( profile == EMonsterFootstepProfile::Ghoul )
	{
		if ( mode == 1 )
		{
			// Ghoul Walk:
			// 총 90 keyframes.
			// 발 접지 frame = 11, 54.
			constexpr float kGhoulWalkFootstep0 = ( 11.0f - 1.0f ) / ( 90.0f - 1.0f );
			constexpr float kGhoulWalkFootstep1 = ( 54.0f - 1.0f ) / ( 90.0f - 1.0f );

			shouldPlayFootstep =
				CrossedNormalizedEvent(prevNormalized, curNormalized, kGhoulWalkFootstep0) ||
				CrossedNormalizedEvent(prevNormalized, curNormalized, kGhoulWalkFootstep1);
		}
		else if ( mode == 2 )
		{
			// Ghoul Run:
			// 총 94 keyframes.
			// 발 접지 frame = 11, 35, 58, 81.
			constexpr float kGhoulRunFootstep0 = ( 11.0f - 1.0f ) / ( 94.0f - 1.0f );
			constexpr float kGhoulRunFootstep1 = ( 35.0f - 1.0f ) / ( 94.0f - 1.0f );
			constexpr float kGhoulRunFootstep2 = ( 58.0f - 1.0f ) / ( 94.0f - 1.0f );
			constexpr float kGhoulRunFootstep3 = ( 81.0f - 1.0f ) / ( 94.0f - 1.0f );

			shouldPlayFootstep =
				CrossedNormalizedEvent(prevNormalized, curNormalized, kGhoulRunFootstep0) ||
				CrossedNormalizedEvent(prevNormalized, curNormalized, kGhoulRunFootstep1) ||
				CrossedNormalizedEvent(prevNormalized, curNormalized, kGhoulRunFootstep2) ||
				CrossedNormalizedEvent(prevNormalized, curNormalized, kGhoulRunFootstep3);
		}
	}
	else if ( profile == EMonsterFootstepProfile::Mutant )
	{
		if ( mode == 1 )
		{
			// Mutant Walk:
			// 총 35 keyframes.
			// 발 접지 frame = 6, 21.
			// normalized = (frame - 1) / (keyframeCount - 1)
			constexpr float kMutantWalkFootstep0 = ( 6.0f - 1.0f ) / ( 35.0f - 1.0f );
			constexpr float kMutantWalkFootstep1 = ( 21.0f - 1.0f ) / ( 35.0f - 1.0f );

			shouldPlayFootstep =
				CrossedNormalizedEvent(prevNormalized, curNormalized, kMutantWalkFootstep0) ||
				CrossedNormalizedEvent(prevNormalized, curNormalized, kMutantWalkFootstep1);
		}
		else if ( mode == 2 )
		{
			// Mutant Run:
			// 총 21 keyframes.
			// 발 접지 frame = 4, 14.
			constexpr float kMutantRunFootstep0 = ( 4.0f - 1.0f ) / ( 21.0f - 1.0f );
			constexpr float kMutantRunFootstep1 = ( 14.0f - 1.0f ) / ( 21.0f - 1.0f );

			shouldPlayFootstep =
				CrossedNormalizedEvent(prevNormalized, curNormalized, kMutantRunFootstep0) ||
				CrossedNormalizedEvent(prevNormalized, curNormalized, kMutantRunFootstep1);
		}
	}
	else
	{
		if ( mode == 1 )
		{
			// Humanoid Walk:
			// 총 21 keyframes.
			// 발 접지 frame = 2, 11.
			constexpr float kWalkFootstep0 = ( 2.0f - 1.0f ) / ( 21.0f - 1.0f );
			constexpr float kWalkFootstep1 = ( 11.0f - 1.0f ) / ( 21.0f - 1.0f );

			shouldPlayFootstep =
				CrossedNormalizedEvent(prevNormalized, curNormalized, kWalkFootstep0) ||
				CrossedNormalizedEvent(prevNormalized, curNormalized, kWalkFootstep1);
		}
		else if ( mode == 2 )
		{
			// Humanoid Run:
			// 총 16 keyframes.
			// 발 접지 frame = 2, 10.
			constexpr float kRunFootstep0 = ( 2.0f - 1.0f ) / ( 16.0f - 1.0f );
			constexpr float kRunFootstep1 = ( 10.0f - 1.0f ) / ( 16.0f - 1.0f );

			shouldPlayFootstep =
				CrossedNormalizedEvent(prevNormalized, curNormalized, kRunFootstep0) ||
				CrossedNormalizedEvent(prevNormalized, curNormalized, kRunFootstep1);
		}
	}

	if ( shouldPlayFootstep )
		PlayMonsterFootstepSfx(monster);

	state.prevNormalizedTime = curNormalized;
}

void CGameScene::PlayMonsterFootstepSfx(CGameObject* monster)
{
	if ( !monster )
		return;

	ScheduleMonsterSfx(
		EMonsterSfxKind::Footstep,
		monster,
		SelectRandomMonsterFootstepBlockSfxPath(),
		0.0f,
		kMonsterFootstepSfxVolume,
		false
	);
}

void CGameScene::UpdateMonsterAttackSfx()
{
	if ( m_prevGhoulAttackPhase.size() < m_ghoulRefs.size() )
		m_prevGhoulAttackPhase.resize(m_ghoulRefs.size(), false);

	for ( size_t i = 0; i < m_ghoulRefs.size(); ++i )
	{
		CGameObject* ghoul = m_ghoulRefs[i];

		if ( !ghoul || IsMonsterDead(ghoul) )
		{
			m_prevGhoulAttackPhase[i] = false;
			continue;
		}

		bool isAttack = false;

		if ( auto* animComp = ghoul->GetComponent<CAnimatorComponent>() )
		{
			if ( auto* ctrl = animComp->EnsureMonsterController() )
			{
				isAttack = ctrl->IsAttackPrimaryPhase();
			}
		}

		if ( isAttack && !m_prevGhoulAttackPhase[i] )
		{
			RequestGhoulAttackSfx(ghoul);
		}

		m_prevGhoulAttackPhase[i] = isAttack;
	}

	if ( m_prevSwordManAttackPhase.size() < m_swordManRefs.size() )
		m_prevSwordManAttackPhase.resize(m_swordManRefs.size(), false);

	for ( size_t i = 0; i < m_swordManRefs.size(); ++i )
	{
		CGameObject* swordman = m_swordManRefs[i];

		if ( !swordman || IsMonsterDead(swordman) )
		{
			m_prevSwordManAttackPhase[i] = false;
			continue;
		}

		bool isAttack = false;

		if ( auto* animComp = swordman->GetComponent<CAnimatorComponent>() )
		{
			if ( auto* ctrl = animComp->EnsureMonsterController() )
			{
				isAttack = ctrl->IsAttackPrimaryPhase();
			}
		}

		if ( isAttack && !m_prevSwordManAttackPhase[i] )
		{
			RequestSwordManAttackSfx(swordman);
			BeginSwordManSwordTrail(swordman);
		}

		m_prevSwordManAttackPhase[i] = isAttack;
	}

	if ( m_prevMutantAttackPhase.size() < m_MutantRefs.size() )
		m_prevMutantAttackPhase.resize(m_MutantRefs.size(), false);

	for ( size_t i = 0; i < m_MutantRefs.size(); ++i )
	{
		CGameObject* mutant = m_MutantRefs[i];

		if ( !mutant || IsMonsterDead(mutant) )
		{
			m_prevMutantAttackPhase[i] = false;
			continue;
		}

		bool isAttack = false;

		if ( auto* animComp = mutant->GetComponent<CAnimatorComponent>() )
		{
			if ( auto* ctrl = animComp->EnsureMonsterController() )
			{
				isAttack = ctrl->IsAttackPrimaryPhase();
			}
		}

		if ( isAttack && !m_prevMutantAttackPhase[i] )
		{
			RequestMutantAttackSfx(mutant);
		}

		m_prevMutantAttackPhase[i] = isAttack;
	}

	if ( m_prevBowManSfxLoadPhase.size() < m_bowManRefs.size() )
		m_prevBowManSfxLoadPhase.resize(m_bowManRefs.size(), false);

	for ( size_t i = 0; i < m_bowManRefs.size(); ++i )
	{
		CGameObject* bowman = m_bowManRefs[i];

		if ( !bowman || IsMonsterDead(bowman) )
		{
			m_prevBowManSfxLoadPhase[i] = false;
			continue;
		}

		bool isBowLoad = false;

		if ( auto* animComp = bowman->GetComponent<CAnimatorComponent>() )
		{
			if ( auto* ctrl = animComp->EnsureMonsterController() )
			{
				isBowLoad = ctrl->IsAttackPrimaryPhase(); // Bow_Load
			}
		}

		// 플레이어 활과 동일하게 Bow_Load 진입 순간에 Loading 사운드를 예약하고,
		// Release 사운드는 Bow_Load 시작 기준 26/60초 뒤에 예약한다.
		if ( isBowLoad && !m_prevBowManSfxLoadPhase[i] )
		{
			RequestBowManLoadSfx(bowman);
		}

		m_prevBowManSfxLoadPhase[i] = isBowLoad;
	}
}

void CGameScene::RequestGhoulAttackSfx(CGameObject* ghoul)
{
	ScheduleMonsterSfx(
		EMonsterSfxKind::GhoulWhoosh,
		ghoul,
		GetMonsterGhoulWhooshPath(),
		kMonsterGhoulWhooshDelaySeconds,
		kMonsterGhoulWhooshVolume,
		true
	);
}

void CGameScene::RequestSwordManAttackSfx(CGameObject* swordman)
{
	ScheduleMonsterSfx(
		EMonsterSfxKind::SwordWhoosh,
		swordman,
		GetMonsterSwordManWhooshPath(),
		kMonsterSwordWhooshDelaySeconds,
		kMonsterSwordWhooshVolume,
		true
	);
}

void CGameScene::RequestMutantAttackSfx(CGameObject* mutant)
{
	ScheduleMonsterSfx(
		EMonsterSfxKind::MutantWhoosh,
		mutant,
		GetMonsterMutantWhooshPath(),
		kMonsterMutantWhooshDelaySeconds,
		kMonsterMutantWhooshVolume,
		true
	);
}

void CGameScene::RequestBowManLoadSfx(CGameObject* bowman)
{
	ScheduleMonsterSfx(
		EMonsterSfxKind::BowLoading,
		bowman,
		GetMonsterBowLoadingSfxPath(),
		kMonsterBowLoadingSfxDelaySeconds,
		kMonsterBowLoadingSfxVolume,
		true
	);

	ScheduleMonsterSfx(
		EMonsterSfxKind::BowRelease,
		bowman,
		GetMonsterBowReleaseSfxPath(),
		kMonsterBowReleaseSfxDelayFromLoadSeconds,
		kMonsterBowReleaseSfxVolume,
		true
	);
}

void CGameScene::RequestBossAttackSfx(CGameObject* boss)
{
	ScheduleMonsterSfx(
		EMonsterSfxKind::BossAttack,
		boss,
		GetBossAttackSfxPath(),
		kBossAttackSfxDelaySeconds,
		kBossAttackSfxVolume,
		true
	);

	char buf[256];
	sprintf_s(
		buf,
		"[BossAttackSfx][Request] delay=%.4f sec (%.1f ms)\n",
		kBossAttackSfxDelaySeconds,
		kBossAttackSfxDelaySeconds * 1000.0f
	);
}

void CGameScene::ScheduleMonsterSfx(
	EMonsterSfxKind kind,
	CGameObject* owner,
	const char* soundPath,
	float delaySeconds,
	float volume,
	bool followOwner)
{
	if ( kind == EMonsterSfxKind::None )
		return;

	if ( !owner )
		return;

	if ( IsMonsterDead(owner) )
		return;

	if ( !soundPath || !soundPath[0] )
		return;

	PendingMonsterSfx sfx{};
	sfx.kind = kind;
	sfx.owner = owner;
	sfx.path = soundPath;
	sfx.timer = delaySeconds;
	sfx.originalDelay = delaySeconds;
	sfx.volume = volume;
	sfx.followOwner = followOwner;

	m_pendingMonsterSfxList.push_back(sfx);

	if ( delaySeconds <= 0.0f )
		PlayPendingMonsterSfxAt(m_pendingMonsterSfxList.size() - 1);
}

void CGameScene::UpdatePendingMonsterSfx(float dt)
{
	for ( size_t i = 0; i < m_pendingMonsterSfxList.size(); )
	{
		PendingMonsterSfx& sfx = m_pendingMonsterSfxList[i];

		if ( !sfx.owner || IsMonsterDead(sfx.owner) )
		{
			m_pendingMonsterSfxList.erase(m_pendingMonsterSfxList.begin() + i);
			continue;
		}

		if ( sfx.timer > 0.0f )
		{
			sfx.timer -= dt;

			if ( sfx.timer > 0.0f )
			{
				++i;
				continue;
			}
		}

		PlayPendingMonsterSfxAt(i);
	}
}

void CGameScene::PlayPendingMonsterSfxAt(size_t index)
{
	if ( index >= m_pendingMonsterSfxList.size() )
		return;

	const PendingMonsterSfx played = m_pendingMonsterSfxList[index];

	m_pendingMonsterSfxList.erase(m_pendingMonsterSfxList.begin() + index);

	if ( !m_pAudioManager )
		return;

	if ( !played.owner || IsMonsterDead(played.owner) )
		return;

	if ( !played.path || !played.path[0] )
		return;

	const XMFLOAT3 pos = played.owner->GetPosition();

	FMOD::Channel* channel = m_pAudioManager->PlaySound3D(
		played.path,
		pos,
		false,
		false,
		played.volume,
		false
	);

	if ( channel && played.followOwner )
	{
		ActiveMonsterSfx active{};
		active.kind = played.kind;
		active.channel = channel;
		active.followTarget = played.owner;
		active.prevPosition = pos;
		active.hasPrevPosition = true;

		m_activeMonsterSfxList.push_back(active);
	}
}

void CGameScene::UpdateActiveMonsterSfx()
{
	if ( !m_pAudioManager )
	{
		m_activeMonsterSfxList.clear();
		return;
	}

	for ( size_t i = 0; i < m_activeMonsterSfxList.size(); )
	{
		ActiveMonsterSfx& active = m_activeMonsterSfxList[i];

		if ( !active.channel || !active.followTarget || IsMonsterDead(active.followTarget) )
		{
			m_activeMonsterSfxList.erase(m_activeMonsterSfxList.begin() + i);
			continue;
		}

		if ( !m_pAudioManager->IsChannelPlaying(active.channel) )
		{
			m_activeMonsterSfxList.erase(m_activeMonsterSfxList.begin() + i);
			continue;
		}

		const XMFLOAT3 pos = active.followTarget->GetPosition();

		XMFLOAT3 vel(0.0f, 0.0f, 0.0f);

		if ( active.hasPrevPosition )
		{
			vel.x = pos.x - active.prevPosition.x;
			vel.y = pos.y - active.prevPosition.y;
			vel.z = pos.z - active.prevPosition.z;
		}

		m_pAudioManager->SetChannel3DAttributes(
			active.channel,
			pos,
			vel
		);

		active.prevPosition = pos;
		active.hasPrevPosition = true;

		++i;
	}
}