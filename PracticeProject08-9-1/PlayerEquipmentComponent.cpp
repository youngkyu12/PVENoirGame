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
	constexpr float kSwordWhooshDelayStepSeconds = 1.0f / 60.0f;

	// 실제 게임에서도 사용하는 기본 딜레이.
	// 테스트가 끝나면 여기 값을 확정하거나, 애니메이션 이벤트 방식으로 교체하면 된다.
	float g_swordWhooshDelaySeconds = 0.0f;

#if ENABLE_PLAYER_SWORD_WHOOSH_TUNING
	// 0 = random, 1~3 = fixed
	int g_debugSwordWhooshFixedIndex = 0;
#endif

	int RandomSwordWhooshIndex()
	{
		static std::mt19937 rng{ std::random_device{}( ) };
		static std::uniform_int_distribution<int> dist(1, 3);
		return dist(rng);
	}

	const char* SwordWhooshModeText()
	{
#if ENABLE_PLAYER_SWORD_WHOOSH_TUNING
		switch ( g_debugSwordWhooshFixedIndex )
		{
		case 1: return "Fixed: Whoosh_Sword1";
		case 2: return "Fixed: Whoosh_Sword2";
		case 3: return "Fixed: Whoosh_Sword3";
		default: break;
		}
#endif
		return "Random";
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
	if ( !m_pendingSwordWhoosh )
		return;

	if ( m_pendingSwordWhooshTimer > 0.0f )
	{
		m_pendingSwordWhooshTimer -= dt;

		if ( m_pendingSwordWhooshTimer > 0.0f )
			return;
	}

	PlayPendingSwordWhoosh();
}

bool CPlayerEquipmentComponent::RequestSwordAttackWhoosh()
{
	if ( m_equippedWeapon != EWeaponType::Sword )
		return false;

	if ( !m_audioManager )
		return false;

	m_pendingSwordWhoosh = true;
	m_pendingSwordWhooshTimer = g_swordWhooshDelaySeconds;
	m_pendingSwordWhooshIndex = SelectSwordWhooshIndex();

	// 딜레이 0초면 같은 프레임에 즉시 재생한다.
	// 그래도 구조상 “딜레이 후 실행” 경로를 그대로 타므로 나중에 조절하기 쉽다.
	if ( m_pendingSwordWhooshTimer <= 0.0f )
		PlayPendingSwordWhoosh();

	return true;
}

void CPlayerEquipmentComponent::PlayPendingSwordWhoosh()
{
	if ( !m_pendingSwordWhoosh )
		return;

	m_pendingSwordWhoosh = false;
	m_pendingSwordWhooshTimer = 0.0f;

	if ( !m_audioManager )
		return;

	const char* path = GetSwordWhooshPath(m_pendingSwordWhooshIndex);
	if ( !path || !path[0] )
		return;

	const XMFLOAT3 pos =
		m_pOwner ? m_pOwner->GetPosition() : XMFLOAT3(0.0f, 0.0f, 0.0f);

	m_audioManager->PlaySound3D(
		path,
		pos,
		false,  // loop
		false,  // stream
		1.0f,   // volume
		false   // startPaused
	);

#if ENABLE_PLAYER_SWORD_WHOOSH_TUNING
	char buf[512];
	sprintf_s(
		buf,
		"[SwordWhoosh] sound=\"%s\" mode=\"%s\" delay=%.4f sec / %.2f ms owner=%p pos=(%.3f, %.3f, %.3f)\n",
		path,
		SwordWhooshModeText(),
		g_swordWhooshDelaySeconds,
		g_swordWhooshDelaySeconds * 1000.0f,
		static_cast< void* >( m_pOwner ),
		pos.x,
		pos.y,
		pos.z
	);
	OutputDebugStringA(buf);
#endif
}

int CPlayerEquipmentComponent::SelectSwordWhooshIndex() const
{
#if ENABLE_PLAYER_SWORD_WHOOSH_TUNING
	if ( g_debugSwordWhooshFixedIndex >= 1 && g_debugSwordWhooshFixedIndex <= 3 )
		return g_debugSwordWhooshFixedIndex;
#endif

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

#if ENABLE_PLAYER_SWORD_WHOOSH_TUNING

void CPlayerEquipmentComponent::DebugSetSwordWhooshFixedIndex(int index)
{
	if ( index < 0 ) index = 0;
	if ( index > 3 ) index = 3;

	g_debugSwordWhooshFixedIndex = index;

	char buf[256];
	sprintf_s(
		buf,
		"[SwordWhooshDebug] mode=%s delay=%.4f sec / %.2f ms\n",
		SwordWhooshModeText(),
		g_swordWhooshDelaySeconds,
		g_swordWhooshDelaySeconds * 1000.0f
	);
	OutputDebugStringA(buf);
}

void CPlayerEquipmentComponent::DebugDecreaseSwordWhooshDelayOneFrame()
{
	if ( g_swordWhooshDelaySeconds <= 0.0f )
		return;

	g_swordWhooshDelaySeconds -= kSwordWhooshDelayStepSeconds;

	if ( g_swordWhooshDelaySeconds < 0.0f )
		g_swordWhooshDelaySeconds = 0.0f;

	char buf[256];
	sprintf_s(
		buf,
		"[SwordWhooshDebug] delay decreased: %.4f sec / %.2f ms\n",
		g_swordWhooshDelaySeconds,
		g_swordWhooshDelaySeconds * 1000.0f
	);
	OutputDebugStringA(buf);
}

void CPlayerEquipmentComponent::DebugIncreaseSwordWhooshDelayOneFrame()
{
	g_swordWhooshDelaySeconds += kSwordWhooshDelayStepSeconds;

	char buf[256];
	sprintf_s(
		buf,
		"[SwordWhooshDebug] delay increased: %.4f sec / %.2f ms\n",
		g_swordWhooshDelaySeconds,
		g_swordWhooshDelaySeconds * 1000.0f
	);
	OutputDebugStringA(buf);
}

#endif