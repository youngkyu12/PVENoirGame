#ifndef __WATER_HLSL__
#define __WATER_HLSL__

#include "Common.hlsl"
#include "RenderTypes.hlsl"

cbuffer cbWater : register(b10)
{
    // x = current time, y = water height, z = base uv scale, w = alpha
    float4 gvWaterParams;

    // x = base texture, y = detail0 texture, z = detail1 texture, w = unused
    uint4 gvWaterTextureIndices;

    // xy = base flow direction/speed, zw = detail0 flow direction/speed
    float4 gvWaterFlowParams;

    // xy = detail1 flow direction/speed, z = detail0 uv scale, w = detail1 uv scale
    float4 gvWaterDetailParams;

    float4x4 gf4x4TextureAnimation;
};

struct VS_WATER_OUTPUT
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 SampleWaterTexture(uint textureIndex, float2 uv, float4 fallback)
{
    if (textureIndex >= MAX_GLOBAL_SRVS)
        return fallback;

    return gtxtGlobalTextures[textureIndex].SampleLevel(gssDefaultSamplerState, uv, 0);
}

float ComputeTerrainEdgeWaterAlpha(float2 positionXZ)
{
    const float2 fadeCenter = float2(0.0f, 400.0f);
    const float opaqueInnerHalfExtent = 500.0f;
    const float translucentHalfExtent = 590.0f;
    const float outerHalfExtent = 620.0f;
    const float translucentAlpha = 0.9f;
    const float opaqueAlpha = 1.0f;

    float2 centeredXZ = abs(positionXZ - fadeCenter);
    float squareDistance = max(centeredXZ.x, centeredXZ.y);

    if (squareDistance <= opaqueInnerHalfExtent)
        return opaqueAlpha;

    if (squareDistance <= translucentHalfExtent)
    {
        float innerBlend = smoothstep(
            opaqueInnerHalfExtent,
            translucentHalfExtent,
            squareDistance
        );

        return lerp(opaqueAlpha, translucentAlpha, innerBlend);
    }

    float edgeBlend = smoothstep(translucentHalfExtent, outerHalfExtent, squareDistance);
    return lerp(translucentAlpha, opaqueAlpha, edgeBlend);
}

VS_WATER_OUTPUT VSWaterInstanced(VS_TEXTURED_LIGHTING_INSTANCED_INPUT input)
{
    VS_WATER_OUTPUT output;

    float4x4 mtxInstanceWorld = float4x4(
        input.instWorld0,
        input.instWorld1,
        input.instWorld2,
        input.instWorld3
    );

    float4 positionW = mul(float4(input.position, 1.0f), mtxInstanceWorld);
    positionW.y += gvWaterParams.y;

    output.positionW = positionW.xyz;
    output.normalW = normalize(mul(input.normal, (float3x3)mtxInstanceWorld));
    output.position = mul(mul(positionW, gmtxView), gmtxProjection);
    output.uv = input.uv;
    output.color = float4(1.0f, 1.0f, 1.0f, gvWaterParams.w);

    return output;
}

PS_MULTIPLE_RENDER_TARGETS_OUTPUT PSWaterToMultipleRTs(VS_WATER_OUTPUT input)
{
    PS_MULTIPLE_RENDER_TARGETS_OUTPUT output;

    const float time = gvWaterParams.x;
    const float baseUvScale = gvWaterParams.z;
    const float alpha = gvWaterParams.w;

    float2 uv = input.uv * baseUvScale;
    uv = mul(float3(uv, 1.0f), (float3x3)gf4x4TextureAnimation).xy;

    const float2 baseUV = uv + gvWaterFlowParams.xy * time;
    const float2 detail0UV =
        uv * gvWaterDetailParams.z + gvWaterFlowParams.zw * time;
    const float2 detail1UV =
        uv * gvWaterDetailParams.w + gvWaterDetailParams.xy * time;

    float4 baseTexColor = SampleWaterTexture(
        gvWaterTextureIndices.x,
        baseUV,
        float4(0.05f, 0.20f, 0.28f, 1.0f)
    );

    float4 detail0TexColor = SampleWaterTexture(
        gvWaterTextureIndices.y,
        detail0UV,
        float4(1.0f, 1.0f, 1.0f, 1.0f)
    );

    float4 detail1TexColor = SampleWaterTexture(
        gvWaterTextureIndices.z,
        detail1UV,
        float4(0.5f, 0.5f, 0.5f, 1.0f)
    );

    float4 waterColor = lerp(
        baseTexColor * detail0TexColor,
        detail1TexColor.rrrr * 0.5f,
        0.35f
    );

    waterColor.a = saturate(alpha * ComputeTerrainEdgeWaterAlpha(input.positionW.xz));

    float3 normalW = normalize(input.normalW);

    output.color = waterColor;
    output.cTexture = waterColor;
    output.cIllumination = waterColor;
    output.normal = float4(normalW * 0.5f + 0.5f, 1.0f);
    output.zDepth = input.position.z;

    return output;
}

float4 PSWaterForward(VS_WATER_OUTPUT input) : SV_TARGET
{
    const float time = gvWaterParams.x;
    const float baseUvScale = gvWaterParams.z;
    const float alpha = gvWaterParams.w;

    float2 uv = input.uv * baseUvScale;
    uv = mul(float3(uv, 1.0f), (float3x3)gf4x4TextureAnimation).xy;

    const float2 baseUV = uv + gvWaterFlowParams.xy * time;
    const float2 detail0UV =
        uv * gvWaterDetailParams.z + gvWaterFlowParams.zw * time;
    const float2 detail1UV =
        uv * gvWaterDetailParams.w + gvWaterDetailParams.xy * time;

    float4 baseTexColor = SampleWaterTexture(
        gvWaterTextureIndices.x,
        baseUV,
        float4(0.05f, 0.20f, 0.28f, 1.0f)
    );

    float4 detail0TexColor = SampleWaterTexture(
        gvWaterTextureIndices.y,
        detail0UV,
        float4(1.0f, 1.0f, 1.0f, 1.0f)
    );

    float4 detail1TexColor = SampleWaterTexture(
        gvWaterTextureIndices.z,
        detail1UV,
        float4(0.5f, 0.5f, 0.5f, 1.0f)
    );

    float4 waterColor = lerp(
        baseTexColor * detail0TexColor,
        detail1TexColor.rrrr * 0.5f,
        0.35f
    );

    waterColor.a = saturate(alpha * ComputeTerrainEdgeWaterAlpha(input.positionW.xz));

    return waterColor;
}

#endif
