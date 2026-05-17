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
std::unordered_map<std::string, AnimationClip> AssetManager::s_clipCache;

std::unordered_map<MATERIALS*, std::unordered_set<std::string>>
AssetManager::s_appliedAssetKeysByMaterials;

UINT AssetManager::s_nextMaterialID = 0;

namespace
{
	void AppendFloatToKey(std::string& out, float v)
	{
		out += std::to_string(v);
		out += "|";
	}

	void AppendUIntToKey(std::string& out, UINT v)
	{
		out += std::to_string(v);
		out += "|";
	}

	void AppendFloat4ToKey(std::string& out, const XMFLOAT4& v)
	{
		AppendFloatToKey(out, v.x);
		AppendFloatToKey(out, v.y);
		AppendFloatToKey(out, v.z);
		AppendFloatToKey(out, v.w);
	}

	void AppendTexTransformToKey(std::string& out, const BinMaterialTexTransform& t)
	{
		AppendFloat4ToKey(out, t.uvST);
		AppendUIntToKey(out, t.wrapModeU);
		AppendUIntToKey(out, t.wrapModeV);
	}

	std::string BuildMaterialFingerprint(const SubMesh& sm)
	{
		std::string key;

		AppendFloat4ToKey(key, sm.diffuseColor);
		AppendFloat4ToKey(key, sm.emissiveColor);
		AppendFloat4ToKey(key, sm.specularColor);

		AppendTexTransformToKey(key, sm.diffuseTransform);
		AppendTexTransformToKey(key, sm.normalTransform);
		AppendTexTransformToKey(key, sm.emissiveTransform);
		AppendTexTransformToKey(key, sm.specularTransform);

		return key;
	}
}

BuiltAsset AssetManager::BuildAsset(
	ID3D12Device* device,
	ID3D12GraphicsCommandList* cmd,
	MATERIALS* pMaterials,
	const AssetBuildDesc& desc)
{
	const std::string assetKey = MakeAssetKey(desc);

	auto it = s_assetCache.find(assetKey);
	if ( it == s_assetCache.end() )
	{
		BuiltAsset built = BuildAssetInternal(device, cmd, desc);
		it = s_assetCache.emplace(assetKey, std::move(built)).first;
	}

	if ( pMaterials )
	{
		auto& appliedAssetKeys = s_appliedAssetKeysByMaterials[pMaterials];

		const bool firstApplyToThisMaterialTable =
			appliedAssetKeys.insert(assetKey).second;

		if ( firstApplyToThisMaterialTable )
		{
			ApplyBuiltAssetToSceneMaterials(it->second, pMaterials);
		}
	}

	return it->second;
}

void AssetManager::BeginSceneMaterialBuild(MATERIALS* pMaterials)
{
	if ( !pMaterials )
		return;

	s_appliedAssetKeysByMaterials[pMaterials].clear();
}

void AssetManager::ClearCache()
{
	s_assetCache.clear();
	s_materialCache.clear();
	s_textureCache.clear();
	s_clipCache.clear();
	s_appliedAssetKeysByMaterials.clear();
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

		const std::string materialFingerprint = BuildMaterialFingerprint(sm);

		const std::string materialKey = MakeMaterialKey(
			desc.type,
			desc.textureRoot,
			sm.materialName,
			materialFingerprint
		);

        auto matIt = s_materialCache.find(materialKey);
        if (matIt != s_materialCache.end())
        {
            sm.material = matIt->second;
            sm.materialId = matIt->second->GetMaterialID();
            continue;
        }

        auto mat = std::make_shared<CMaterial>();

		while ( s_nextMaterialID == ( MAX_MATERIALS - 1 ) )
			++s_nextMaterialID;

		const UINT materialId = s_nextMaterialID++;
		assert(materialId < ( MAX_MATERIALS - 1 ));

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
	const std::string& materialFingerprint)
{
	return
		std::to_string(( int ) type) + "|" +
		textureRoot + "|" +
		materialName + "|" +
		materialFingerprint;
}

std::string AssetManager::MakeClipKey(
	const std::string& skeletonKey,
	const std::string& animBinPath,
	const std::string& clipName,
	float timeScale)
{
	return
		skeletonKey + "|" +
		animBinPath + "|" +
		clipName + "|" +
		std::to_string(timeScale);
}

bool AssetManager::LoadCachedClip(
	CMesh* mesh,
	const std::string& skeletonKey,
	const char* animBinPath,
	const char* clipName,
	AnimationClip& outClip,
	float timeScale)
{
	if ( !mesh ) return false;
	if ( !animBinPath || !clipName ) return false;

	const std::string key = MakeClipKey(
		skeletonKey,
		animBinPath,
		clipName,
		timeScale
	);

	auto it = s_clipCache.find(key);
	if ( it != s_clipCache.end() )
	{
		outClip = it->second;
		return true;
	}

	AnimationClip clip{};
	if ( !mesh->LoadAnimationFromBIN(animBinPath, clipName, clip, timeScale) )
		return false;

	clip.name = clipName;
	s_clipCache.emplace(key, clip);
	outClip = clip;
	return true;
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