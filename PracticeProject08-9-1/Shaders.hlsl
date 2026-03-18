//-----------------------------------------------------------------------------
// File: Shaders.hlsl
//-----------------------------------------------------------------------------
#define MAX_GLOBAL_SRVS 1024

// Global Texture2D pool: t0 ~ t1023 in space0
Texture2D gtxtGlobalTextures[MAX_GLOBAL_SRVS] : register(t0);

cbuffer cbPlayerInfo : register(b0)
{
    matrix gmtxPlayerWorld;
};

cbuffer cbGameObjectInfo : register(b2)
{
    float4x4 gmtxGameObject;
    uint gnObjectID;
    uint3 _padObj;
};


SamplerState gssDefaultSamplerState : register(s0);

cbuffer cbDrawOptions : register(b5)
{
    int4 gvDrawOptions; // x = 'T','L','N','D','Z'
    uint4 gvPostSrvIdx0; // x=T, y=L, z=N, w=D
    uint4 gvPostSrvIdx1; // x=Z, 나머지 패딩
};

#define MAX_BONES 256

cbuffer cbBonePalette : register(b7)
{
    float4x4 gBoneTransforms[MAX_BONES];
};

#include "Light.hlsl"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float3 GetNormalWFromMap(uint packedNormal, float3 normalW_in, float4 tangentW_in, float2 uv)
{
    float3 N = normalize(normalW_in);

    if (packedNormal == 0)
        return N;

    uint normalIndex = packedNormal - 1;
    if (normalIndex >= MAX_GLOBAL_SRVS)
        return N;

    float3 nTS = gtxtGlobalTextures[normalIndex].Sample(gssDefaultSamplerState, uv).xyz;
    nTS = nTS * 2.0f - 1.0f;
    //nTS.y = -nTS.y;

    float3 T = normalize(tangentW_in.xyz);
    T = normalize(T - N * dot(T, N));

    float3 B = normalize(cross(N, T) * tangentW_in.w);
    //float3 B = normalize(cross(T, N) * tangentW_in.w);

    float3 nW = normalize(T * nTS.x + B * nTS.y + N * nTS.z);
    return nW;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Textured (VS/PS)
struct VS_TEXTURED_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
};

struct VS_TEXTURED_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

VS_TEXTURED_OUTPUT VSTextured(VS_TEXTURED_INPUT input)
{
    VS_TEXTURED_OUTPUT output;

    output.position = mul(mul(mul(float4(input.position, 1.0f), gmtxGameObject), gmtxView), gmtxProjection);
    output.uv = input.uv;

    return (output);
}

float4 PSTextured(VS_TEXTURED_OUTPUT input, uint nPrimitiveID : SV_PrimitiveID) : SV_TARGET
{
    uint packed = gMaterials[gnMaterialID].TextureIndices.x;

    if (packed == 0)
        return float4(1, 0, 1, 1);

    uint diffuseIndex = packed - 1;

    if (diffuseIndex >= MAX_GLOBAL_SRVS)
        return float4(1, 0, 0, 1);

    return gtxtGlobalTextures[diffuseIndex]
            .Sample(gssDefaultSamplerState, input.uv);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Textured + Lighting (VS) + MRT PS
struct VS_TEXTURED_LIGHTING_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
};

struct VS_TEXTURED_LIGHTING_OUTPUT
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangentW : TANGENT;
};

VS_TEXTURED_LIGHTING_OUTPUT VSTexturedLighting(VS_TEXTURED_LIGHTING_INPUT input)
{
    VS_TEXTURED_LIGHTING_OUTPUT output;

    output.normalW = mul(input.normal, (float3x3) gmtxGameObject);
    output.positionW = (float3) mul(float4(input.position, 1.0f), gmtxGameObject);
    output.position = mul(mul(float4(output.positionW, 1.0f), gmtxView), gmtxProjection);
    output.uv = input.uv;
    float3 tW = mul(input.tangent.xyz, (float3x3) gmtxGameObject);
    output.tangentW = float4(tW, input.tangent.w);

    return (output);
}

struct PS_MULTIPLE_RENDER_TARGETS_OUTPUT
{
    float4 color : SV_TARGET0;
    float4 cTexture : SV_TARGET1;
    float4 cIllumination : SV_TARGET2;
    float4 normal : SV_TARGET3;
    float zDepth : SV_TARGET4;
};

PS_MULTIPLE_RENDER_TARGETS_OUTPUT PSTexturedLightingToMultipleRTs(
    VS_TEXTURED_LIGHTING_OUTPUT input,
    uint nPrimitiveID : SV_PrimitiveID)
{
    PS_MULTIPLE_RENDER_TARGETS_OUTPUT output;

    uint packed = gMaterials[gnMaterialID].TextureIndices.x;

    if (packed == 0)
    {
        output.color = float4(1, 0, 1, 1);
        output.cTexture = output.color;
        output.cIllumination = float4(0, 0, 0, 0);
        output.normal = float4(0, 0, 1, 1);
        output.zDepth = input.position.z;
        return output;
    }

    uint diffuseIndex = packed - 1;
    
    if (diffuseIndex >= MAX_GLOBAL_SRVS)
    {
        output.color = float4(1, 0, 0, 1);
        output.cTexture = output.color;
        output.cIllumination = float4(0, 0, 0, 0);
        output.normal = float4(0, 0, 1, 1);
        output.zDepth = input.position.z;
        return output;
    }

    float4 texColor =
        gtxtGlobalTextures[diffuseIndex]
            .Sample(gssDefaultSamplerState, input.uv);

    uint packedN = gMaterials[gnMaterialID].TextureIndices.y;
    float3 normalW = GetNormalWFromMap(packedN, input.normalW, input.tangentW, input.uv);
    float4 illumination = Lighting(input.positionW, normalW);

    output.cTexture = texColor;
    output.cIllumination = illumination;
    output.color = texColor;
    //output.color = texColor * illumination;
    //output.color = float4(normalW * 0.5f + 0.5f, 1.0f);
    output.normal = float4(normalW * 0.5f + 0.5f, 1.0f);
    output.zDepth = input.position.z;

    return output;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Skinned (VS only; PS는 위 MRT PS를 재사용)
struct VS_SKINNED_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
    uint4 blendIndices : BLENDINDICES;
    float4 blendWeights : BLENDWEIGHT;
};

struct VS_SKINNED_OUTPUT
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangentW : TANGENT;
};

VS_SKINNED_OUTPUT VSSkinned(VS_SKINNED_INPUT input)
{
    VS_SKINNED_OUTPUT output;

    uint bi = input.blendIndices.x;

    float4 posL = float4(input.position, 1.0f);
    float4 posSkinned = mul(posL, gBoneTransforms[bi]);
    float4 posW = mul(posSkinned, gmtxGameObject);

    output.positionW = posW.xyz;
    output.position = mul(mul(posW, gmtxView), gmtxProjection);
    output.uv = input.uv;

    float3 nSkinned = mul(float4(input.normal, 0.0f), gBoneTransforms[bi]).xyz;
    float3 nW = mul(nSkinned, (float3x3) gmtxGameObject);
    output.normalW = nW;

    float3 tSkinned = mul(float4(input.tangent.xyz, 0.0f), gBoneTransforms[bi]).xyz;
    float3 tW = mul(tSkinned, (float3x3) gmtxGameObject);
    output.tangentW = float4(tW, input.tangent.w);

    return output;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// PostProcessing (VS/PS)
float4 VSPostProcessing(uint nVertexID : SV_VertexID) : SV_POSITION
{
    if (nVertexID == 0)
        return (float4(-1.0f, +1.0f, 0.0f, 1.0f));
    if (nVertexID == 1)
        return (float4(+1.0f, +1.0f, 0.0f, 1.0f));
    if (nVertexID == 2)
        return (float4(+1.0f, -1.0f, 0.0f, 1.0f));

    if (nVertexID == 3)
        return (float4(-1.0f, +1.0f, 0.0f, 1.0f));
    if (nVertexID == 4)
        return (float4(+1.0f, -1.0f, 0.0f, 1.0f));
    if (nVertexID == 5)
        return (float4(-1.0f, -1.0f, 0.0f, 1.0f));

    return (float4(0, 0, 0, 0));
}

float4 PSPostProcessing(float4 position : SV_POSITION) : SV_Target
{
    return (float4(0.0f, 0.0f, 0.0f, 1.0f));
}

///////////////////////////////////////////////////////////////////////////////
// Fullscreen sampling (VS/PS)
struct VS_SCREEN_RECT_TEXTURED_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

VS_SCREEN_RECT_TEXTURED_OUTPUT VSScreenRectSamplingTextured(uint nVertexID : SV_VertexID)
{
    VS_SCREEN_RECT_TEXTURED_OUTPUT output = (VS_TEXTURED_OUTPUT) 0;

    if (nVertexID == 0)
    {
        output.position = float4(-1.0f, +1.0f, 0.0f, 1.0f);
        output.uv = float2(0.0f, 0.0f);
    }
    else if (nVertexID == 1)
    {
        output.position = float4(+1.0f, +1.0f, 0.0f, 1.0f);
        output.uv = float2(1.0f, 0.0f);
    }
    else if (nVertexID == 2)
    {
        output.position = float4(+1.0f, -1.0f, 0.0f, 1.0f);
        output.uv = float2(1.0f, 1.0f);
    }
    else if (nVertexID == 3)
    {
        output.position = float4(-1.0f, +1.0f, 0.0f, 1.0f);
        output.uv = float2(0.0f, 0.0f);
    }
    else if (nVertexID == 4)
    {
        output.position = float4(+1.0f, -1.0f, 0.0f, 1.0f);
        output.uv = float2(1.0f, 1.0f);
    }
    else if (nVertexID == 5)
    {
        output.position = float4(-1.0f, -1.0f, 0.0f, 1.0f);
        output.uv = float2(0.0f, 1.0f);
    }

    return (output);
}

float4 PSScreenRectSamplingTextured(VS_TEXTURED_OUTPUT input) : SV_Target
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