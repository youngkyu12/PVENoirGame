#ifndef __SKINNED_HLSL__
#define __SKINNED_HLSL__

#include "Common.hlsl"

struct VS_SKINNED_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
    uint4 blendIndices : BLENDINDICES;
    float4 blendWeights : BLENDWEIGHT;
};

struct VS_SKINNED_INSTANCED_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
    uint4 blendIndices : BLENDINDICES;
    float4 blendWeights : BLENDWEIGHT;

    float4 instWorld0 : INSTANCE_WORLD0;
    float4 instWorld1 : INSTANCE_WORLD1;
    float4 instWorld2 : INSTANCE_WORLD2;
    float4 instWorld3 : INSTANCE_WORLD3;
    uint instMaterialId : INSTANCE_MATERIAL_ID0;
    uint instBoneBase : INSTANCE_BONE_BASE0;
};

struct VS_SKINNED_OUTPUT
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangentW : TANGENT;
    nointerpolation uint materialId : MATERIAL_ID;
    float4 shadowPosH : TEXCOORD1;
};

float4 SkinPosition4(float3 position, uint4 blendIndices, float4 blendWeights, uint boneBase)
{
    float4 posL = float4(position, 1.0f);

    float4 p0 = mul(posL, gBonePalette[boneBase + blendIndices.x]) * blendWeights.x;
    float4 p1 = mul(posL, gBonePalette[boneBase + blendIndices.y]) * blendWeights.y;
    float4 p2 = mul(posL, gBonePalette[boneBase + blendIndices.z]) * blendWeights.z;
    float4 p3 = mul(posL, gBonePalette[boneBase + blendIndices.w]) * blendWeights.w;

    return p0 + p1 + p2 + p3;
}

float3 SkinDirection4(float3 v, uint4 blendIndices, float4 blendWeights, uint boneBase)
{
    float4 dirL = float4(v, 0.0f);

    float3 d0 = mul(dirL, gBonePalette[boneBase + blendIndices.x]).xyz * blendWeights.x;
    float3 d1 = mul(dirL, gBonePalette[boneBase + blendIndices.y]).xyz * blendWeights.y;
    float3 d2 = mul(dirL, gBonePalette[boneBase + blendIndices.z]).xyz * blendWeights.z;
    float3 d3 = mul(dirL, gBonePalette[boneBase + blendIndices.w]).xyz * blendWeights.w;

    return d0 + d1 + d2 + d3;
}

VS_SKINNED_OUTPUT VSSkinned(VS_SKINNED_INPUT input)
{
    VS_SKINNED_OUTPUT output;

    const uint boneBase = 0;

    float4 posSkinned = SkinPosition4(input.position, input.blendIndices, input.blendWeights, boneBase);
    float4 posW = mul(posSkinned, gmtxGameObject);

    output.positionW = posW.xyz;
    output.position = mul(mul(posW, gmtxView), gmtxProjection);
    output.uv = input.uv;

    float3 nSkinned = SkinDirection4(input.normal, input.blendIndices, input.blendWeights, boneBase);
    float3 nW = mul(nSkinned, (float3x3) gmtxGameObject);
    output.normalW = nW;

    float3 tSkinned = SkinDirection4(input.tangent.xyz, input.blendIndices, input.blendWeights, boneBase);
    float3 tW = mul(tSkinned, (float3x3) gmtxGameObject);
    output.tangentW = float4(tW, input.tangent.w);

    output.materialId = gnMaterialID;
    output.shadowPosH = mul(float4(output.positionW, 1.0f), gmtxShadowTransform);

    return output;
}

VS_SKINNED_OUTPUT VSSkinnedInstanced(VS_SKINNED_INSTANCED_INPUT input)
{
    VS_SKINNED_OUTPUT output;

    float4x4 mtxInstanceWorld = float4x4(
        input.instWorld0,
        input.instWorld1,
        input.instWorld2,
        input.instWorld3
    );

    float4 posSkinned = SkinPosition4(
        input.position,
        input.blendIndices,
        input.blendWeights,
        input.instBoneBase
    );

    float4 posW = mul(posSkinned, mtxInstanceWorld);

    output.positionW = posW.xyz;
    output.position = mul(mul(posW, gmtxView), gmtxProjection);
    output.uv = input.uv;

    float3 nSkinned = SkinDirection4(
        input.normal,
        input.blendIndices,
        input.blendWeights,
        input.instBoneBase
    );
    float3 nW = mul(nSkinned, (float3x3) mtxInstanceWorld);
    output.normalW = nW;

    float3 tSkinned = SkinDirection4(
        input.tangent.xyz,
        input.blendIndices,
        input.blendWeights,
        input.instBoneBase
    );
    float3 tW = mul(tSkinned, (float3x3) mtxInstanceWorld);
    output.tangentW = float4(tW, input.tangent.w);

    output.materialId = input.instMaterialId;
    output.shadowPosH = mul(float4(output.positionW, 1.0f), gmtxShadowTransform);
    return output;
}

#endif