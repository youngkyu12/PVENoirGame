//PostProcess.hlsl
#ifndef __POST_PROCESS_HLSL__
#define __POST_PROCESS_HLSL__

#include "Common.hlsl"
#include "MaterialTexture.hlsl"

float4 VSPostProcessing(uint nVertexID : SV_VertexID) : SV_POSITION
{
    static const float4 kPos[6] =
    {
        float4(-1.0f, +1.0f, 0.0f, 1.0f),
        float4(+1.0f, +1.0f, 0.0f, 1.0f),
        float4(+1.0f, -1.0f, 0.0f, 1.0f),

        float4(-1.0f, +1.0f, 0.0f, 1.0f),
        float4(+1.0f, -1.0f, 0.0f, 1.0f),
        float4(-1.0f, -1.0f, 0.0f, 1.0f)
    };

    return (nVertexID < 6) ? kPos[nVertexID] : float4(0, 0, 0, 0);
}

float4 PSPostProcessing(float4 position : SV_POSITION) : SV_Target
{
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}

struct VS_SCREEN_RECT_TEXTURED_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

VS_SCREEN_RECT_TEXTURED_OUTPUT VSScreenRectSamplingTextured(uint nVertexID : SV_VertexID)
{
    static const float2 kLocalPos[6] =
    {
        float2(-0.5f, -0.5f),
        float2(-0.5f, +0.5f),
        float2(+0.5f, -0.5f),

        float2(+0.5f, -0.5f),
        float2(-0.5f, +0.5f),
        float2(+0.5f, +0.5f)
    };

    static const float2 kUV[6] =
    {
        float2(0.0f, 0.0f),
        float2(0.0f, 1.0f),
        float2(1.0f, 0.0f),

        float2(1.0f, 0.0f),
        float2(0.0f, 1.0f),
        float2(1.0f, 1.0f)
    };

    float2 centerPx = gvUiRect.xy;
    float2 sizePx = gvUiRect.zw;
    float2 invVp = gvViewport.zw;

    float2 pixelPos = centerPx + kLocalPos[nVertexID] * sizePx;

    VS_SCREEN_RECT_TEXTURED_OUTPUT output = (VS_SCREEN_RECT_TEXTURED_OUTPUT) 0;

    output.position = float4(
        pixelPos.x * invVp.x * 2.0f - 1.0f,
        1.0f - pixelPos.y * invVp.y * 2.0f,
        0.0f,
        1.0f
    );

    output.uv = kUV[nVertexID];
    return output;
}

float4 PSScreenRectSamplingTextured(VS_SCREEN_RECT_TEXTURED_OUTPUT input) : SV_Target
{
    uint idx = 0xFFFFFFFFu;

    switch (gvDrawOptions.x)
    {
        case 84:
            idx = gvPostSrvIdx0.x;
            break; // 'T'
        case 76:
            idx = gvPostSrvIdx0.y;
            break; // 'L'
        case 78:
            idx = gvPostSrvIdx0.z;
            break; // 'N'
        case 68:
            idx = gvPostSrvIdx0.w;
            break; // 'D'
        case 90:
            idx = gvPostSrvIdx1.x;
            break; // 'Z'
        default:
            return float4(0, 0, 0, 1);
    }

    if (idx == 0xFFFFFFFFu || idx >= MAX_GLOBAL_SRVS)
        return float4(1, 0, 1, 1);

    if (gvDrawOptions.x == 68 || gvDrawOptions.x == 90)
    {
        float d = gtxtGlobalTextures[idx].Load(uint3((uint) input.position.x, (uint) input.position.y, 0)).x;
        return float4(d, d, d, 1);
    }

    return gtxtGlobalTextures[idx].Sample(gssDefaultSamplerState, input.uv);
}

#endif