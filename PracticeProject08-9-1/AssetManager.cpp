#include "stdafx.h"
#include "AssetManager.h"
#include "Mesh.h"
#include "Material.h"
#include "Texture.h"
#include "Scene.h"

BuiltAsset AssetManager::BuildAsset(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmd,
    MATERIALS* pMaterials,
    const AssetBuildDesc& desc
)
{
    // ============================================================
    // 1. Mesh 로드
    // ============================================================
    auto mesh = std::make_shared<CMesh>(device, cmd);
    mesh->LoadMeshFromBIN(
        device,
        cmd,
        desc.meshBinPath.c_str()
    );

    // ============================================================
    // 2. Material ID 발급기 + 캐시 (씬 단위)
    // ============================================================
    static UINT s_NextMaterialID = 0;
    static std::unordered_map<std::string, std::shared_ptr<CMaterial>> materialCache;

    constexpr UINT ROOTPARAM_TEX_SRV_TABLE = ROOT_PARAMETER_GLOBAL_SRV;

    // ============================================================
    // 3. SubMesh 순회하며 Material 생성 / 재사용
    // ============================================================
    for (auto& sm : mesh->m_SubMeshes)
    {
        if (sm.materialName.empty())
            continue;

        // (3-1) 캐시 재사용
        std::string key = std::to_string((int)desc.type) + "|" + sm.materialName; // 또는 desc.textureRoot 포함해도 됨
        auto it = materialCache.find(key);
        if (it != materialCache.end())
        {
            sm.material = it->second;
            sm.materialId = it->second->GetMaterialID();
            continue;
        }

        // (3-2) 새 Material 생성
        auto mat = std::make_shared<CMaterial>();

        const UINT materialId = s_NextMaterialID++;
        mat->SetMaterialID(materialId);

        assert(materialId < MAX_MATERIALS);

        // (3-3) Texture 생성 및 로드
        auto tex = std::make_shared<CTexture>(
            1,
            RESOURCE_TEXTURE2D,
            0,
            1
        );

        std::wstring texPath = ResolveTexturePath(
            desc.type,
            desc.textureRoot,
            sm.materialName,
            sm.diffuseTextureName
        );

        tex->LoadTextureFromFile(
            device,
            cmd,
            texPath.c_str(),
            RESOURCE_TEXTURE2D,
            0
        );

        // (3-4) Global SRV Heap 등록
        CScene::m_pDescriptorHeap->CreateShaderResourceViews(
            device,
            tex.get(),
            ROOTPARAM_TEX_SRV_TABLE
        );

        // (3-5) Material에 Texture 연결
        mat->SetTexture(tex);

        // (3-6) Materials CB에 SRV 인덱스 기록
        if (pMaterials)
        {
            const UINT srvIndex = mat->GetDiffuseSrvIndex();
            pMaterials->m_pReflections[materialId].m_xmn4TextureIndices.x =
                (srvIndex == UINT_MAX) ? 0xFFFFFFFFu : srvIndex;
        }

        // (3-7) 캐시 등록 + SubMesh 연결
        materialCache.emplace(sm.materialName, mat);
        sm.material = mat;
        sm.materialId = materialId;
    }

    return { mesh };
}

// ------------------------------------------------------------
// textureRoot + 정책 기반 텍스처 경로 결정
// ------------------------------------------------------------
std::wstring AssetManager::ResolveTexturePath(
    AssetType type,
    const std::string& textureRoot,
    const std::string& materialName,
    const std::string& diffuseTextureName
)
{
    // 현재는 Unitychan / Airplane 동일 규칙
    // 필요해지면 type에 따라 분기
    std::wstring rootW(textureRoot.begin(), textureRoot.end());
    std::wstring texW(diffuseTextureName.begin(), diffuseTextureName.end());

    return rootW + L"/" + texW + L".dds";
}
