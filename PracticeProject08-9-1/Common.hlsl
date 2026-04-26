#ifndef __COMMON_HLSL__
#define __COMMON_HLSL__

#define MAX_GLOBAL_SRVS 8192
#define MAX_LIGHTS      4
#define MAX_MATERIALS   256

#define POINT_LIGHT         1
#define SPOT_LIGHT          2
#define DIRECTIONAL_LIGHT   3

#define _WITH_LOCAL_VIEWER_HIGHLIGHTING
#define _WITH_THETA_PHI_CONES
//#define _WITH_REFLECT

Texture2D gtxtGlobalTextures[MAX_GLOBAL_SRVS] : register(t0);

cbuffer cbPlayerInfo : register(b0)
{
    matrix gmtxPlayerWorld;
};

cbuffer cbCameraInfo : register(b1)
{
    matrix gmtxView : packoffset(c0);
    matrix gmtxProjection : packoffset(c4);
    float3 gvCameraPosition : packoffset(c8);
};

cbuffer cbGameObjectInfo : register(b2)
{
    float4x4 gmtxGameObject;
    uint gnObjectID;
    uint3 _padObj;
};

SamplerState gssDefaultSamplerState : register(s0);
SamplerComparisonState gssShadowSampler : register(s1);

cbuffer cbDrawOptions : register(b5)
{
    int4 gvDrawOptions; // x = 'T','L','N','D','Z'
    uint4 gvPostSrvIdx0; // x=T, y=L, z=N, w=D
    uint4 gvPostSrvIdx1; // x=Z, 나머지 패딩

    // UI rectangle in pixels
    // x = centerX, y = centerY, z = width, w = height
    float4 gvUiRect;

    // viewport info
    // x = viewportWidth, y = viewportHeight
    // z = 1/viewportWidth, w = 1/viewportHeight
    float4 gvViewport;
};

cbuffer cbPerDrawMaterialId : register(b6)
{
    uint gnMaterialID;
};

cbuffer cbFog : register(b7)
{
    float4 gvFogColor;

    // x = fogStart
    // y = fogEnd
    // z = fogDensity
    // w = fogEnable
    float4 gvFogParams0;

    // x = cameraNear
    // y = cameraFar
    // z = fogMode (0=Linear, 1=Exp, 2=Exp2)
    // w = reserved
    float4 gvFogParams1;
};

cbuffer cbShadow : register(b8)
{
    matrix gmtxShadowViewProj;
    matrix gmtxShadowTransform;

    float4 gvShadowLightPos;
    float4 gvShadowParams0;
    uint4 gvShadowParams1;
};

StructuredBuffer<float4x4> gBonePalette : register(t0, space1);

static const uint INVALID_TEXTURE_INDEX = 0xffffffffu;

#endif