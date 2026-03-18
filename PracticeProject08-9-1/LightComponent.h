//-----------------------------------------------------------------------------
// File: LightComponent.h
//-----------------------------------------------------------------------------
#pragma once
#include "Object.h"
#include "LightTypes.h"

enum class ELightType : int
{
    Point = POINT_LIGHT,
    Spot = SPOT_LIGHT,
    Directional = DIRECTIONAL_LIGHT,
};

class CLightComponent final : public CComponentT<CLightComponent>
{
public:
    explicit CLightComponent(CGameObject* owner) : CComponentT(owner) {}

    ELightType type = ELightType::Point;

    XMFLOAT4 ambient = { 0.0f, 0.0f, 0.0f, 0.0f };
    XMFLOAT4 diffuse = { 0.0f, 0.0f, 0.0f, 0.0f };
    XMFLOAT4 specular = { 0.0f, 0.0f, 0.0f, 0.0f };
    XMFLOAT4 Emmision = { 0.0f, 0.0f, 0.0f, 0.0f };;

    float    range{};
    XMFLOAT3 attenuation = { 1.0f, 0.01f, 0.0001f };
    float    falloff{};

    // Spot only: cos(theta), cos(phi)
    float cosTheta{};
    float cosPhi{};

    void Fill(LIGHT& out) const
    {
        ::ZeroMemory(&out, sizeof(LIGHT));

        out.m_bEnable = IsEnabled();
        out.m_nType = (int)type;

        out.m_xmf4Ambient = ambient;
        out.m_xmf4Diffuse = diffuse;
        out.m_xmf4Specular = specular;

        out.m_fRange = range;
        out.m_xmf3Attenuation = attenuation;
        out.m_fFalloff = falloff;

        out.m_fTheta = cosTheta;
        out.m_fPhi = cosPhi;

        if (auto* tr = GetOwner()->GetComponent<CTransformComponent>())
        {
            out.m_xmf3Position = tr->position;
            out.m_xmf3Direction = tr->GetLook(); // Transform¿« forward
        }
    }
};

class CFollowTransformComponent final : public CComponentT<CFollowTransformComponent>
{
public:
    explicit CFollowTransformComponent(CGameObject* owner) : CComponentT(owner) {}

    void SetTarget(CGameObject* target) { m_pTarget = target; }
    CGameObject* GetTarget() const { return m_pTarget; }

    void OnUpdate(float) override
    {
        if (!m_pTarget) return;

        auto* ownerTr = GetOwner()->GetComponent<CTransformComponent>();
        auto* targetTr = m_pTarget->GetComponent<CTransformComponent>();
        if (!ownerTr || !targetTr) return;

        ownerTr->SetPosition(targetTr->position);
        ownerTr->SetRotationFromBasis(targetTr->GetRight(), targetTr->GetUp(), targetTr->GetLook());
    }

private:
    CGameObject* m_pTarget = nullptr;
};