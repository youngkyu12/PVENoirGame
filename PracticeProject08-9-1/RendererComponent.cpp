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

    const int n = owner->GetMeshCount();
    for (int i = 0; i < n; ++i)
    {
        std::shared_ptr<CMesh> mesh = owner->GetMeshShared(i);
        if (mesh)
            mesh->Render(cmd, owner->GetMappedGameObjectCB());
    }

}

void CSkinnedMeshRendererComponent::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
    (void)camera;
    CGameObject* owner = GetOwner();
    if (!owner) return;

    owner->SetRootParameter(cmd);

    const int n = owner->GetMeshCount();
    for (int i = 0; i < n; ++i)
    {
        std::shared_ptr<CMesh> mesh = owner->GetMeshShared(i);
        if (mesh)
            mesh->Render(cmd, owner->GetMappedGameObjectCB());
    }

}
