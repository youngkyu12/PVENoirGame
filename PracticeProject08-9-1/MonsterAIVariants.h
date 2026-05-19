//-----------------------------------------------------------------------------
// File: MonsterAIVariants.h
//-----------------------------------------------------------------------------

#pragma once

#include "MonsterAIComponent.h"

//-----------------------------------------------------------------------------
// Ghoul
//-----------------------------------------------------------------------------
class CGhoulAIComponent final : public CMonsterAIComponent
{
public:
	explicit CGhoulAIComponent(CGameObject* owner);
	~CGhoulAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CGhoulAIComponent>();
	}

protected:
	bool TryPerformAttack() override;
};

//-----------------------------------------------------------------------------
// SwordMan
//-----------------------------------------------------------------------------
class CSwordManAIComponent final : public CMonsterAIComponent
{
public:
	explicit CSwordManAIComponent(CGameObject* owner);
	~CSwordManAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CSwordManAIComponent>();
	}

protected:
	bool TryPerformAttack() override;
};

//-----------------------------------------------------------------------------
// BowMan
//-----------------------------------------------------------------------------
class CBowManAIComponent final : public CMonsterAIComponent
{
public:
	explicit CBowManAIComponent(CGameObject* owner);
	~CBowManAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CBowManAIComponent>();
	}

protected:
	bool CanStartAttackAgainstTarget() const override;
	bool TryPerformAttack() override;
};

//-----------------------------------------------------------------------------
// Mutant
//-----------------------------------------------------------------------------
class CMutantAIComponent final : public CMonsterAIComponent
{
public:
	explicit CMutantAIComponent(CGameObject* owner);
	~CMutantAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CMutantAIComponent>();
	}

protected:
	bool TryPerformAttack() override;
};

//-----------------------------------------------------------------------------
// Boss
//-----------------------------------------------------------------------------
class CBossAIComponent final : public CMonsterAIComponent
{
public:
	explicit CBossAIComponent(CGameObject* owner);
	~CBossAIComponent() override = default;

public:
	TypeId GetTypeId() const override
	{
		return CComponent::StaticTypeId<CBossAIComponent>();
	}

protected:
	bool TryPerformAttack() override;
};