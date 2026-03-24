//-----------------------------------------------------------------------------
// File: Shaders.hlsl
//-----------------------------------------------------------------------------
#define MAX_GLOBAL_SRVS 8192

// Global Texture2D pool: t0 ~ t8191 in space0
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

    // UI rectangle in pixels
    // x = centerX, y = centerY, z = width, w = height
    float4 gvUiRect;

    // viewport info
    // x = viewportWidth, y = viewportHeight
    // z = 1/viewportWidth, w = 1/viewportHeight
    float4 gvViewport;
};

#define MAX_BONES 256

cbuffer cbBonePalette : register(b7)
{
    float4x4 gBoneTransforms[MAX_BONES];
};

#include "Light.hlsl"

static const uint INVALID_TEXTURE_INDEX = 0xffffffffu;

uint DecodePackedTextureIndex(uint packedIndex)
{
    return (packedIndex == 0u) ? INVALID_TEXTURE_INDEX : (packedIndex - 1u);
}

float ApplyWrap1D(float value, uint wrapMode)
{
    return (wrapMode == 1u) ? saturate(value) : frac(value);
}

float2 ApplyWrap2D(float2 uv, uint wrapModeU, uint wrapModeV)
{
    return float2(
        ApplyWrap1D(uv.x, wrapModeU),
        ApplyWrap1D(uv.y, wrapModeV)
    );
}

float2 ApplyUVST(float2 uv, float4 uvST)
{
    return uv * uvST.xy + uvST.zw;
}

float2 GetDiffuseUV(float2 baseUV)
{
    MATERIAL mat = gMaterials[gnMaterialID];
    return ApplyWrap2D(
        ApplyUVST(baseUV, mat.DiffuseUVST),
        mat.WrapModes0.x,
        mat.WrapModes0.y
    );
}

float2 GetNormalUV(float2 baseUV)
{
    MATERIAL mat = gMaterials[gnMaterialID];
    return ApplyWrap2D(
        ApplyUVST(baseUV, mat.NormalUVST),
        mat.WrapModes0.z,
        mat.WrapModes0.w
    );
}

float2 GetEmissiveUV(float2 baseUV)
{
    MATERIAL mat = gMaterials[gnMaterialID];
    return ApplyWrap2D(
        ApplyUVST(baseUV, mat.EmissiveUVST),
        mat.WrapModes1.x,
        mat.WrapModes1.y
    );
}

float2 GetSpecularUV(float2 baseUV)
{
    MATERIAL mat = gMaterials[gnMaterialID];
    return ApplyWrap2D(
        ApplyUVST(baseUV, mat.SpecularUVST),
        mat.WrapModes1.z,
        mat.WrapModes1.w
    );
}

float4 SampleTextureRGBA(uint packedIndex, float2 uv, float4 fallbackColor)
{
    uint textureIndex = DecodePackedTextureIndex(packedIndex);

    if (textureIndex == INVALID_TEXTURE_INDEX)
        return fallbackColor;

    if (textureIndex >= MAX_GLOBAL_SRVS)
        return fallbackColor;

    return gtxtGlobalTextures[textureIndex].Sample(gssDefaultSamplerState, uv);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float3 GetNormalWFromMap(uint packedNormal, float3 normalW_in, float4 tangentW_in, float2 uv)
{
    float3 N = normalize(normalW_in);

    uint normalIndex = DecodePackedTextureIndex(packedNormal);
    if (normalIndex == INVALID_TEXTURE_INDEX)
        return N;

    if (normalIndex >= MAX_GLOBAL_SRVS)
        return N;

    float3 nTS = gtxtGlobalTextures[normalIndex].Sample(gssDefaultSamplerState, uv).xyz;
    nTS = nTS * 2.0f - 1.0f;

    float3 T = normalize(tangentW_in.xyz);
    T = normalize(T - N * dot(T, N));

    float3 B = normalize(cross(N, T) * tangentW_in.w);

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
    MATERIAL mat = gMaterials[gnMaterialID];

    float2 diffuseUV = GetDiffuseUV(input.uv);

    float4 diffuseSample = SampleTextureRGBA(
        mat.TextureIndices.x,
        diffuseUV,
        float4(1.0f, 1.0f, 1.0f, 1.0f)
    );

    return diffuseSample * mat.m_cDiffuse;
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

    MATERIAL mat = gMaterials[gnMaterialID];

    float2 diffuseUV = GetDiffuseUV(input.uv);
    float2 normalUV = GetNormalUV(input.uv);
    float2 emissiveUV = GetEmissiveUV(input.uv);
    float2 specularUV = GetSpecularUV(input.uv);

    float4 diffuseSample = SampleTextureRGBA(
        mat.TextureIndices.x,
        diffuseUV,
        float4(1.0f, 1.0f, 1.0f, 1.0f)
    );

    float4 emissiveSample = SampleTextureRGBA(
        mat.TextureIndices.z,
        emissiveUV,
        float4(1.0f, 1.0f, 1.0f, 1.0f)
    );
    float4 specularSample = SampleTextureRGBA(
        mat.TextureIndices.w,
        specularUV,
        float4(1.0f, 1.0f, 1.0f, 1.0f)
    );

    float4 texColor = diffuseSample * mat.m_cDiffuse;
    float3 normalW = GetNormalWFromMap(mat.TextureIndices.y, input.normalW, input.tangentW, normalUV);
    float3 emissiveColor = emissiveSample.rgb * mat.m_cEmissive.rgb;

    float3 specularColor = specularSample.rgb * mat.m_cSpecular.rgb;
    float shininess = mat.m_cSpecular.a;

    float4 illumination = Lighting(
        input.positionW,
        normalW,
        texColor,
        emissiveColor,
        specularColor,
        shininess
    );
    
    output.cTexture = texColor;
    output.cIllumination = illumination;
    //output.color = illumination;
    output.color = texColor;
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

    // 픽셀 단위 사각형 좌표
    float2 pixelPos = centerPx + kLocalPos[nVertexID] * sizePx;

    VS_SCREEN_RECT_TEXTURED_OUTPUT output = (VS_SCREEN_RECT_TEXTURED_OUTPUT) 0;

    // pixel -> NDC
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
