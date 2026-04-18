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

StructuredBuffer<float4x4> gBonePalette : register(t0, space1);

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

float2 GetDiffuseUV(uint materialId, float2 baseUV)
{
    MATERIAL mat = gMaterials[materialId];
    return ApplyWrap2D(
        ApplyUVST(baseUV, mat.DiffuseUVST),
        mat.WrapModes0.x,
        mat.WrapModes0.y
    );
}

float2 GetNormalUV(uint materialId, float2 baseUV)
{
    MATERIAL mat = gMaterials[materialId];
    return ApplyWrap2D(
        ApplyUVST(baseUV, mat.NormalUVST),
        mat.WrapModes0.z,
        mat.WrapModes0.w
    );
}

float2 GetEmissiveUV(uint materialId, float2 baseUV)
{
    MATERIAL mat = gMaterials[materialId];
    return ApplyWrap2D(
        ApplyUVST(baseUV, mat.EmissiveUVST),
        mat.WrapModes1.x,
        mat.WrapModes1.y
    );
}

float2 GetSpecularUV(uint materialId, float2 baseUV)
{
    MATERIAL mat = gMaterials[materialId];
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

 //정점 셰이더의 입력을 위한 구조체를 선언한다.
struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
};

 //정점 셰이더의 출력(픽셀 셰이더의 입력)을 위한 구조체를 선언한다.
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

 //정점 셰이더를 정의한다.
VS_OUTPUT VSDiffused(VS_INPUT input)
{
    VS_OUTPUT output;
    
    //정점을 변환(월드 변환, 카메라 변환, 투영 변환)한다.
    output.position = mul(mul(mul(float4(input.position, 1.0f), gmtxGameObject), gmtxView), gmtxProjection);
    
    //입력되는 픽셀의 색상(래스터라이저 단계에서 보간하여 얻은 색상)을 그대로 출력한다. 
    output.color = input.color;
    
    return (output);
}

 //픽셀 셰이더를 정의한다.
float4 PSDiffused(VS_OUTPUT input) : SV_TARGET
{
 //입력되는 픽셀의 색상을 그대로 출력-병합 단계(렌더 타겟)로 출력한다. 
    return (input.color);
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

    float2 diffuseUV = GetDiffuseUV(gnMaterialID, input.uv);

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

struct VS_TEXTURED_LIGHTING_INSTANCED_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;

    float4 instWorld0 : INSTANCE_WORLD0;
    float4 instWorld1 : INSTANCE_WORLD1;
    float4 instWorld2 : INSTANCE_WORLD2;
    float4 instWorld3 : INSTANCE_WORLD3;
    uint instObjectId : INSTANCE_OBJECT_ID0;
};

struct VS_TEXTURED_LIGHTING_OUTPUT
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangentW : TANGENT;
    nointerpolation uint materialId : MATERIAL_ID;
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
    output.materialId = gnMaterialID;
    
    return (output);
}

VS_TEXTURED_LIGHTING_OUTPUT VSTexturedLightingInstanced(VS_TEXTURED_LIGHTING_INSTANCED_INPUT input)
{
    VS_TEXTURED_LIGHTING_OUTPUT output;

    float4x4 mtxInstanceWorld = float4x4(
        input.instWorld0,
        input.instWorld1,
        input.instWorld2,
        input.instWorld3
    );

    output.normalW = mul(input.normal, (float3x3) mtxInstanceWorld);
    output.positionW = (float3) mul(float4(input.position, 1.0f), mtxInstanceWorld);
    output.position = mul(mul(float4(output.positionW, 1.0f), gmtxView), gmtxProjection);
    output.uv = input.uv;

    float3 tW = mul(input.tangent.xyz, (float3x3) mtxInstanceWorld);
    output.tangentW = float4(tW, input.tangent.w);
    output.materialId = gnMaterialID;
    
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

    uint materialId = input.materialId;
    MATERIAL mat = gMaterials[materialId];

    float2 diffuseUV = GetDiffuseUV(materialId, input.uv);
    float2 normalUV = GetNormalUV(materialId, input.uv);
    float2 emissiveUV = GetEmissiveUV(materialId, input.uv);
    float2 specularUV = GetSpecularUV(materialId, input.uv);

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
        materialId,
        input.positionW,
        normalW,
        texColor,
        emissiveColor,
        specularColor,
        shininess
    );
    
    output.cTexture = texColor;
    output.cIllumination = illumination;
    output.color = illumination;
    //output.color = float4(gcGlobalAmbientLight.rgb, 1.0f);
    output.normal = float4(normalW * 0.5f + 0.5f, 1.0f);
    output.zDepth = input.position.z;

    return output;
}


PS_MULTIPLE_RENDER_TARGETS_OUTPUT PSTexturedLightingToMultipleRTs_AlphaClip(
    VS_TEXTURED_LIGHTING_OUTPUT input,
    uint nPrimitiveID : SV_PrimitiveID)
{
    PS_MULTIPLE_RENDER_TARGETS_OUTPUT output;

    uint materialId = input.materialId;
    MATERIAL mat = gMaterials[materialId];

    float2 diffuseUV = GetDiffuseUV(materialId, input.uv);
    float2 normalUV = GetNormalUV(materialId, input.uv);
    float2 emissiveUV = GetEmissiveUV(materialId, input.uv);
    float2 specularUV = GetSpecularUV(materialId, input.uv);

    float4 diffuseSample = SampleTextureRGBA(
        mat.TextureIndices.x,
        diffuseUV,
        float4(1.0f, 1.0f, 1.0f, 1.0f)
    );

    float finalAlpha = diffuseSample.a * mat.m_cDiffuse.a;
    clip(finalAlpha - 0.5f);

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
        materialId,
        input.positionW,
        normalW,
        texColor,
        emissiveColor,
        specularColor,
        shininess
    );

    output.cTexture = texColor;
    output.cIllumination = illumination;
    output.color = illumination;
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

struct VS_SKINNED_INSTANCED_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;
    uint4 blendIndices : BLENDINDICES;
    float4 blendWeights : BLENDWEIGHT;

    float4 instWorld0 : INSTANCE_WORLD0;
    float4 instWorld1 : INSTANCE_WORLD1;
    float4 instWorld2 : INSTANCE_WORLD2;
    float4 instWorld3 : INSTANCE_WORLD3;
    uint instMaterialId : INSTANCE_MATERIAL_ID0;
    uint instBoneBase : INSTANCE_BONE_BASE0;
};

struct VS_SKINNED_OUTPUT
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangentW : TANGENT;
    nointerpolation uint materialId : MATERIAL_ID;
};

float4 SkinPosition4(float3 position, uint4 blendIndices, float4 blendWeights, uint boneBase)
{
    float4 posL = float4(position, 1.0f);

    float4 p0 = mul(posL, gBonePalette[boneBase + blendIndices.x]) * blendWeights.x;
    float4 p1 = mul(posL, gBonePalette[boneBase + blendIndices.y]) * blendWeights.y;
    float4 p2 = mul(posL, gBonePalette[boneBase + blendIndices.z]) * blendWeights.z;
    float4 p3 = mul(posL, gBonePalette[boneBase + blendIndices.w]) * blendWeights.w;

    return p0 + p1 + p2 + p3;
}

float3 SkinDirection4(float3 v, uint4 blendIndices, float4 blendWeights, uint boneBase)
{
    float4 dirL = float4(v, 0.0f);

    float3 d0 = mul(dirL, gBonePalette[boneBase + blendIndices.x]).xyz * blendWeights.x;
    float3 d1 = mul(dirL, gBonePalette[boneBase + blendIndices.y]).xyz * blendWeights.y;
    float3 d2 = mul(dirL, gBonePalette[boneBase + blendIndices.z]).xyz * blendWeights.z;
    float3 d3 = mul(dirL, gBonePalette[boneBase + blendIndices.w]).xyz * blendWeights.w;

    return d0 + d1 + d2 + d3;
}

VS_SKINNED_OUTPUT VSSkinned(VS_SKINNED_INPUT input)
{
    VS_SKINNED_OUTPUT output;

    const uint boneBase = 0;

    float4 posSkinned = SkinPosition4(input.position, input.blendIndices, input.blendWeights, boneBase);
    float4 posW = mul(posSkinned, gmtxGameObject);

    output.positionW = posW.xyz;
    output.position = mul(mul(posW, gmtxView), gmtxProjection);
    output.uv = input.uv;

    float3 nSkinned = SkinDirection4(input.normal, input.blendIndices, input.blendWeights, boneBase);
    float3 nW = mul(nSkinned, (float3x3) gmtxGameObject);
    output.normalW = nW;

    float3 tSkinned = SkinDirection4(input.tangent.xyz, input.blendIndices, input.blendWeights, boneBase);
    float3 tW = mul(tSkinned, (float3x3) gmtxGameObject);
    output.tangentW = float4(tW, input.tangent.w);

    output.materialId = gnMaterialID;
    return output;
}

VS_SKINNED_OUTPUT VSSkinnedInstanced(VS_SKINNED_INSTANCED_INPUT input)
{
    VS_SKINNED_OUTPUT output;

    float4x4 mtxInstanceWorld = float4x4(
        input.instWorld0,
        input.instWorld1,
        input.instWorld2,
        input.instWorld3
    );

    float4 posSkinned = SkinPosition4(
        input.position,
        input.blendIndices,
        input.blendWeights,
        input.instBoneBase
    );

    float4 posW = mul(posSkinned, mtxInstanceWorld);

    output.positionW = posW.xyz;
    output.position = mul(mul(posW, gmtxView), gmtxProjection);
    output.uv = input.uv;

    float3 nSkinned = SkinDirection4(
        input.normal,
        input.blendIndices,
        input.blendWeights,
        input.instBoneBase
    );
    float3 nW = mul(nSkinned, (float3x3) mtxInstanceWorld);
    output.normalW = nW;

    float3 tSkinned = SkinDirection4(
        input.tangent.xyz,
        input.blendIndices,
        input.blendWeights,
        input.instBoneBase
    );
    float3 tW = mul(tSkinned, (float3x3) mtxInstanceWorld);
    output.tangentW = float4(tW, input.tangent.w);

    output.materialId = input.instMaterialId;
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

float ResolveLinearDepthFromDeviceZ(float deviceZ)
{
    const float nearZ = max(0.0001f, gvFogParams1.x);
    const float farZ = max(nearZ + 0.0001f, gvFogParams1.y);

    // deviceZ: [0,1] depth
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

    if (sceneColorIdx == 0xFFFFFFFFu || sceneColorIdx >= MAX_GLOBAL_SRVS)
        return float4(1, 0, 1, 1);

    if (sceneDepthIdx == 0xFFFFFFFFu || sceneDepthIdx >= MAX_GLOBAL_SRVS)
        return float4(1, 0, 1, 1);

    const float4 sceneColor =
        gtxtGlobalTextures[sceneColorIdx].Sample(gssDefaultSamplerState, input.uv);

    const float deviceZ =
        gtxtGlobalTextures[sceneDepthIdx].Load(
            uint3((uint) input.position.x, (uint) input.position.y, 0)
        ).x;

    const float linearDepth = ResolveLinearDepthFromDeviceZ(deviceZ);
    const float fogFactor = ComputeFogFactor(linearDepth);

    const float3 rgb = lerp(sceneColor.rgb, gvFogColor.rgb, fogFactor);
    return float4(rgb, sceneColor.a);
}