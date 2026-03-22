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

	auto LoadSharedTexture =
		[ & ] (const std::string& texName) -> std::shared_ptr<CTexture>
		{
			if ( texName.empty() )
				return std::shared_ptr<CTexture>();

			const std::wstring texPath = ResolveTexturePath(
				desc.type,
				desc.textureRoot,
				"",
				texName
			);

			const std::string texKey(texPath.begin(), texPath.end());

			auto texIt = s_textureCache.find(texKey);
			if ( texIt != s_textureCache.end() )
				return texIt->second;

			auto tex = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 1);
			tex->LoadTextureFromFile(
				device,
				cmd,
				texPath.c_str(),
				RESOURCE_TEXTURE2D,
				0
			);

			CScene::m_pDescriptorHeap->CreateShaderResourceViews(
				device,
				tex.get(),
				ROOTPARAM_TEX_SRV_TABLE
			);

			s_textureCache.emplace(texKey, tex);
			return tex;
		};

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
		mat->SetDiffuseTextureName(sm.diffuseTextureName);
		mat->SetNormalTextureName(sm.normalTextureName);
		mat->SetEmissiveTextureName(sm.emissiveTextureName);
		mat->SetSpecularTextureName(sm.specularTextureName);

		{
			auto diffuseTex = LoadSharedTexture(sm.diffuseTextureName);
			mat->SetTexture(diffuseTex);
		}

		{
			auto normalTex = LoadSharedTexture(sm.normalTextureName);
			mat->SetNormalTexture(normalTex);
		}

		{
			auto emissiveTex = LoadSharedTexture(sm.emissiveTextureName);
			mat->SetEmissiveTexture(emissiveTex);
		}

		{
			auto specularTex = LoadSharedTexture(sm.specularTextureName);
			mat->SetSpecularTexture(specularTex);
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
	if ( !pMaterials ) return;
	if ( !asset.mesh ) return;

	for ( size_t si = 0; si < asset.mesh->m_SubMeshes.size(); ++si )
	{
		const auto& sm = asset.mesh->m_SubMeshes[si];
		if ( !sm.material ) continue;

		const UINT materialId = sm.materialId;
		if ( materialId >= MAX_MATERIALS ) continue;

		const UINT diffSrvIndex = sm.material->GetDiffuseSrvIndex();
		const UINT normSrvIndex = sm.material->GetNormalSrvIndex();
		const UINT emisSrvIndex = sm.material->GetEmissiveSrvIndex();
		const UINT specSrvIndex = sm.material->GetSpecularSrvIndex();

		const UINT packedDiff = ( diffSrvIndex == UINT_MAX ) ? 0u : ( diffSrvIndex + 1u );
		const UINT packedNorm = ( normSrvIndex == UINT_MAX ) ? 0u : ( normSrvIndex + 1u );
		const UINT packedEmis = ( emisSrvIndex == UINT_MAX ) ? 0u : ( emisSrvIndex + 1u );
		const UINT packedSpec = ( specSrvIndex == UINT_MAX ) ? 0u : ( specSrvIndex + 1u );

		MATERIAL& dst = pMaterials->m_pReflections[materialId];

		dst.m_xmf4Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		dst.m_xmf4Diffuse = sm.diffuseColor;
		dst.m_xmf4Specular = sm.specularColor;
		dst.m_xmf4Emissive = sm.emissiveColor;

		dst.m_xmn4TextureIndices = XMUINT4(
			packedDiff,
			packedNorm,
			packedEmis,
			packedSpec
		);

		dst.m_xmf4DiffuseUVST = sm.diffuseTransform.uvST;
		dst.m_xmf4NormalUVST = sm.normalTransform.uvST;
		dst.m_xmf4EmissiveUVST = sm.emissiveTransform.uvST;
		dst.m_xmf4SpecularUVST = sm.specularTransform.uvST;

		dst.m_xmn4WrapModes0 = XMUINT4(
			sm.diffuseTransform.wrapModeU,
			sm.diffuseTransform.wrapModeV,
			sm.normalTransform.wrapModeU,
			sm.normalTransform.wrapModeV
		);

		dst.m_xmn4WrapModes1 = XMUINT4(
			sm.emissiveTransform.wrapModeU,
			sm.emissiveTransform.wrapModeV,
			sm.specularTransform.wrapModeU,
			sm.specularTransform.wrapModeV
		);
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