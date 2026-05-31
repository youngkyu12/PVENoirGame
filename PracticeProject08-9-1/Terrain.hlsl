#ifndef __TERRAIN_HLSL__
#define __TERRAIN_HLSL__

#include "Common.hlsl"
#include "MaterialTexture.hlsl"
#include "Lighting.hlsl"
#include "RenderTypes.hlsl"

// x/y/z = terrain scale, w = use height map texture(0/1)
cbuffer cbTerrain : register(b9)
{
    float4 gvTerrainScale;
    float4 gvTerrainHeightMapSize;

        // x = height map, y/z/w unused
    uint4 gvTerrainTextureIndices;

        // x = grass, y = ground, z = dirt, w = unused
    uint4 gvTerrainDiffuseTextureIndices;

        // x = grass normal, y = ground normal, z = dirt normal, w = unused
    uint4 gvTerrainNormalTextureIndices;

        // x = mega grid size, y = village size, z = blend width, w = uv scale
    float4 gvTerrainBlendParams;
};

float4 SampleTerrainTexture(uint textureIndex, float2 uv, float4 fallback)
{
    if (textureIndex >= MAX_GLOBAL_SRVS)
        return fallback;

    return gtxtGlobalTextures[textureIndex].Sample(gssDefaultSamplerState, uv);
}

float GetTerrainVillageMask(float3 positionW)
{
    const float megaSize = gvTerrainBlendParams.x;
    const float villageSize = gvTerrainBlendParams.y;
    const float blendWidth = gvTerrainBlendParams.z;

    const float centerX = round(positionW.x / megaSize) * megaSize;
    const float centerZ = round((positionW.z - megaSize) / megaSize) * megaSize + megaSize;

    const float2 localFromCenter = positionW.xz - float2(centerX, centerZ);
    const float maxAxis = max(abs(localFromCenter.x), abs(localFromCenter.y));

    const float villageHalfSize = villageSize * 0.5f;

    return 1.0f - smoothstep(
                villageHalfSize - blendWidth,
                villageHalfSize + blendWidth,
                maxAxis
        );
}

float GetTerrainDirtRoadMask(float3 positionW)
{
    const float megaSize = gvTerrainBlendParams.x;
    const float villageSize = gvTerrainBlendParams.y;
    const float blendWidth = gvTerrainBlendParams.z;

    const float centerX = round(positionW.x / megaSize) * megaSize;
    const float centerZ = round((positionW.z - megaSize) / megaSize) * megaSize + megaSize;

    const float2 localFromCenter = positionW.xz - float2(centerX, centerZ);
    const float2 absLocal = abs(localFromCenter);

    const float terrainHalfSize = megaSize * 0.5f;
    const float villageHalfSize = villageSize * 0.5f;
    const float roadHalfWidth = villageSize * 0.25f; // 100 width -> half 50

    const float northSouthLengthMask =
                smoothstep(villageHalfSize - blendWidth, villageHalfSize + blendWidth, absLocal.y) *
                (1.0f - smoothstep(terrainHalfSize - blendWidth, terrainHalfSize + blendWidth, absLocal.y));

    const float northSouthWidthMask =
                1.0f - smoothstep(roadHalfWidth - blendWidth, roadHalfWidth + blendWidth, absLocal.x);

    const float eastWestLengthMask =
                smoothstep(villageHalfSize - blendWidth, villageHalfSize + blendWidth, absLocal.x) *
                (1.0f - smoothstep(terrainHalfSize - blendWidth, terrainHalfSize + blendWidth, absLocal.x));

    const float eastWestWidthMask =
                1.0f - smoothstep(roadHalfWidth - blendWidth, roadHalfWidth + blendWidth, absLocal.y);

    const float northSouthRoad = northSouthLengthMask * northSouthWidthMask;
    const float eastWestRoad = eastWestLengthMask * eastWestWidthMask;

    return saturate(max(northSouthRoad, eastWestRoad));
}

float3 GetTerrainScale()
{
	float3 scale = gvTerrainScale.xyz;
	return (scale.x == 0.0f || scale.y == 0.0f || scale.z == 0.0f)
		? float3(8.0f, 2.0f, 8.0f)
		: scale;
}

float2 GetTerrainHeightMapSize()
{
	float2 heightMapSize = gvTerrainHeightMapSize.xy;
	return (heightMapSize.x <= 1.0f || heightMapSize.y <= 1.0f)
		? float2(257.0f, 257.0f)
		: heightMapSize;
}

float2 GetTerrainHeightMapCoordFromCenteredLocalXZ(float2 localXZ)
{
    const float3 terrainScale = GetTerrainScale();
    const float2 heightMapSize = GetTerrainHeightMapSize();
    const float2 terrainWorldSize = (heightMapSize - 1.0f) * terrainScale.xz;
    return (localXZ + terrainWorldSize * 0.5f) / terrainScale.xz;
}

float LoadTerrainHeightMapValue(uint x, uint z)
{
	const uint heightMapIndex = gvTerrainTextureIndices.x;

	if (heightMapIndex >= MAX_GLOBAL_SRVS)
		return 0.0f;

	return gtxtGlobalTextures[heightMapIndex].Load(uint3(x, z, 0)).r;
}

float GetTerrainHeight(float fx, float fz)
{
	const float2 heightMapSize = GetTerrainHeightMapSize();

	if (fx < 0.0f || fz < 0.0f ||
		fx >= (heightMapSize.x - 1.0f) ||
		fz >= (heightMapSize.y - 1.0f))
	{
		return 0.0f;
	}

	const uint x = (uint)fx;
	const uint z = (uint)fz;
	const float fxPercent = fx - x;
	const float fzPercent = fz - z;
	const bool reverseQuad = ((z % 2u) != 0u);

	float bottomLeft = LoadTerrainHeightMapValue(x, z);
	float bottomRight = LoadTerrainHeightMapValue(x + 1u, z);
	float topLeft = LoadTerrainHeightMapValue(x, z + 1u);
	float topRight = LoadTerrainHeightMapValue(x + 1u, z + 1u);

	if (reverseQuad)
	{
		if (fzPercent >= fxPercent)
			bottomRight = bottomLeft + (topRight - topLeft);
		else
			topLeft = topRight + (bottomLeft - bottomRight);
	}
	else
	{
		if (fzPercent < (1.0f - fxPercent))
			topRight = topLeft + (bottomRight - bottomLeft);
		else
			bottomLeft = topLeft + (bottomRight - topRight);
	}

	const float topHeight = lerp(topLeft, topRight, fxPercent);
	const float bottomHeight = lerp(bottomLeft, bottomRight, fxPercent);

	return lerp(bottomHeight, topHeight, fzPercent);
}

VS_TEXTURED_LIGHTING_OUTPUT VSTerrain(VS_TEXTURED_LIGHTING_INSTANCED_INPUT input)
{
    VS_TEXTURED_LIGHTING_OUTPUT output;

    float4x4 mtxInstanceWorld = float4x4(
        input.instWorld0,
        input.instWorld1,
        input.instWorld2,
        input.instWorld3
    );

    const float3 terrainScale = GetTerrainScale();

    if (gvTerrainTextureIndices.x < MAX_GLOBAL_SRVS)
    {
        const float2 heightMapCoord = GetTerrainHeightMapCoordFromCenteredLocalXZ(input.position.xz);
        input.position.y = GetTerrainHeight(heightMapCoord.x, heightMapCoord.y) * 255.0f * terrainScale.y;
    }

    output.normalW = mul(input.normal, (float3x3) mtxInstanceWorld);
    output.positionW = (float3) mul(float4(input.position, 1.0f), mtxInstanceWorld);
    output.position = mul(mul(float4(output.positionW, 1.0f), gmtxView), gmtxProjection);
    output.uv = input.uv;

    float3 tangentW = mul(input.tangent.xyz, (float3x3) mtxInstanceWorld);
    output.tangentW = float4(tangentW, input.tangent.w);
    output.materialId = gnMaterialID;
    output.shadowPosH = mul(float4(output.positionW, 1.0f), gmtxShadowTransform);

    return output;
}


PS_MULTIPLE_RENDER_TARGETS_OUTPUT PSTerrainToMultipleRTs(
        VS_TEXTURED_LIGHTING_OUTPUT input,
        uint primitiveId : SV_PrimitiveID)
{
    PS_MULTIPLE_RENDER_TARGETS_OUTPUT output;

    const uint materialId = input.materialId;
    const MATERIAL mat = gMaterials[materialId];

    const float uvScale = gvTerrainBlendParams.w;
    const float2 terrainUV = input.positionW.xz * uvScale;

    const float villageMask = GetTerrainVillageMask(input.positionW);
    const float dirtMask = GetTerrainDirtRoadMask(input.positionW);

    const float4 grassColor = SampleTerrainTexture(
                gvTerrainDiffuseTextureIndices.x,
                terrainUV,
                float4(0.20f, 0.45f, 0.16f, 1.0f)
        );

    const float4 groundColor = SampleTerrainTexture(
                gvTerrainDiffuseTextureIndices.y,
                terrainUV,
                float4(0.45f, 0.38f, 0.28f, 1.0f)
        );
    
    const float4 dirtColor = SampleTerrainTexture(
                gvTerrainDiffuseTextureIndices.z,
                terrainUV,
                float4(0.35f, 0.25f, 0.16f, 1.0f)
        );
    
    float4 texColor = lerp(grassColor, groundColor, villageMask);
    texColor = lerp(texColor, dirtColor, dirtMask);
    texColor *= mat.m_cDiffuse;

    const float3 normalW = normalize(input.normalW);

    const float3 emissiveColor = mat.m_cEmissive.rgb;
    const float3 specularColor = mat.m_cSpecular.rgb;
    const float shininess = mat.m_cSpecular.a;

    const float4 illumination = Lighting(
                materialId,
                input.positionW,
                normalW,
                texColor,
                emissiveColor,
                specularColor,
                shininess,
                input.shadowPosH
        );

    output.cTexture = texColor;
    output.cIllumination = illumination;
    output.color = illumination;
    output.normal = float4(normalW * 0.5f + 0.5f, 1.0f);
    output.zDepth = input.position.z;

    return output;
}

float4 PSTerrain(VS_TEXTURED_LIGHTING_OUTPUT input) : SV_TARGET
{
    const float uvScale = gvTerrainBlendParams.w;
    const float2 terrainUV = input.positionW.xz * uvScale;

    const float4 grassColor = SampleTerrainTexture(
                gvTerrainDiffuseTextureIndices.x,
                terrainUV,
                float4(0.20f, 0.45f, 0.16f, 1.0f)
        );

    const float4 groundColor = SampleTerrainTexture(
                gvTerrainDiffuseTextureIndices.y,
                terrainUV,
                float4(0.45f, 0.38f, 0.28f, 1.0f)
        );
    
    const float4 dirtColor = SampleTerrainTexture(
        gvTerrainDiffuseTextureIndices.z,
        terrainUV,
        float4(0.35f, 0.25f, 0.16f, 1.0f)
  );
    
    const float villageMask = GetTerrainVillageMask(input.positionW);
    const float dirtMask = GetTerrainDirtRoadMask(input.positionW);
    
    float4 texColor = lerp(grassColor, groundColor, villageMask);
    texColor = lerp(texColor, dirtColor, dirtMask);
    
    return texColor;
}

VS_SHADOW_MAP_OUTPUT VSShadowMapTerrainInstanced(
      VS_TEXTURED_LIGHTING_INSTANCED_INPUT input)
{
    VS_SHADOW_MAP_OUTPUT output;

    float4x4 mtxInstanceWorld = float4x4(
          input.instWorld0,
          input.instWorld1,
          input.instWorld2,
          input.instWorld3
      );

    const float3 terrainScale = GetTerrainScale();

    if (gvTerrainTextureIndices.x < MAX_GLOBAL_SRVS)
    {
        const float2 heightMapCoord = GetTerrainHeightMapCoordFromCenteredLocalXZ(input.position.xz);
        input.position.y = GetTerrainHeight(heightMapCoord.x, heightMapCoord.y) * 255.0f * terrainScale.y;
    }

    float3 positionW =
          (float3) mul(float4(input.position, 1.0f), mtxInstanceWorld);

    output.position =
          mul(float4(positionW, 1.0f), gmtxShadowViewProj);

    output.uv = input.uv;
    output.materialId = gnMaterialID;

    return output;
}

#endif
