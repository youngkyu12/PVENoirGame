//-----------------------------------------------------------------------------
// File: PlayerEquipmentComponent.h
//-----------------------------------------------------------------------------
#ifndef ENABLE_PLAYER_SWORD_WHOOSH_TUNING
#define ENABLE_PLAYER_SWORD_WHOOSH_TUNING 1
#endif
#pragma once

#include <array>
#include <cstdint>

#include "Component.h"

class CGameObject;
class CAudioManager;

enum class EWeaponType : uint8_t
{
    Sword = 0,
    Bow = 1,
    Axe = 2,
    Gun = 3,
    Count = 4,
    None = 0xFF
};

class CPlayerEquipmentComponent final : public CComponentT<CPlayerEquipmentComponent>
{
public:
    explicit CPlayerEquipmentComponent(CGameObject* owner);

    void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;

public:
    // ------------------------------------------------------------------------
    // Weapon object references (not owned)
    // ------------------------------------------------------------------------
    void SetWeaponObject(EWeaponType type, CGameObject* weaponObject);
    CGameObject* GetWeaponObject(EWeaponType type) const;
    void ClearWeaponObjects();

public:
    // ------------------------------------------------------------------------
    // Loadout / ownership
    // - player can own 0~2 weapons
    // - if owns 1 weapon, must equip it
    // - if owns 2 weapons, default equip = first
    // ------------------------------------------------------------------------
    void ClearOwnedWeapons();
    bool AddOwnedWeapon(EWeaponType type); // max 2, duplicate ignored
    bool HasWeapon(EWeaponType type) const;

    int GetOwnedWeaponCount() const { return m_ownedCount; }
    EWeaponType GetOwnedWeapon(int index) const;

    void SetLoadout(EWeaponType first, EWeaponType second = EWeaponType::None);

public:
    // ------------------------------------------------------------------------
    // Equip / swap
    // ------------------------------------------------------------------------
    EWeaponType GetEquippedWeapon() const { return m_equippedWeapon; }
    CGameObject* GetEquippedWeaponObject() const;

    bool EquipWeapon(EWeaponType type);
    bool EquipOwnedWeaponByIndex(int index);
    bool SwapWeapon(); // if 2 owned -> toggle, if 1 owned -> keep current
    void Unequip();

private:
    static bool IsWeaponType(EWeaponType type);
    static int  ToIndex(EWeaponType type);

    void RefreshEquippedState();
    void RefreshWeaponVisibility();
    void SetWeaponObjectVisible(EWeaponType type, bool visible);

private:
    // 4 weapon object refs: Sword/Bow/Axe/Gun
    std::array<CGameObject*, 4> m_weaponObjects = { nullptr, nullptr, nullptr, nullptr };

    // owned weapon list (max 2)
    std::array<EWeaponType, 2> m_ownedWeapons = { EWeaponType::None, EWeaponType::None };
    int m_ownedCount = 0;

    // actually equipped weapon
    EWeaponType m_equippedWeapon = EWeaponType::None;

public:
	void OnUpdate(float dt) override;

	void SetAudioManager(CAudioManager* audioManager) { m_audioManager = audioManager; }

	// 공격이 실제 accepted 되었을 때 GameScene에서 호출한다.
	bool RequestSwordAttackWhoosh();

#if ENABLE_PLAYER_SWORD_WHOOSH_TUNING
public:
	// 0 = 랜덤, 1~3 = Whoosh_Sword1~3 고정
	static void DebugSetSwordWhooshFixedIndex(int index);
	static void DebugDecreaseSwordWhooshDelayOneFrame();
	static void DebugIncreaseSwordWhooshDelayOneFrame();
#endif

private:
	void PlayPendingSwordWhoosh();
	int SelectSwordWhooshIndex() const;
	static const char* GetSwordWhooshPath(int index);

private:
	CAudioManager* m_audioManager = nullptr;

	bool  m_pendingSwordWhoosh = false;
	float m_pendingSwordWhooshTimer = 0.0f;
	int   m_pendingSwordWhooshIndex = 1;
};