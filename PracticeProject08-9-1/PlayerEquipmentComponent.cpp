//-----------------------------------------------------------------------------
// File: PlayerEquipmentComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "PlayerEquipmentComponent.h"
#include "AudioManager.h"

#include "Object.h"

#include <random>
#include <algorithm>

CPlayerEquipmentComponent::CPlayerEquipmentComponent(CGameObject* owner)
    : CComponentT(owner)
{
}

void CPlayerEquipmentComponent::OnCreate(ID3D12Device* /*dev*/, ID3D12GraphicsCommandList* /*cmd*/)
{
    RefreshEquippedState();
}

namespace
{
	constexpr float kSfxDelayStepSeconds = 1.0f / 60.0f;

	constexpr float kSword1WhooshDelaySeconds = 13.0f / 60.0f; // 0.2167 sec
	constexpr float kSword2WhooshDelaySeconds = 13.0f / 60.0f; // 0.2167 sec
	constexpr float kSword3WhooshDelaySeconds = 15.0f / 60.0f; // 0.2500 sec

	constexpr float kAxeWhooshDelaySeconds = 23.0f / 60.0f; // 0.3833 sec

	constexpr float kRollSfxDelayForwardSeconds = 0.1000f;
	constexpr float kRollSfxDelayBackwardSeconds = 0.0000f;
	constexpr float kRollSfxDelayLeftSeconds = 0.1000f;
	constexpr float kRollSfxDelayRightSeconds = 0.1000f;

	constexpr float kSwordWhooshVolume = 0.5f;
	constexpr float kAxeWhooshVolume = 1.0f;
	constexpr float kRollSfxVolume = 2.0f;

	constexpr float kBowLoadingSfxDelaySeconds = 5.0f / 60.0f;  // 0.0833 sec
	constexpr float kBowReleaseSfxDelayFromLoadSeconds = 26.0f / 60.0f; // 0.4333 sec

	constexpr float kBowLoadingSfxVolume = 2.0f;
	constexpr float kBowReleaseSfxVolume = 2.0f;

	int RandomSwordWhooshIndex()
	{
		static std::mt19937 rng{ std::random_device{}( ) };
		static std::uniform_int_distribution<int> dist(1, 3);
		return dist(rng);
	}

	float GetRollSfxDelaySeconds(uint32_t dirBits)
	{
		const uint32_t horizontalDirBits =
			dirBits & ( DIR_FORWARD | DIR_BACKWARD | DIR_LEFT | DIR_RIGHT );

		// 뒤 구르기만 즉시 재생.
		if ( horizontalDirBits & DIR_BACKWARD )
			return kRollSfxDelayBackwardSeconds;

		if ( horizontalDirBits & DIR_LEFT )
			return kRollSfxDelayLeftSeconds;

		if ( horizontalDirBits & DIR_RIGHT )
			return kRollSfxDelayRightSeconds;

		// 무입력 구르기는 전방 구르기 취급.
		return kRollSfxDelayForwardSeconds;
	}
}

bool CPlayerEquipmentComponent::IsWeaponType(EWeaponType type)
{
    const int idx = static_cast<int>(type);
    return (idx >= 0 && idx < static_cast<int>(EWeaponType::Count));
}

int CPlayerEquipmentComponent::ToIndex(EWeaponType type)
{
    return IsWeaponType(type) ? static_cast<int>(type) : -1;
}

void CPlayerEquipmentComponent::SetWeaponObject(EWeaponType type, CGameObject* weaponObject)
{
    const int idx = ToIndex(type);
    if (idx < 0) return;

    // 이전 오브젝트가 있었다면 일단 숨김
    if (m_weaponObjects[idx] && m_weaponObjects[idx] != weaponObject)
    {
        if (auto* renderer = m_weaponObjects[idx]->GetRenderer())
            renderer->SetEnabled(false);
    }

    m_weaponObjects[idx] = weaponObject;
    RefreshWeaponVisibility();
}

CGameObject* CPlayerEquipmentComponent::GetWeaponObject(EWeaponType type) const
{
    const int idx = ToIndex(type);
    if (idx < 0) return nullptr;
    return m_weaponObjects[idx];
}

void CPlayerEquipmentComponent::ClearWeaponObjects()
{
    for (int i = 0; i < static_cast<int>(EWeaponType::Count); ++i)
    {
        if (m_weaponObjects[i])
        {
            if (auto* renderer = m_weaponObjects[i]->GetRenderer())
                renderer->SetEnabled(false);
        }
        m_weaponObjects[i] = nullptr;
    }
}

void CPlayerEquipmentComponent::ClearOwnedWeapons()
{
    m_ownedWeapons[0] = EWeaponType::None;
    m_ownedWeapons[1] = EWeaponType::None;
    m_ownedCount = 0;
    m_equippedWeapon = EWeaponType::None;

    RefreshWeaponVisibility();
}

bool CPlayerEquipmentComponent::HasWeapon(EWeaponType type) const
{
    if (!IsWeaponType(type)) return false;

    for (int i = 0; i < m_ownedCount; ++i)
    {
        if (m_ownedWeapons[i] == type)
            return true;
    }
    return false;
}

EWeaponType CPlayerEquipmentComponent::GetOwnedWeapon(int index) const
{
    if (index < 0 || index >= m_ownedCount)
        return EWeaponType::None;

    return m_ownedWeapons[index];
}

bool CPlayerEquipmentComponent::AddOwnedWeapon(EWeaponType type)
{
    if (!IsWeaponType(type))
        return false;

    if (HasWeapon(type))
    {
        RefreshEquippedState();
        return true;
    }

    if (m_ownedCount >= 2)
        return false;

    m_ownedWeapons[m_ownedCount] = type;
    ++m_ownedCount;

    RefreshEquippedState();
    return true;
}

void CPlayerEquipmentComponent::SetLoadout(EWeaponType first, EWeaponType second)
{
    m_ownedWeapons[0] = EWeaponType::None;
    m_ownedWeapons[1] = EWeaponType::None;
    m_ownedCount = 0;
    m_equippedWeapon = EWeaponType::None;

    if (IsWeaponType(first))
        AddOwnedWeapon(first);

    if (IsWeaponType(second) && second != first)
        AddOwnedWeapon(second);

    RefreshEquippedState();
}

CGameObject* CPlayerEquipmentComponent::GetEquippedWeaponObject() const
{
    return GetWeaponObject(m_equippedWeapon);
}

bool CPlayerEquipmentComponent::EquipWeapon(EWeaponType type)
{
    if (!HasWeapon(type))
        return false;

    m_equippedWeapon = type;
    RefreshWeaponVisibility();
    return true;
}

bool CPlayerEquipmentComponent::EquipOwnedWeaponByIndex(int index)
{
    if (index < 0 || index >= m_ownedCount)
        return false;

    return EquipWeapon(m_ownedWeapons[index]);
}

bool CPlayerEquipmentComponent::SwapWeapon()
{
    if (m_ownedCount <= 0)
    {
        Unequip();
        return false;
    }

    if (m_ownedCount == 1)
    {
        // 하나만 있으면 무조건 그걸 듦
        if (m_equippedWeapon != m_ownedWeapons[0])
            EquipWeapon(m_ownedWeapons[0]);
        return false;
    }

    // m_ownedCount == 2
    if (m_equippedWeapon == m_ownedWeapons[0])
        return EquipWeapon(m_ownedWeapons[1]);

    if (m_equippedWeapon == m_ownedWeapons[1])
        return EquipWeapon(m_ownedWeapons[0]);

    // 비정상 상태면 첫 번째 무기로 복구
    return EquipWeapon(m_ownedWeapons[0]);
}

void CPlayerEquipmentComponent::Unequip()
{
    m_equippedWeapon = EWeaponType::None;
    RefreshWeaponVisibility();
}

void CPlayerEquipmentComponent::RefreshEquippedState()
{
    if (m_ownedCount <= 0)
    {
        m_equippedWeapon = EWeaponType::None;
        RefreshWeaponVisibility();
        return;
    }

    // 하나라도 있으면 반드시 하나는 들고 있어야 함
    if (!HasWeapon(m_equippedWeapon))
        m_equippedWeapon = m_ownedWeapons[0];

    RefreshWeaponVisibility();
}

void CPlayerEquipmentComponent::SetWeaponObjectVisible(EWeaponType type, bool visible)
{
    CGameObject* obj = GetWeaponObject(type);
    if (!obj) return;

    if (auto* renderer = obj->GetRenderer())
        renderer->SetEnabled(visible);
}

void CPlayerEquipmentComponent::RefreshWeaponVisibility()
{
    for (int i = 0; i < static_cast<int>(EWeaponType::Count); ++i)
    {
        const EWeaponType type = static_cast<EWeaponType>(i);
        const bool visible = HasWeapon(type) && (type == m_equippedWeapon);
        SetWeaponObjectVisible(type, visible);
    }
}

void CPlayerEquipmentComponent::OnUpdate(float dt)
{
	UpdateActivePlayerSfx();

	for ( size_t i = 0; i < m_pendingSfxList.size(); )
	{
		PendingPlayerSfx& sfx = m_pendingSfxList[i];

		if ( sfx.timer > 0.0f )
		{
			sfx.timer -= dt;

			if ( sfx.timer > 0.0f )
			{
				++i;
				continue;
			}
		}

		PlayPendingPlayerSfxAt(i);
	}

	UpdateActivePlayerSfx();
}

bool CPlayerEquipmentComponent::RequestSwordAttackWhoosh()
{
	if ( m_equippedWeapon != EWeaponType::Sword )
		return false;

	if ( !m_audioManager )
		return false;

	const int index = SelectSwordWhooshIndex();
	const char* path = GetSwordWhooshPath(index);
	const float delaySeconds = GetSwordWhooshDelaySeconds(index);

	SchedulePlayerSfx(
		EPendingPlayerSfxKind::SwordWhoosh,
		path,
		delaySeconds,
		kSwordWhooshVolume
	);

	return true;
}

bool CPlayerEquipmentComponent::RequestAxeAttackWhoosh()
{
	if ( m_equippedWeapon != EWeaponType::Axe )
		return false;

	if ( !m_audioManager )
		return false;

	SchedulePlayerSfx(
		EPendingPlayerSfxKind::AxeWhoosh,
		GetAxeWhooshPath(),
		kAxeWhooshDelaySeconds,
		kAxeWhooshVolume
	);

	return true;
}

bool CPlayerEquipmentComponent::RequestRollSfx(uint32_t dirBits)
{
	if ( !m_audioManager )
		return false;

	const float delaySeconds = GetRollSfxDelaySeconds(dirBits);

	SchedulePlayerSfx(
		EPendingPlayerSfxKind::Roll,
		GetRollSfxPath(),
		delaySeconds,
		kRollSfxVolume
	);

	return true;
}

bool CPlayerEquipmentComponent::RequestBowLoadingSfx()
{
	if ( m_equippedWeapon != EWeaponType::Bow )
		return false;

	if ( !m_audioManager )
		return false;

	SchedulePlayerSfx(
		EPendingPlayerSfxKind::BowLoading,
		GetBowLoadingSfxPath(),
		kBowLoadingSfxDelaySeconds,
		kBowLoadingSfxVolume
	);

	return true;
}

bool CPlayerEquipmentComponent::RequestBowReleaseSfx()
{
	return RequestBowReleaseSfxFromLoadPhase();
}

const char* CPlayerEquipmentComponent::GetBowLoadingSfxPath()
{
	return "Assets/Audio/Bow_Loading.mp3";
}

const char* CPlayerEquipmentComponent::GetBowReleaseSfxPath()
{
	return "Assets/Audio/Bow_Release.mp3";
}

void CPlayerEquipmentComponent::SchedulePlayerSfx(
	EPendingPlayerSfxKind kind,
	const char* soundPath,
	float delaySeconds,
	float volume)
{
	if ( kind == EPendingPlayerSfxKind::None )
		return;

	if ( !soundPath || !soundPath[0] )
		return;

	PendingPlayerSfx sfx{};
	sfx.kind = kind;
	sfx.path = soundPath;
	sfx.timer = delaySeconds;
	sfx.originalDelay = delaySeconds;
	sfx.volume = volume;

	m_pendingSfxList.push_back(sfx);

	if ( delaySeconds <= 0.0f )
		PlayPendingPlayerSfxAt(m_pendingSfxList.size() - 1);
}

void CPlayerEquipmentComponent::PlayPendingPlayerSfxAt(size_t index)
{
	if ( index >= m_pendingSfxList.size() )
		return;

	const PendingPlayerSfx played = m_pendingSfxList[index];

	m_pendingSfxList.erase(m_pendingSfxList.begin() + index);

	if ( !m_audioManager )
		return;

	if ( !played.path || !played.path[0] )
		return;

	const XMFLOAT3 pos =
		m_pOwner ? m_pOwner->GetPosition() : XMFLOAT3(0.0f, 0.0f, 0.0f);

	FMOD::Channel* channel = m_audioManager->PlaySound3D(
		played.path,
		pos,
		false,
		false,
		played.volume,
		false
	);

	if ( channel && ShouldFollowOwnerForSfx(played.kind) && m_pOwner )
	{
		ActivePlayerSfx active{};
		active.kind = played.kind;
		active.channel = channel;
		active.followTarget = m_pOwner;
		active.prevPosition = pos;
		active.hasPrevPosition = true;

		m_activeSfxList.push_back(active);
	}

	if ( played.kind == EPendingPlayerSfxKind::BowLoading ||
		played.kind == EPendingPlayerSfxKind::BowRelease )
	{
		const char* tag =
			( played.kind == EPendingPlayerSfxKind::BowLoading )
			? "BowLoadingSfx"
			: "BowReleaseSfx";

		char buf[512];
		sprintf_s(
			buf,
			"[%s] sound=\"%s\" delay=%.4f sec / %.2f ms volume=%.2f owner=%p pos=(%.3f, %.3f, %.3f)\n",
			tag,
			played.path,
			played.originalDelay,
			played.originalDelay * 1000.0f,
			played.volume,
			static_cast< void* >( m_pOwner ),
			pos.x,
			pos.y,
			pos.z
		);
		OutputDebugStringA(buf);
	}
}

int CPlayerEquipmentComponent::SelectSwordWhooshIndex()
{
	return RandomSwordWhooshIndex();
}

const char* CPlayerEquipmentComponent::GetSwordWhooshPath(int index)
{
	switch ( index )
	{
	case 1: return "Assets/Audio/Whoosh_Sword1.wav";
	case 2: return "Assets/Audio/Whoosh_Sword2.wav";
	case 3: return "Assets/Audio/Whoosh_Sword3.wav";
	default: break;
	}

	return "Assets/Audio/Whoosh_Sword1.wav";
}

const char* CPlayerEquipmentComponent::GetRollSfxPath()
{
	return "Assets/Audio/Player_Roll.mp3";
}

float CPlayerEquipmentComponent::GetSwordWhooshDelaySeconds(int index)
{
	switch ( index )
	{
	case 1: return kSword1WhooshDelaySeconds;
	case 2: return kSword2WhooshDelaySeconds;
	case 3: return kSword3WhooshDelaySeconds;
	default: break;
	}

	return kSword1WhooshDelaySeconds;
}

const char* CPlayerEquipmentComponent::GetAxeWhooshPath()
{
	return "Assets/Audio/Whoosh_Axe.wav";
}

bool CPlayerEquipmentComponent::RequestBowReleaseSfxFromLoadPhase()
{
	if ( m_equippedWeapon != EWeaponType::Bow )
		return false;

	if ( !m_audioManager )
		return false;

	SchedulePlayerSfx(
		EPendingPlayerSfxKind::BowRelease,
		GetBowReleaseSfxPath(),
		kBowReleaseSfxDelayFromLoadSeconds,
		kBowReleaseSfxVolume
	);

	return true;
}

bool CPlayerEquipmentComponent::ShouldFollowOwnerForSfx(EPendingPlayerSfxKind kind) const
{
	switch ( kind )
	{
	case EPendingPlayerSfxKind::SwordWhoosh:
	case EPendingPlayerSfxKind::AxeWhoosh:
	case EPendingPlayerSfxKind::Roll:
	case EPendingPlayerSfxKind::BowLoading:
	case EPendingPlayerSfxKind::BowRelease:
		return true;

	default:
		return false;
	}
}

void CPlayerEquipmentComponent::UpdateActivePlayerSfx()
{
	if ( !m_audioManager )
	{
		m_activeSfxList.clear();
		return;
	}

	for ( size_t i = 0; i < m_activeSfxList.size(); )
	{
		ActivePlayerSfx& active = m_activeSfxList[i];

		if ( !active.channel || !active.followTarget )
		{
			m_activeSfxList.erase(m_activeSfxList.begin() + i);
			continue;
		}

		if ( !m_audioManager->IsChannelPlaying(active.channel) )
		{
			m_activeSfxList.erase(m_activeSfxList.begin() + i);
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

		m_audioManager->SetChannel3DAttributes(
			active.channel,
			pos,
			vel
		);

		active.prevPosition = pos;
		active.hasPrevPosition = true;

		++i;
	}
}