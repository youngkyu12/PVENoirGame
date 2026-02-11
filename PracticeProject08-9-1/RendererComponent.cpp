//-----------------------------------------------------------------------------
// File: RendererComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "RendererComponent.h"
#include "Object.h"
#include "Camera.h"

void CStaticMeshRendererComponent::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
    (void)camera;

    CGameObject* owner = GetOwner();
    if (!owner) return;

    owner->SetRootParameter(cmd);

    if (!owner->m_ppMeshes.empty())
    {
        for (int i = 0; i < owner->m_nMeshes; ++i)
        {
            if (owner->m_ppMeshes[i])
                owner->m_ppMeshes[i]->Render(cmd, owner->GetMappedGameObjectCB());
        }
    }
}

void CSkinnedMeshRendererComponent::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
    (void)camera;
    CGameObject* owner = GetOwner();
    if (!owner) return;

    owner->SetRootParameter(cmd);

    for (int i = 0; i < owner->m_nMeshes; ++i)
        if (owner->m_ppMeshes[i])
            owner->m_ppMeshes[i]->Render(cmd, owner->GetMappedGameObjectCB());
}
