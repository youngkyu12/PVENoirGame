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
    if (gvDrawOptions.x == 83)
    {
        if (gvDrawOptions.y == 4)
        {
            float2 pixelPos = input.position.xy;
            float2 viewportSize = gvViewport.xy;
            float2 center = viewportSize * 0.5f;

            float thickness = max(gvUIParams0.x, 1.0f);
            float cornerRadius = thickness * 1.35f;

            float2 innerHalfSize = max(center - thickness, float2(1.0f, 1.0f));
            cornerRadius = min(cornerRadius, min(innerHalfSize.x, innerHalfSize.y));

            float2 q = abs(pixelPos - center) - (innerHalfSize - cornerRadius);
            float outsideDistance = length(max(q, 0.0f)) + min(max(q.x, q.y), 0.0f) - cornerRadius;

            float edge = saturate(outsideDistance / thickness);
            edge = edge * edge * (3.0f - 2.0f * edge);

            return float4(gvUIColor.rgb, gvUIColor.a * edge);
        }

        return gvUIColor;
    }

    uint idx = 0xFFFFFFFFu;

    switch (gvDrawOptions.x)
    {
        case 84:
            idx = gvPostSrvIdx0.x;
            break;
        case 76:
            idx = gvPostSrvIdx0.y;
            break;
        case 78:
            idx = gvPostSrvIdx0.z;
            break;
        case 68:
            idx = gvPostSrvIdx0.w;
            break;
        case 90:
            idx = gvPostSrvIdx1.x;
            break;
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

    float4 color = gtxtGlobalTextures[idx].Sample(gssDefaultSamplerState, input.uv);

    if (gvDrawOptions.y == 1)
    {
        float gray = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
        color.rgb = lerp(color.rgb, gray.xxx, 0.85f);
        color.rgb *= 0.42f;
        color.a *= 0.85f;
    }
    else if (gvDrawOptions.y == 2)
    {
        color.rgb = float3(1.0f, 1.0f, 1.0f);
    }

    return color;
}

#endif