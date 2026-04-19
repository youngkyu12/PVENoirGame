#pragma once

#include "Component.h"
#include <DirectXMath.h>

using namespace DirectX;

class CGameObject;

class CBulletComponent final : public CComponentT<CBulletComponent>
{
public:
	explicit CBulletComponent(CGameObject* owner);

	bool FireFromObjects(
		CGameObject* spawnSource,
		CGameObject* directionSource,
		float speed,
		float lifeSec,
		bool firedByPlayer = true
	);

	void Activate(
		const XMFLOAT3& position,
		const XMFLOAT3& velocity,
		float lifeSec,
		bool firedByPlayer = true
	);

	void Deactivate();

	bool IsActive() const { return m_state == EState::Flying; }

	void OnUpdate(float dt) override;

private:
	enum class EState : uint8_t
	{
		Inactive = 0,
		Flying
	};
private:
	static XMFLOAT3 NormalizeSafe(const XMFLOAT3& v);
	static XMFLOAT3 GetForwardFromObject(const CGameObject* obj);
	void ApplyProjectileColliderProfile();

private:
	EState   m_state = EState::Inactive;
	float    m_lifeRemaining = 0.0f;
	XMFLOAT3 m_velocity = { 0.0f, 0.0f, 0.0f };
	bool     m_firedByPlayer = true;
};