//-----------------------------------------------------------------------------
// File: AssetManager.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "AssetManager.h"

#include "Mesh.h"
#include "Material.h"
#include "Texture.h"
#include "Scene.h"

#include <filesystem>
#include <cassert>

std::unordered_map<std::string, BuiltAsset> AssetManager::s_assetCache;
std::unordered_map<std::string, std::shared_ptr<CMaterial>> AssetManager::s_materialCache;
std::unordered_map<std::string, std::shared_ptr<CTexture>> AssetManager::s_textureCache;
UINT AssetManager::s_nextMaterialID = 0;

BuiltAsset AssetManager::BuildAsset(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmd,
    MATERIALS* pMaterials,
    const AssetBuildDesc& desc)
{
    const std::string assetKey = MakeAssetKey(desc);

    auto it = s_assetCache.find(assetKey);
    if (it == s_assetCache.end())
    {
        BuiltAsset built = BuildAssetInternal(device, cmd, desc);
        it = s_assetCache.emplace(assetKey, std::move(built)).first;
    }

    if (pMaterials)
    {
        ApplyBuiltAssetToSceneMaterials(it->second, pMaterials);
    }

    return it->second;
}

void AssetManager::ClearCache()
{
    s_assetCache.clear();
    s_materialCache.clear();
    s_textureCache.clear();
    s_nextMaterialID = 0;
}

BuiltAsset AssetManager::BuildAssetInternal(
    ID3D12Device* device,
    ID3D12GraphicsCommandList* cmd,
    const AssetBuildDesc& desc)
{
    auto mesh = std::make_shared<CMesh>(device, cmd);
    mesh->LoadMeshFromBIN(
        device,
        cmd,
        desc.meshBinPath.c_str()
    );

    constexpr UINT ROOTPARAM_TEX_SRV_TABLE = ROOT_PARAMETER_GLOBAL_SRV;

    for (size_t si = 0; si < mesh->m_SubMeshes.size(); ++si)
    {
        auto& sm = mesh->m_SubMeshes[si];

        if (sm.materialName.empty())
            continue;

        const std::string materialKey = MakeMaterialKey(
            desc.type,
            desc.textureRoot,
            sm.materialName,
            sm.diffuseTextureName,
            sm.normalTextureName
        );

        auto matIt = s_materialCache.find(materialKey);
        if (matIt != s_materialCache.end())
        {
            sm.material = matIt->second;
            sm.materialId = matIt->second->GetMaterialID();
            continue;
        }

        auto mat = std::make_shared<CMaterial>();

        const UINT materialId = s_nextMaterialID++;
        assert(materialId < MAX_MATERIALS);

        mat->SetMaterialID(materialId);

        std::shared_ptr<CTexture> diffuseTex;
        {
            const std::wstring diffusePath = ResolveTexturePath(
                desc.type,
                desc.textureRoot,
                sm.materialName,
                sm.diffuseTextureName
            );

            const std::string diffuseKey(diffusePath.begin(), diffusePath.end());

            auto texIt = s_textureCache.find(diffuseKey);
            if (texIt != s_textureCache.end())
            {
                diffuseTex = texIt->second;
            }
            else
            {
                diffuseTex = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 1);
                diffuseTex->LoadTextureFromFile(
                    device,
                    cmd,
                    diffusePath.c_str(),
                    RESOURCE_TEXTURE2D,
                    0
                );

                CScene::m_pDescriptorHeap->CreateShaderResourceViews(
                    device,
                    diffuseTex.get(),
                    ROOTPARAM_TEX_SRV_TABLE
                );

                s_textureCache.emplace(diffuseKey, diffuseTex);
            }

            mat->SetTexture(diffuseTex);
        }

        if (!sm.normalTextureName.empty())
        {
            std::shared_ptr<CTexture> normalTex;

            const std::wstring normalPath = ResolveTexturePath(
                desc.type,
                desc.textureRoot,
                sm.materialName,
                sm.normalTextureName
            );

            const std::string normalKey(normalPath.begin(), normalPath.end());

            auto texIt = s_textureCache.find(normalKey);
            if (texIt != s_textureCache.end())
            {
                normalTex = texIt->second;
            }
            else
            {
                normalTex = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 1);
                normalTex->LoadTextureFromFile(
                    device,
                    cmd,
                    normalPath.c_str(),
                    RESOURCE_TEXTURE2D,
                    0
                );

                CScene::m_pDescriptorHeap->CreateShaderResourceViews(
                    device,
                    normalTex.get(),
                    ROOTPARAM_TEX_SRV_TABLE
                );

                s_textureCache.emplace(normalKey, normalTex);
            }

            mat->SetNormalTexture(normalTex);
        }
        else
        {
            mat->SetNormalTexture(std::shared_ptr<CTexture>());
        }

        sm.material = mat;
        sm.materialId = materialId;

        s_materialCache.emplace(materialKey, mat);
    }

    return { mesh };
}

void AssetManager::ApplyBuiltAssetToSceneMaterials(
    const BuiltAsset& asset,
    MATERIALS* pMaterials)
{
    if (!pMaterials) return;
    if (!asset.mesh) return;

    for (size_t si = 0; si < asset.mesh->m_SubMeshes.size(); ++si)
    {
        const auto& sm = asset.mesh->m_SubMeshes[si];
        if (!sm.material) continue;

        const UINT materialId = sm.materialId;
        if (materialId >= MAX_MATERIALS) continue;

        const UINT diffSrvIndex = sm.material->GetDiffuseSrvIndex();
        const UINT normSrvIndex = sm.material->GetNormalSrvIndex();

        const UINT packedDiff = (diffSrvIndex == UINT_MAX) ? 0u : (diffSrvIndex + 1u);
        const UINT packedNorm = (normSrvIndex == UINT_MAX) ? 0u : (normSrvIndex + 1u);

        pMaterials->m_pReflections[materialId].m_xmn4TextureIndices.x = packedDiff;
        pMaterials->m_pReflections[materialId].m_xmn4TextureIndices.y = packedNorm;
    }
}

std::string AssetManager::MakeAssetKey(const AssetBuildDesc& desc)
{
    return
        std::to_string((int)desc.type) + "|" +
        desc.meshBinPath + "|" +
        desc.textureRoot;
}

std::string AssetManager::MakeMaterialKey(
    AssetType type,
    const std::string& textureRoot,
    const std::string& materialName,
    const std::string& diffuseTextureName,
    const std::string& normalTextureName)
{
    return
        std::to_string((int)type) + "|" +
        textureRoot + "|" +
        materialName + "|" +
        diffuseTextureName + "|" +
        normalTextureName;
}

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