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
	// shadow pass는 현재 obj->Render(cmd, nullptr) 형태로 호출되고,
	// 현재 shadow 전용 VS는 스키닝을 처리하지 않는다.
	// 따라서 phase 1에서는 skinned renderer를 shadow pass에서 강제 제외한다.
	if ( !camera )
		return;

	CGameObject* owner = GetOwner();
	if ( !owner )
		return;

	owner->SetRootParameter(cmd);

	const int n = owner->GetMeshCount();
	for ( int i = 0; i < n; ++i )
	{
		std::shared_ptr<CMesh> mesh = owner->GetMeshShared(i);
		if ( mesh )
			mesh->Render(cmd, owner->GetMappedGameObjectCB());
	}
}

void CColliderMeshRendererComponent::Render(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	( void ) camera;
	CGameObject* owner = GetOwner();
	if ( !owner ) return;

	owner->SetRootParameter(cmd);

	const int n = owner->GetMeshCount();
	for ( int i = 0; i < n; ++i )
	{
		std::shared_ptr<CMesh> mesh = owner->GetMeshShared(i);
		if ( mesh )
			mesh->Render(cmd, owner->GetMappedGameObjectCB());
	}
}
