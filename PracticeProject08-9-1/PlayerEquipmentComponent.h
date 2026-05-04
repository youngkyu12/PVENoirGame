//-----------------------------------------------------------------------------
// File: PlayerEquipmentComponent.h
//-----------------------------------------------------------------------------
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

public:
	bool RequestSwordAttackWhoosh();
	bool RequestAxeAttackWhoosh();
	bool RequestRollSfx(uint32_t dirBits);

private:
	enum class EPendingPlayerSfxKind : uint8_t
	{
		None = 0,
		SwordWhoosh,
		AxeWhoosh,
		Roll
	};

private:
	void SchedulePlayerSfx(
		EPendingPlayerSfxKind kind,
		const char* soundPath,
		float delaySeconds,
		float volume
	);

	void PlayPendingPlayerSfx();

	static int SelectSwordWhooshIndex();
	static const char* GetSwordWhooshPath(int index);
	static float GetSwordWhooshDelaySeconds(int index);

	static const char* GetAxeWhooshPath();
	static const char* GetRollSfxPath();

private:
	CAudioManager* m_audioManager = nullptr;

	EPendingPlayerSfxKind m_pendingSfxKind = EPendingPlayerSfxKind::None;
	const char* m_pendingSfxPath = nullptr;
	float m_pendingSfxTimer = 0.0f;
	float m_pendingSfxOriginalDelay = 0.0f;
	float m_pendingSfxVolume = 1.0f;
};