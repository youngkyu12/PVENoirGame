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
    uint textureIndex = DecodePackedTextureIndex(packedIndex);

    if (textureIndex == INVALID_TEXTURE_INDEX)
        return fallbackColor;

    if (textureIndex >= MAX_GLOBAL_SRVS)
        return fallbackColor;

    return gtxtGlobalTextures[textureIndex].Sample(gssDefaultSamplerState, uv);
}

float CalcShadowFactor(float4 shadowPosH, float3 normalW, float3 vToLight)
{
    if (gvShadowParams1.y == 0u)
        return 1.0f;

    const uint shadowMapIdx = gvShadowParams1.x;
    if (shadowMapIdx == 0xffffffffu || shadowMapIdx >= MAX_GLOBAL_SRVS)
        return 1.0f;

    float invW = rcp(max(0.0001f, shadowPosH.w));
    float3 proj = shadowPosH.xyz * invW;

    if (proj.x < 0.0f || proj.x > 1.0f ||
        proj.y < 0.0f || proj.y > 1.0f ||
        proj.z < 0.0f || proj.z > 1.0f)
    {
        return 1.0f;
    }

    float ndotl = saturate(dot(normalize(normalW), normalize(vToLight)));
    float bias = max(gvShadowParams0.y, gvShadowParams0.z * (1.0f - ndotl));

    const float shadowMapSize = max(gvShadowParams0.x, 1.0f);
    const float2 texelSize = float2(1.0f / shadowMapSize, 1.0f / shadowMapSize);

    float shadowSum = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            const float2 sampleUv = proj.xy + float2((float) x, (float) y) * texelSize;

            shadowSum += gtxtGlobalTextures[shadowMapIdx].SampleCmpLevelZero(
                gssShadowSampler,
                sampleUv,
                proj.z - bias
            );
        }
    }

    const float shadowLit = shadowSum / 9.0f;
    return lerp(1.0f, shadowLit, saturate(gvShadowParams0.w));
}

float3 GetNormalWFromMap(uint packedNormal, float3 normalW_in, float4 tangentW_in, float2 uv)
{
    float3 N = normalize(normalW_in);

    uint normalIndex = DecodePackedTextureIndex(packedNormal);
    if (normalIndex == INVALID_TEXTURE_INDEX)
        return N;

    if (normalIndex >= MAX_GLOBAL_SRVS)
        return N;

    float3 nTS = gtxtGlobalTextures[normalIndex].Sample(gssDefaultSamplerState, uv).xyz;
    nTS = nTS * 2.0f - 1.0f;

    float3 T = normalize(tangentW_in.xyz);
    T = normalize(T - N * dot(T, N));

    float3 B = normalize(cross(N, T) * tangentW_in.w);

    float3 nW = normalize(T * nTS.x + B * nTS.y + N * nTS.z);
    return nW;
}

#endif