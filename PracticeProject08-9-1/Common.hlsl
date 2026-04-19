//Common.HLSL
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
static const uint SHADOW_MAP_SRV_INDEX = 2u;

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

SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbDrawOptions : register(b5)
{
    int4 gvDrawOptions; // x = 'T','L','N','D','Z'
    uint4 gvPostSrvIdx0; // x=T, y=L, z=N, w=D
    uint4 gvPostSrvIdx1; // x=Z, 나머지 패딩

    float4 gvUiRect; // x=centerX, y=centerY, z=width, w=height
    float4 gvViewport; // x=viewportWidth, y=viewportHeight, z=1/w, w=1/h
};

cbuffer cbPerDrawMaterialId : register(b6)
{
    uint gnMaterialID;
};

cbuffer cbShadowPass : register(b7)
{
    float4x4 gLightViewProj;
    float4x4 gShadowTransform;
    float3 gLightDirectionW;
    float _padShadow0;
};

StructuredBuffer<float4x4> gBonePalette : register(t0, space1);

static const uint INVALID_TEXTURE_INDEX = 0xffffffffu;

#endif