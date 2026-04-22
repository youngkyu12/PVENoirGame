#ifndef __SHADOW_HLSL__
#define __SHADOW_HLSL__

#include "Common.hlsl"
#include "MaterialTexture.hlsl"
#include "Skinned.hlsl"

struct VS_SHADOW_STATIC_INSTANCED_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;

    float4 instWorld0 : INSTANCE_WORLD0;
    float4 instWorld1 : INSTANCE_WORLD1;
    float4 instWorld2 : INSTANCE_WORLD2;
    float4 instWorld3 : INSTANCE_WORLD3;
    uint instObjectId : INSTANCE_OBJECT_ID0;
};

struct VS_SHADOW_SKINNED_INSTANCED_INPUT
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

struct VS_SHADOW_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    nointerpolation uint materialId : MATERIAL_ID;
};

VS_SHADOW_OUTPUT VSShadowMapStaticInstanced(VS_SHADOW_STATIC_INSTANCED_INPUT input)
{
    VS_SHADOW_OUTPUT output;

    float4x4 mtxInstanceWorld = float4x4(
        input.instWorld0,
        input.instWorld1,
        input.instWorld2,
        input.instWorld3
    );

    float4 positionW = mul(float4(input.position, 1.0f), mtxInstanceWorld);
    output.position = mul(positionW, gmtxShadowViewProj);
    output.uv = input.uv;
    output.materialId = gnMaterialID;

    return output;
}

VS_SHADOW_OUTPUT VSShadowMapSkinnedInstanced(VS_SHADOW_SKINNED_INSTANCED_INPUT input)
{
    VS_SHADOW_OUTPUT output;

    float4 skinnedPos = SkinPosition4(
        input.position,
        input.blendIndices,
        input.blendWeights,
        input.instBoneBase
    );

    float4x4 mtxInstanceWorld = float4x4(
        input.instWorld0,
        input.instWorld1,
        input.instWorld2,
        input.instWorld3
    );

    float4 positionW = mul(skinnedPos, mtxInstanceWorld);
    output.position = mul(positionW, gmtxShadowViewProj);
    output.uv = input.uv;
    output.materialId = input.instMaterialId;

    return output;
}

void PSShadowMapOpaque(VS_SHADOW_OUTPUT input)
{
}

void PSShadowMapAlphaClip(VS_SHADOW_OUTPUT input)
{
    MATERIAL mat = gMaterials[input.materialId];

    float2 diffuseUV = GetDiffuseUV(input.materialId, input.uv);

    float4 diffuseSample = SampleTextureRGBA(
        mat.TextureIndices.x,
        diffuseUV,
        float4(1.0f, 1.0f, 1.0f, 1.0f)
    );

    float finalAlpha = diffuseSample.a * mat.m_cDiffuse.a;
    clip(finalAlpha - 0.5f);
}

#endif