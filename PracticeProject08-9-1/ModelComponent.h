//-----------------------------------------------------------------------------
// File: ModelComponent.h
//-----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

#include "Component.h"

class CMesh;
struct Bone;

class CModelComponent final : public CComponentT<CModelComponent>
{
public:
    explicit CModelComponent(CGameObject* owner, int nMeshes = 1);

    void Resize(int nMeshes);
    int  GetMeshCount() const;

    std::shared_ptr<CMesh> GetMeshShared(int idx) const;
    void SetMesh(int idx, std::shared_ptr<CMesh> mesh);

    std::vector<std::shared_ptr<CMesh>>& GetMeshes();
    const std::vector<std::shared_ptr<CMesh>>& GetMeshes() const;

    void ReleaseUploadBuffers();

    int GetMaxBoneCount() const;

    const std::vector<Bone>& GetBones() const;
    const std::unordered_map<std::string, int>& GetBoneNameToIndex() const;

private:
    std::vector<std::shared_ptr<CMesh>> m_meshes;
};
