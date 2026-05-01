#ifndef __GEOMETRY_HLSL__
#define __GEOMETRY_HLSL__

#include "Common.hlsl"
#include "MaterialTexture.hlsl"
#include "Lighting.hlsl"

struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VS_OUTPUT VSDiffused(VS_INPUT input)
{
    VS_OUTPUT output;

    output.position = mul(mul(mul(float4(input.position, 1.0f), gmtxGameObject), gmtxView), gmtxProjection);
    output.color = input.color;

    return output;
}

float4 PSDiffused(VS_OUTPUT input) : SV_TARGET
{
    return input.color;
}

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

    return output;
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

struct VS_ITEM_BILLBOARD_INSTANCED_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;

    float4 instWorld0 : INSTANCE_WORLD0;
    float4 instWorld1 : INSTANCE_WORLD1;
    float4 instWorld2 : INSTANCE_WORLD2;
    float4 instWorld3 : INSTANCE_WORLD3;

    uint instMaterialId : INSTANCE_MATERIAL_ID0;
};

struct VS_TEXTURED_LIGHTING_OUTPUT
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangentW : TANGENT;
    nointerpolation uint materialId : MATERIAL_ID;
    float4 shadowPosH : TEXCOORD1;
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
    output.shadowPosH = mul(float4(output.positionW, 1.0f), gmtxShadowTransform);

    return output;
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
    output.shadowPosH = mul(float4(output.positionW, 1.0f), gmtxShadowTransform);

    return output;
}

VS_TEXTURED_LIGHTING_OUTPUT VSItemBillboardInstanced(VS_ITEM_BILLBOARD_INSTANCED_INPUT input)
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

    // 기존 static instancing과 달리 인스턴스별 material 사용
    output.materialId = input.instMaterialId;

    output.shadowPosH = mul(float4(output.positionW, 1.0f), gmtxShadowTransform);

    return output;
}

struct VS_OCCLUSION_STATIC_OUTPUT
{
    float4 position : SV_POSITION;
};

VS_OCCLUSION_STATIC_OUTPUT VSStaticOcclusionInstanced(VS_TEXTURED_LIGHTING_INSTANCED_INPUT input)
{
    VS_OCCLUSION_STATIC_OUTPUT output;

    float4x4 mtxInstanceWorld = float4x4(
        input.instWorld0,
        input.instWorld1,
        input.instWorld2,
        input.instWorld3
    );

    float3 positionW = (float3) mul(float4(input.position, 1.0f), mtxInstanceWorld);
    output.position = mul(mul(float4(positionW, 1.0f), gmtxView), gmtxProjection);

    return output;
}

float4 PSOcclusionOpaque(VS_OCCLUSION_STATIC_OUTPUT input) : SV_TARGET
{
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
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
        shininess,
        input.shadowPosH
    );

    output.cTexture = texColor;
    output.cIllumination = illumination;
    output.color = illumination;
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
        shininess,
        input.shadowPosH
    );

    output.cTexture = texColor;
    output.cIllumination = illumination;
    output.color = illumination;
    output.normal = float4(normalW * 0.5f + 0.5f, 1.0f);
    output.zDepth = input.position.z;

    return output;
}
#endif