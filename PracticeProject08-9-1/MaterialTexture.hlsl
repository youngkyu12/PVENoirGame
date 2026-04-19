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
    return 1.0f;
}

float ResolveLinearDepthFromDeviceZ(float deviceZ)
{
    const float nearZ = max(0.0001f, gvFogParams1.x);
    const float farZ = max(nearZ + 0.0001f, gvFogParams1.y);

    // deviceZ: [0,1] depth
    const float zNdc = deviceZ * 2.0f - 1.0f;

    const float denom = max(
        0.0001f,
        farZ + nearZ - zNdc * (farZ - nearZ)
    );

    return (2.0f * nearZ * farZ) / denom;
}

float ComputeFogFactorLinear(float linearDepth)
{
    const float fogStart = max(0.0f, gvFogParams0.x);
    const float fogEnd = max(fogStart + 0.0001f, gvFogParams0.y);

    return saturate((linearDepth - fogStart) / (fogEnd - fogStart));
}

float ComputeFogFactorExp(float linearDepth)
{
    const float density = max(0.0001f, gvFogParams0.z);
    return saturate(1.0f - exp(-linearDepth * density));
}

float ComputeFogFactorExp2(float linearDepth)
{
    const float density = max(0.0001f, gvFogParams0.z);
    const float x = linearDepth * density;
    return saturate(1.0f - exp(-(x * x)));
}

float ComputeFogFactor(float linearDepth)
{
    if (gvFogParams0.w <= 0.0f)
        return 0.0f;

    const uint fogMode = (uint) (gvFogParams1.z + 0.5f);

    float baseFogFactor = 0.0f;

    if (fogMode == 1u)
        baseFogFactor = ComputeFogFactorExp(linearDepth);
    else if (fogMode == 2u)
        baseFogFactor = ComputeFogFactorExp2(linearDepth);
    else
        baseFogFactor = ComputeFogFactorLinear(linearDepth);

    const float fogIntensity = saturate(gvFogParams1.w);
    return baseFogFactor * fogIntensity;
}



#endif