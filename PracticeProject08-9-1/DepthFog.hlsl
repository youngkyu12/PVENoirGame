//DepthFog.hlsl
#ifndef __DEPTH_FOG_HLSL__
#define __DEPTH_FOG_HLSL__

#include "Common.hlsl"
#include "PostProcess.hlsl"

float ResolveLinearDepthFromDeviceZ(float deviceZ)
{
    const float nearZ = max(0.0001f, gvFogParams1.x);
    const float farZ = max(nearZ + 0.0001f, gvFogParams1.y);

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

float4 PSDepthFog(VS_SCREEN_RECT_TEXTURED_OUTPUT input) : SV_Target
{
    const uint sceneColorIdx = gvPostSrvIdx0.x;
    const uint sceneDepthIdx = gvPostSrvIdx0.y;
    const uint ambientOcclusionIdx = gvPostSrvIdx0.z;

    if (sceneColorIdx == 0xFFFFFFFFu || sceneColorIdx >= MAX_GLOBAL_SRVS)
        return float4(1, 0, 1, 1);

    if (sceneDepthIdx == 0xFFFFFFFFu || sceneDepthIdx >= MAX_GLOBAL_SRVS)
        return float4(1, 0, 1, 1);

    float4 sceneColor =
        gtxtGlobalTextures[sceneColorIdx].Sample(gssDefaultSamplerState, input.uv);

    if (ambientOcclusionIdx != 0xFFFFFFFFu && ambientOcclusionIdx < MAX_GLOBAL_SRVS)
    {
        const float ambientAccess =
            gtxtGlobalTextures[ambientOcclusionIdx].Sample(gssDefaultSamplerState, input.uv).r;
        sceneColor.rgb *= saturate(ambientAccess);
    }

    const float deviceZ =
        gtxtGlobalTextures[sceneDepthIdx].Load(
            uint3((uint) input.position.x, (uint) input.position.y, 0)
        ).x;

    const float linearDepth = ResolveLinearDepthFromDeviceZ(deviceZ);
    const float fogFactor = ComputeFogFactor(linearDepth);

    const float3 rgb = lerp(sceneColor.rgb, gvFogColor.rgb, fogFactor);
    return float4(rgb, sceneColor.a);
}

#endif
