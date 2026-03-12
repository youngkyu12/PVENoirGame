//-----------------------------------------------------------------------------
// File: PlayerEquipmentComponent.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "PlayerEquipmentComponent.h"

#include "Object.h"

CPlayerEquipmentComponent::CPlayerEquipmentComponent(CGameObject* owner)
    : CComponentT(owner)
{
}

void CPlayerEquipmentComponent::OnCreate(ID3D12Device* /*dev*/, ID3D12GraphicsCommandList* /*cmd*/)
{
    RefreshEquippedState();
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