//Occlusion.hlsl
#ifndef __OCCLUSION_HLSL__
#define __OCCLUSION_HLSL__

#include "Common.hlsl"
#include "RenderTypes.hlsl"

struct VS_OCCLUSION_STATIC_OUTPUT
{
    float4 position : SV_POSITION;
};

VS_OCCLUSION_STATIC_OUTPUT VSStaticOcclusionInstanced(
    VS_TEXTURED_LIGHTING_INSTANCED_INPUT input)
{
    VS_OCCLUSION_STATIC_OUTPUT output;

    float4x4 mtxInstanceWorld = float4x4(
        input.instWorld0,
        input.instWorld1,
        input.instWorld2,
        input.instWorld3
    );

    float3 positionW =
        (float3) mul(float4(input.position, 1.0f), mtxInstanceWorld);

    output.position =
        mul(mul(float4(positionW, 1.0f), gmtxView), gmtxProjection);

    return output;
}

float4 PSOcclusionOpaque(VS_OCCLUSION_STATIC_OUTPUT input) : SV_TARGET
{
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

#endif