//MaterialTexture.hlsl
#ifndef __MATERIAL_TEXTURE_HLSL__
#define __MATERIAL_TEXTURE_HLSL__

#include "Common.hlsl"

struct MATERIAL
{
    float4 m_cAmbient;
    float4 m_cDiffuse;
    float4 m_cSpecular; // rgb=specular, a=shininess
    float4 m_cEmissive;

    // x=diffuse, y=normal, z=emissive, w=specular
    uint4 TextureIndices;

    // x=scaleU, y=scaleV, z=offsetU, w=offsetV
    float4 DiffuseUVST;
    float4 NormalUVST;
    float4 EmissiveUVST;
    float4 SpecularUVST;

    // x=diffuseU, y=diffuseV, z=normalU, w=normalV
    uint4 WrapModes0;

    // x=emissiveU, y=emissiveV, z=specularU, w=specularV
    uint4 WrapModes1;
};

cbuffer cbMaterial : register(b3)
{
    MATERIAL gMaterials[MAX_MATERIALS];
};

uint DecodePackedTextureIndex(uint packedIndex)
{
    return (packedIndex == 0u) ? INVALID_TEXTURE_INDEX : (packedIndex - 1u);
}

float ApplyWrap1D(float value, uint wrapMode)
{
    return (wrapMode == 1u) ? saturate(value) : frac(value);
}

float2 ApplyWrap2D(float2 uv, uint wrapModeU, uint wrapModeV)
{
    return float2(
        ApplyWrap1D(uv.x, wrapModeU),
        ApplyWrap1D(uv.y, wrapModeV)
    );
}

float2 ApplyUVST(float2 uv, float4 uvST)
{
    return uv * uvST.xy + uvST.zw;
}

float2 GetDiffuseUV(uint materialId, float2 baseUV)
{
    MATERIAL mat = gMaterials[materialId];
    return ApplyWrap2D(
        ApplyUVST(baseUV, mat.DiffuseUVST),
        mat.WrapModes0.x,
        mat.WrapModes0.y
    );
}

float2 GetNormalUV(uint materialId, float2 baseUV)
{
    MATERIAL mat = gMaterials[materialId];
    return ApplyWrap2D(
        ApplyUVST(baseUV, mat.NormalUVST),
        mat.WrapModes0.z,
        mat.WrapModes0.w
    );
}

float2 GetEmissiveUV(uint materialId, float2 baseUV)
{
    MATERIAL mat = gMaterials[materialId];
    return ApplyWrap2D(
        ApplyUVST(baseUV, mat.EmissiveUVST),
        mat.WrapModes1.x,
        mat.WrapModes1.y
    );
}

float2 GetSpecularUV(uint materialId, float2 baseUV)
{
    MATERIAL mat = gMaterials[materialId];
    return ApplyWrap2D(
        ApplyUVST(baseUV, mat.SpecularUVST),
        mat.WrapModes1.z,
        mat.WrapModes1.w
    );
}

float4 SampleTextureRGBA(uint packedIndex, float2 uv, float4 fallbackColor)
{
    float4 sampled = fallbackColor;
    uint textureIndex = DecodePackedTextureIndex(packedIndex);

    if (textureIndex == INVALID_TEXTURE_INDEX)
        return sampled;

    if (textureIndex >= MAX_GLOBAL_SRVS)
        return sampled;

    sampled = gtxtGlobalTextures[textureIndex].Sample(gsamLinearWrap, uv);
    return sampled;
}

float3 GetNormalWFromMap(uint packedNormal, float3 normalW_in, float4 tangentW_in, float2 uv)
{
    float3 N = normalize(normalW_in);
    float3 nW = N;
    
    uint normalIndex = DecodePackedTextureIndex(packedNormal);
    if (normalIndex == INVALID_TEXTURE_INDEX)
        return nW;

    if (normalIndex >= MAX_GLOBAL_SRVS)
        return nW;

    float3 nTS = gtxtGlobalTextures[normalIndex].Sample(gsamLinearWrap, uv).xyz;
    nTS = nTS * 2.0f - 1.0f;

    float3 T = normalize(tangentW_in.xyz);
    T = normalize(T - N * dot(T, N));

    float3 B = normalize(cross(N, T) * tangentW_in.w);

    nW = normalize(T * nTS.x + B * nTS.y + N * nTS.z);
    return nW;
}

//---------------------------------------------------------------------------------------
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------
float CalcShadowFactor(float4 shadowPosH)
{
    float shadowFactor = 1.0f;
    
    if (shadowPosH.w <= 0.0f)
        return shadowFactor;

    shadowPosH.xyz /= shadowPosH.w;

    if (shadowPosH.x < 0.0f || shadowPosH.x > 1.0f ||
        shadowPosH.y < 0.0f || shadowPosH.y > 1.0f ||
        shadowPosH.z < 0.0f || shadowPosH.z > 1.0f)
    {
        return shadowFactor;
    }

    float depth = shadowPosH.z;

    uint width, height, numMips;
    gtxtGlobalTextures[SHADOW_MAP_SRV_INDEX].GetDimensions(0, width, height, numMips);

    float dx = 1.0f / (float) width;

    float percentLit = 0.0f;
    const float2 offsets[9] =
    {
        float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
        float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
    };

    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        const float sampledDepth =
            gtxtGlobalTextures[SHADOW_MAP_SRV_INDEX].SampleLevel(
                gsamLinearClamp,
                shadowPosH.xy + offsets[i],
                0.0f
            ).r;

        percentLit += (depth <= sampledDepth + 0.0005f) ? 1.0f : 0.0f;
    }


    shadowFactor = percentLit / 9.0f;
    return shadowFactor;
}

#endif