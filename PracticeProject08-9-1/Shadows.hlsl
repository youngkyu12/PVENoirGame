//Shadows.hlsl
#ifndef __SHADOWS_HLSL__
#define __SHADOWS_HLSL__

#include "Common.hlsl"
#include "MaterialTexture.hlsl"
#include "RenderTypes.hlsl"
#include "Skinned.hlsl"

VS_SHADOW_MAP_OUTPUT VSShadowMapStaticInstanced(
    VS_TEXTURED_LIGHTING_INSTANCED_INPUT input)
{
    VS_SHADOW_MAP_OUTPUT output;

    float4x4 mtxInstanceWorld = float4x4(
        input.instWorld0,
        input.instWorld1,
        input.instWorld2,
        input.instWorld3
    );

    float3 positionW =
        (float3) mul(float4(input.position, 1.0f), mtxInstanceWorld);

    output.position =
        mul(float4(positionW, 1.0f), gmtxShadowViewProj);

    output.uv = input.uv;

    // static alpha-clip shadow에서는 draw call마다 gnMaterialID를 넣는다.
    output.materialId = gnMaterialID;

    return output;
}

VS_SHADOW_MAP_OUTPUT VSShadowMapSkinnedInstanced(
    VS_SKINNED_INSTANCED_INPUT input)
{
    VS_SHADOW_MAP_OUTPUT output;

    float4x4 mtxInstanceWorld = float4x4(
        input.instWorld0,
        input.instWorld1,
        input.instWorld2,
        input.instWorld3
    );

    float4 posSkinned =
        SkinPosition4(
            input.position,
            input.blendIndices,
            input.blendWeights,
            input.instBoneBase
        );

    float4 positionW = mul(posSkinned, mtxInstanceWorld);

    output.position =
        mul(positionW, gmtxShadowViewProj);

    output.uv = input.uv;
    output.materialId = input.instMaterialId;

    return output;
}

void PSShadowMapAlphaClip(VS_SHADOW_MAP_OUTPUT input)
{
    MATERIAL mat = gMaterials[input.materialId];

    float2 diffuseUV = GetDiffuseUVFromMaterial(mat, input.uv);

    float4 diffuseSample =
        SampleTextureRGBA(
            mat.TextureIndices.x,
            diffuseUV,
            float4(1.0f, 1.0f, 1.0f, 1.0f)
        );

    float alpha =
        diffuseSample.a * mat.m_cDiffuse.a;

    clip(alpha - 0.5f);
}

#endif