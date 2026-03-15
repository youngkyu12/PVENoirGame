#include "stdafx.h"
#include "AssetManager.h"
#include "Mesh.h"
#include "Material.h"
#include "Texture.h"
#include "Scene.h"

#include <filesystem>
#include <cassert>

inline void DebugLogA(const std::string& msg) { OutputDebugStringA(msg.c_str()); }
inline void DebugLogW(const std::wstring& msg) { OutputDebugStringW(msg.c_str()); }

BuiltAsset AssetManager::BuildAsset(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmd,
    MATERIALS* pMaterials,
    const AssetBuildDesc& desc)
{
    // ============================================================
    // 1. Mesh 로드
    // ============================================================
    auto mesh = std::make_shared<CMesh>(device, cmd);
    mesh->LoadMeshFromBIN(
        device, 
        cmd, 
        desc.meshBinPath.c_str());

    // ============================================================
    // 2. Material ID 발급기 + 캐시 (씬 단위)
    // ============================================================
    static UINT s_NextMaterialID = 0;
    static std::unordered_map<std::string, std::shared_ptr<CMaterial>> materialCache;

    constexpr UINT ROOTPARAM_TEX_SRV_TABLE = ROOT_PARAMETER_GLOBAL_SRV;

    // ============================================================
    // 3. SubMesh 순회하며 Material 생성 / 재사용
    // ============================================================
    for (size_t si = 0; si < mesh->m_SubMeshes.size(); ++si)
    {
        auto& sm = mesh->m_SubMeshes[si];

        if (sm.materialName.empty())
            continue;

        const std::string key = std::to_string((int)desc.type) + "|" + sm.materialName;

        // (3-1) 캐시 조회
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

        // ============================================================
        // (3-3) Diffuse Texture 생성 및 로드
        // ============================================================
        auto diffuseTex = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 1);

        const std::wstring diffusePath = ResolveTexturePath(
            desc.type,
            desc.textureRoot,
            sm.materialName,
            sm.diffuseTextureName);

        diffuseTex->LoadTextureFromFile(
            device, 
            cmd, 
            diffusePath.c_str(), 
            RESOURCE_TEXTURE2D, 
            0);

        // (3-4) Global SRV Heap 등록 (Diffuse)
        CScene::m_pDescriptorHeap->CreateShaderResourceViews(
            device,
            diffuseTex.get(),
            ROOTPARAM_TEX_SRV_TABLE);

        // (3-5) Material에 Diffuse Texture 연결
        mat->SetTexture(diffuseTex);

        // ============================================================
        // [ADD] 여기(= mat->SetTexture(diffuseTex) 바로 다음)에 Normal Texture 블럭
        // ============================================================
        std::shared_ptr<CTexture> normalTex;

        if (!sm.normalTextureName.empty())
        {
            normalTex = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 1);

            const std::wstring normalPath = ResolveTexturePath(
                desc.type,
                desc.textureRoot,
                sm.materialName,
                sm.normalTextureName
            );

            normalTex->LoadTextureFromFile(device, cmd, normalPath.c_str(), RESOURCE_TEXTURE2D, 0);

            // Global SRV Heap 등록 (Normal)
            CScene::m_pDescriptorHeap->CreateShaderResourceViews(
                device,
                normalTex.get(),
                ROOTPARAM_TEX_SRV_TABLE
            );

            // Material에 Normal Texture 연결
            mat->SetNormalTexture(normalTex);
        }
        else
        {
            // 노멀맵 없으면 null로 연결(인덱스 UINT_MAX로 세팅)
            mat->SetNormalTexture(std::shared_ptr<CTexture>());
        }

        // ============================================================
        // (3-6) Materials CB에 SRV 인덱스 기록 (x=diffuse, y=normal)
        //   packed 규칙: 0 = no texture, 1..N = (srvIndex + 1)
        // ============================================================
        const UINT diffSrvIndex = mat->GetDiffuseSrvIndex();
        const UINT normSrvIndex = mat->GetNormalSrvIndex();

        const UINT packedDiff = (diffSrvIndex == UINT_MAX) ? 0u : (diffSrvIndex + 1u);
        const UINT packedNorm = (normSrvIndex == UINT_MAX) ? 0u : (normSrvIndex + 1u);

        if (pMaterials)
        {
            pMaterials->m_pReflections[materialId].m_xmn4TextureIndices.x = packedDiff;
            pMaterials->m_pReflections[materialId].m_xmn4TextureIndices.y = packedNorm;
        }

        // (3-7) 캐시 등록 + SubMesh 연결
        materialCache.emplace(key, mat);
        sm.material = mat;
        sm.materialId = materialId;
    }

    return { mesh };
}

// ------------------------------------------------------------
// textureRoot + 정책 기반 텍스처 경로 결정
// ------------------------------------------------------------
std::wstring AssetManager::ResolveTexturePath(
    AssetType /*type*/,
    const std::string& textureRoot,
    const std::string& /*materialName*/,
    const std::string& texName)
{
    std::wstring rootW(textureRoot.begin(), textureRoot.end());
    std::wstring texW(texName.begin(), texName.end());
    return rootW + L"/" + texW + L".dds";
}
