//-----------------------------------------------------------------------------
// File: AssetManager.cpp
//-----------------------------------------------------------------------------
#include "stdafx.h"
#include "AssetManager.h"

#include "Mesh.h"
#include "Material.h"
#include "Texture.h"
#include "Scene.h"
#include "Animator.h"
#include "DescriptorHeap.h"

#include <filesystem>
#include <cassert>
#include <vector>
#include <algorithm>
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")

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

	double AssetBytesToMiB(size_t bytes)
	{
		return static_cast< double >( bytes ) / ( 1024.0 * 1024.0 );
	}

	size_t StringCapacityBytesAsset(const std::string& s)
	{
		return s.capacity() + 1;
	}

	template <typename T>
	size_t VectorCapacityBytesAsset(const std::vector<T>& v)
	{
		return sizeof(T) * v.capacity();
	}

	template <typename K, typename V>
	size_t ApproxUnorderedMapBytesAsset(const std::unordered_map<K, V>& m)
	{
		return
			m.bucket_count() * sizeof(void*) +
			m.size() * ( sizeof(typename std::unordered_map<K, V>::value_type) + sizeof(void*) * 3 );
	}

	size_t EstimateBoneKeyframesBytesAsset(const BoneKeyframes& track, size_t& keyframeCount)
	{
		keyframeCount += track.keyframes.size();

		size_t bytes = sizeof(BoneKeyframes);
		bytes += StringCapacityBytesAsset(track.boneName);
		bytes += VectorCapacityBytesAsset(track.keyframes);
		return bytes;
	}

	size_t EstimateAnimationClipBytesAsset(const AnimationClip& clip, size_t& keyframeCount)
	{
		size_t bytes = sizeof(AnimationClip);
		bytes += StringCapacityBytesAsset(clip.name);
		bytes += VectorCapacityBytesAsset(clip.boneTracks);
		bytes += ApproxUnorderedMapBytesAsset(clip.boneNameToTrack);

		for ( const auto& track : clip.boneTracks )
			bytes += EstimateBoneKeyframesBytesAsset(track, keyframeCount);

		bytes += EstimateBoneKeyframesBytesAsset(clip.bindRootTrack, keyframeCount);

		bytes += VectorCapacityBytesAsset(clip.m_RefLocalPose);
		bytes += VectorCapacityBytesAsset(clip.m_refT);
		bytes += VectorCapacityBytesAsset(clip.m_refR);
		bytes += VectorCapacityBytesAsset(clip.m_refS);

		return bytes;
	}

	struct MeshReportRow
	{
		std::string key;
		MeshMemoryReport report{};
	};

	struct TextureReportRow
	{
		std::string key;
		TextureMemoryReport report{};
	};

	struct ClipReportRow
	{
		std::string key;
		size_t bytes = 0;
		size_t keyframes = 0;
	};
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

void AssetManager::ReleaseUploadBuffers()
{
	for ( auto& kv : s_assetCache )
	{
		if ( kv.second.mesh )
			kv.second.mesh->ReleaseUploadBuffers();
	}

	for ( auto& kv : s_textureCache )
	{
		if ( kv.second )
			kv.second->ReleaseUploadBuffers();
	}
}

void AssetManager::DumpMemoryReport(ID3D12Device* device)
{
#if defined(_DEBUG) || defined(DEBUG)
	OutputDebugStringA("\n");
	OutputDebugStringA("============================================================\n");
	OutputDebugStringA("[AssetMemory] AssetManager memory report begin\n");
	OutputDebugStringA("============================================================\n");

	{
		PROCESS_MEMORY_COUNTERS_EX pmc{};
		if ( GetProcessMemoryInfo(
			GetCurrentProcess(),
			reinterpret_cast< PROCESS_MEMORY_COUNTERS* >( &pmc ),
			sizeof(pmc)) )
		{
			char buf[512];
			sprintf_s(
				buf,
				"[AssetMemory][Process] WorkingSet=%zu bytes (%.3f MiB), PrivateUsage=%zu bytes (%.3f MiB)\n",
				static_cast< size_t >( pmc.WorkingSetSize ),
				AssetBytesToMiB(static_cast< size_t >( pmc.WorkingSetSize )),
				static_cast< size_t >( pmc.PrivateUsage ),
				AssetBytesToMiB(static_cast< size_t >( pmc.PrivateUsage ))
			);
			OutputDebugStringA(buf);
		}
	}

	size_t assetMapApproxBytes = 0;
	assetMapApproxBytes += ApproxUnorderedMapBytesAsset(s_assetCache);
	assetMapApproxBytes += ApproxUnorderedMapBytesAsset(s_materialCache);
	assetMapApproxBytes += ApproxUnorderedMapBytesAsset(s_textureCache);
	assetMapApproxBytes += ApproxUnorderedMapBytesAsset(s_clipCache);
	assetMapApproxBytes += ApproxUnorderedMapBytesAsset(s_appliedAssetKeysByMaterials);

	{
		char buf[1024];
		sprintf_s(
			buf,
			"[AssetMemory][CacheCounts] assets=%zu materials=%zu textures=%zu clips=%zu appliedMaterialTables=%zu cacheMapApprox=%.3f MiB\n",
			s_assetCache.size(),
			s_materialCache.size(),
			s_textureCache.size(),
			s_clipCache.size(),
			s_appliedAssetKeysByMaterials.size(),
			AssetBytesToMiB(assetMapApproxBytes)
		);
		OutputDebugStringA(buf);
	}

	std::vector<MeshReportRow> meshRows;
	meshRows.reserve(s_assetCache.size());

	size_t meshCpuTotal = 0;
	size_t meshGpuDefaultTotal = 0;
	size_t meshUploadTotal = 0;

	for ( const auto& kv : s_assetCache )
	{
		const BuiltAsset& asset = kv.second;
		if ( !asset.mesh )
			continue;

		MeshReportRow row{};
		row.key = kv.first;
		row.report = asset.mesh->GetMemoryReport();

		meshCpuTotal += row.report.CpuBytes();
		meshGpuDefaultTotal += row.report.GpuDefaultBytes();
		meshUploadTotal += row.report.UploadBytes();

		meshRows.push_back(std::move(row));
	}

	std::sort(
		meshRows.begin(),
		meshRows.end(),
		[ ] (const MeshReportRow& a, const MeshReportRow& b)
		{
			return a.report.TotalBytes() > b.report.TotalBytes();
		}
	);

	OutputDebugStringA("\n[AssetMemory] ---- Mesh cache top ----\n");

	for ( size_t i = 0; i < meshRows.size() && i < 30; ++i )
	{
		const MeshReportRow& row = meshRows[i];
		const MeshMemoryReport& r = row.report;

		char buf[2048];
		sprintf_s(
			buf,
			"[AssetMemory][MeshTop%02zu] total=%.3f MiB cpu=%.3f gpuDefault=%.3f upload=%.3f "
			"skinned=%d subMeshes=%u bones=%u vertices=%llu indices=%llu key='%s'\n",
			i,
			AssetBytesToMiB(r.TotalBytes()),
			AssetBytesToMiB(r.CpuBytes()),
			AssetBytesToMiB(r.GpuDefaultBytes()),
			AssetBytesToMiB(r.UploadBytes()),
			r.isSkinned ? 1 : 0,
			r.subMeshCount,
			r.boneCount,
			static_cast< unsigned long long >( r.vertexCount ),
			static_cast< unsigned long long >( r.indexCount ),
			row.key.c_str()
		);
		OutputDebugStringA(buf);

		sprintf_s(
			buf,
			"    [MeshBreakdown] cpuPos=%.3f cpuNrm=%.3f cpuUv=%.3f cpuTan=%.3f cpuBoneIdx=%.3f cpuBoneW=%.3f cpuIdx=%.3f gpuVB=%.3f gpuIB=%.3f uploadVB=%.3f uploadIB=%.3f\n",
			AssetBytesToMiB(r.cpuPositionBytes),
			AssetBytesToMiB(r.cpuNormalBytes),
			AssetBytesToMiB(r.cpuUvBytes),
			AssetBytesToMiB(r.cpuTangentBytes),
			AssetBytesToMiB(r.cpuBoneIndexBytes),
			AssetBytesToMiB(r.cpuBoneWeightBytes),
			AssetBytesToMiB(r.cpuIndexBytes),
			AssetBytesToMiB(r.gpuVertexBufferBytes),
			AssetBytesToMiB(r.gpuIndexBufferBytes),
			AssetBytesToMiB(r.uploadVertexBufferBytes),
			AssetBytesToMiB(r.uploadIndexBufferBytes)
		);
		OutputDebugStringA(buf);
	}

	std::vector<TextureReportRow> textureRows;
	textureRows.reserve(s_textureCache.size());

	size_t textureObjectSideTotal = 0;
	size_t textureDefaultTotal = 0;
	size_t textureUploadTotal = 0;

	for ( const auto& kv : s_textureCache )
	{
		if ( !kv.second )
			continue;

		TextureReportRow row{};
		row.key = kv.first;
		row.report = kv.second->GetMemoryReport(device);

		textureObjectSideTotal += row.report.objectSideBytes;
		textureDefaultTotal += row.report.defaultResourceBytes;
		textureUploadTotal += row.report.uploadResourceBytes;

		textureRows.push_back(std::move(row));
	}

	std::sort(
		textureRows.begin(),
		textureRows.end(),
		[ ] (const TextureReportRow& a, const TextureReportRow& b)
		{
			return a.report.TotalBytes() > b.report.TotalBytes();
		}
	);

	OutputDebugStringA("\n[AssetMemory] ---- Texture cache top ----\n");

	for ( size_t i = 0; i < textureRows.size() && i < 40; ++i )
	{
		const TextureReportRow& row = textureRows[i];
		const TextureMemoryReport& r = row.report;

		char buf[2048];
		sprintf_s(
			buf,
			"[AssetMemory][TextureTop%02zu] total=%.3f MiB default=%.3f upload=%.3f objectSide=%.3f textures=%u buffers=%u null=%u key='%s'\n",
			i,
			AssetBytesToMiB(r.TotalBytes()),
			AssetBytesToMiB(r.defaultResourceBytes),
			AssetBytesToMiB(r.uploadResourceBytes),
			AssetBytesToMiB(r.objectSideBytes),
			r.textureCount,
			r.bufferCount,
			r.nullResourceCount,
			row.key.c_str()
		);
		OutputDebugStringA(buf);
	}

	std::vector<ClipReportRow> clipRows;
	clipRows.reserve(s_clipCache.size());

	size_t clipCacheBytes = 0;
	size_t clipCacheKeyframes = 0;

	for ( const auto& kv : s_clipCache )
	{
		ClipReportRow row{};
		row.key = kv.first;
		row.bytes = EstimateAnimationClipBytesAsset(kv.second, row.keyframes);

		clipCacheBytes += row.bytes;
		clipCacheKeyframes += row.keyframes;

		clipRows.push_back(std::move(row));
	}

	std::sort(
		clipRows.begin(),
		clipRows.end(),
		[ ] (const ClipReportRow& a, const ClipReportRow& b)
		{
			return a.bytes > b.bytes;
		}
	);

	OutputDebugStringA("\n[AssetMemory] ---- Animation clip cache top ----\n");

	for ( size_t i = 0; i < clipRows.size() && i < 30; ++i )
	{
		const ClipReportRow& row = clipRows[i];

		char buf[2048];
		sprintf_s(
			buf,
			"[AssetMemory][ClipCacheTop%02zu] bytes=%zu (%.3f MiB) keyframes=%zu key='%s'\n",
			i,
			row.bytes,
			AssetBytesToMiB(row.bytes),
			row.keyframes,
			row.key.c_str()
		);
		OutputDebugStringA(buf);
	}

	{
		char buf[2048];
		sprintf_s(
			buf,
			"\n[AssetMemory][Summary] "
			"meshCpu=%.3f MiB meshGpuDefault=%.3f MiB meshUpload=%.3f MiB meshTotal=%.3f MiB | "
			"textureDefault=%.3f MiB textureUpload=%.3f MiB textureObjectSide=%.3f MiB textureTotal=%.3f MiB | "
			"clipCache=%.3f MiB clipCacheKeyframes=%zu | cacheMaps=%.3f MiB\n",
			AssetBytesToMiB(meshCpuTotal),
			AssetBytesToMiB(meshGpuDefaultTotal),
			AssetBytesToMiB(meshUploadTotal),
			AssetBytesToMiB(meshCpuTotal + meshGpuDefaultTotal + meshUploadTotal),
			AssetBytesToMiB(textureDefaultTotal),
			AssetBytesToMiB(textureUploadTotal),
			AssetBytesToMiB(textureObjectSideTotal),
			AssetBytesToMiB(textureDefaultTotal + textureUploadTotal + textureObjectSideTotal),
			AssetBytesToMiB(clipCacheBytes),
			clipCacheKeyframes,
			AssetBytesToMiB(assetMapApproxBytes)
		);
		OutputDebugStringA(buf);
	}

	OutputDebugStringA("\n[AssetMemory] ---- Animator live copies ----\n");
	CAnimator::DumpGlobalMemoryReport();

	OutputDebugStringA("\n[AssetMemory] ---- Descriptor heap usage ----\n");
	if ( CScene::m_pDescriptorHeap )
		CScene::m_pDescriptorHeap->DumpUsageReport();
	else
		OutputDebugStringA("[DescriptorHeapMemory] CScene::m_pDescriptorHeap=null\n");

	OutputDebugStringA("============================================================\n");
	OutputDebugStringA("[AssetMemory] AssetManager memory report end\n");
	OutputDebugStringA("============================================================\n\n");
#endif
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