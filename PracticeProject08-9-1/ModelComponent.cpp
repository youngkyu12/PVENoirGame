//-----------------------------------------------------------------------------
// File: ModelComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "ModelComponent.h"
#include "Mesh.h" 

CModelComponent::CModelComponent(CGameObject* owner, int nMeshes)
    : CComponentT<CModelComponent>(owner)
{
    Resize(nMeshes);
}

void CModelComponent::Resize(int nMeshes)
{
    if (nMeshes < 0) nMeshes = 0;
    m_meshes.resize(static_cast<size_t>(nMeshes));
}

int CModelComponent::GetMeshCount() const
{
    return static_cast<int>(m_meshes.size());
}

std::shared_ptr<CMesh> CModelComponent::GetMeshShared(int idx) const
{
    if (idx < 0 || idx >= GetMeshCount()) return nullptr;
    return m_meshes[static_cast<size_t>(idx)];
}

void CModelComponent::SetMesh(int idx, std::shared_ptr<CMesh> mesh)
{
    if (idx < 0) return;
    if (idx >= GetMeshCount())
        m_meshes.resize(static_cast<size_t>(idx) + 1);

    m_meshes[static_cast<size_t>(idx)] = std::move(mesh);
}

std::vector<std::shared_ptr<CMesh>>& CModelComponent::GetMeshes()
{
    return m_meshes;
}

const std::vector<std::shared_ptr<CMesh>>& CModelComponent::GetMeshes() const
{
    return m_meshes;
}

void CModelComponent::ReleaseUploadBuffers()
{
    for (std::shared_ptr<CMesh>& m : m_meshes)
        if (m) m->ReleaseUploadBuffers();
}

int CModelComponent::GetMaxBoneCount() const
{
    int nBones = 0;

    for (const std::shared_ptr<CMesh>& m : m_meshes)
    {
        if (!m) continue;
        if (!m->IsSkinnedMesh()) continue;

        const int bc = m->GetBoneCount();
        if (bc > nBones) nBones = bc;
    }
    return nBones;
}

const std::vector<Bone>& CModelComponent::GetBones() const
{
    static const std::vector<Bone> empty;
    std::shared_ptr<CMesh> m0 = GetMeshShared(0);
    return m0 ? m0->GetBones() : empty;
}

const std::unordered_map<std::string, int>& CModelComponent::GetBoneNameToIndex() const
{
    static const std::unordered_map<std::string, int> empty;
    std::shared_ptr<CMesh> m0 = GetMeshShared(0);
    return m0 ? m0->GetBoneNameToIndex() : empty;
}
