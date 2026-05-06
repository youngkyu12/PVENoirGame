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

struct VS_MUZZLE_FLASH_BILLBOARD_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float4 tangent : TANGENT;

    float4 instWorld0 : INSTANCE_WORLD0;
    float4 instWorld1 : INSTANCE_WORLD1;
    float4 instWorld2 : INSTANCE_WORLD2;
    float4 instWorld3 : INSTANCE_WORLD3;

    float4 instColor : INSTANCE_COLOR0;
    float4 instParams0 : INSTANCE_PARAMS0;
    float4 instParams1 : INSTANCE_PARAMS1;
};

struct VS_MUZZLE_FLASH_BILLBOARD_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float4 params0 : TEXCOORD1;
    float4 params1 : TEXCOORD2;
};

VS_MUZZLE_FLASH_BILLBOARD_OUTPUT VSMuzzleFlashBillboardInstanced(
    VS_MUZZLE_FLASH_BILLBOARD_INPUT input)
{
    VS_MUZZLE_FLASH_BILLBOARD_OUTPUT output;

    float ageRatio = saturate(input.instParams0.x);
    float rotation = input.instParams0.z;

    float s = sin(rotation);
    float c = cos(rotation);

    float2 localXY = input.position.xy;

    // 약간 커지는 느낌
    float grow = lerp(1.0f, 1.2f, ageRatio);
    localXY *= grow;

    float2 rotatedXY = float2(
        localXY.x * c - localXY.y * s,
        localXY.x * s + localXY.y * c
    );

    float3 localPos = float3(rotatedXY, input.position.z);

    float4x4 mtxInstanceWorld = float4x4(
        input.instWorld0,
        input.instWorld1,
        input.instWorld2,
        input.instWorld3
    );

    float3 positionW = (float3) mul(float4(localPos, 1.0f), mtxInstanceWorld);

    output.position = mul(mul(float4(positionW, 1.0f), gmtxView), gmtxProjection);
    output.uv = input.uv;
    output.color = input.instColor;
    output.params0 = input.instParams0;
    output.params1 = input.instParams1;

    return output;
}

float4 PSMuzzleFlashProcedural(
    VS_MUZZLE_FLASH_BILLBOARD_OUTPUT input) : SV_TARGET
{
    float2 p = input.uv * 2.0f - 1.0f;

    float r = length(p);
    float angle = atan2(p.y, p.x);

    float ageRatio = saturate(input.params0.x);
    float intensity = input.params0.y;
    float seed = input.params0.w;

    float kind = input.params1.x;

    float alpha = 0.0f;
    float3 color = input.color.rgb;

    // 0 = core
    if (kind < 0.5f)
    {
        float core = saturate(1.0f - r * 2.8f);
        core = pow(core, 1.4f);

        float rays = abs(cos(angle * 6.0f + seed * 3.17f));
        rays = pow(rays, 18.0f);
        rays *= saturate(1.0f - r * 0.85f);

        float flicker = 0.85f + 0.15f * sin(angle * 13.0f + seed * 17.0f);
        float shape = core * 1.45f + rays * flicker * 1.25f;

        float fade = saturate(1.0f - ageRatio);
        fade *= fade;

        alpha = saturate(shape * fade * input.color.a);

        float3 hotColor = float3(1.0f, 0.96f, 0.70f);
        float3 outerColor = input.color.rgb;
        color = lerp(hotColor, outerColor, saturate(r));
        color *= intensity;
    }
    // 1 = ring
    else if (kind < 1.5f)
    {
        float ringRadius = lerp(0.15f, 0.70f, ageRatio);
        float ringWidth = lerp(0.14f, 0.05f, ageRatio);

        float ring = 1.0f - saturate(abs(r - ringRadius) / max(ringWidth, 0.001f));
        ring = smoothstep(0.0f, 1.0f, ring);

        float fade = saturate(1.0f - ageRatio);
        alpha = ring * fade * 0.55f * input.color.a;

        color = input.color.rgb * intensity * 0.85f;
    }
    // 2 = spark
    else
    {
        float2 q = p;

        // 세로로 긴 streak처럼 보이게
        float body = exp(-abs(q.x) * 10.0f) * saturate(1.1f - abs(q.y) * 1.6f);

        float head = saturate(1.2f - length(float2(q.x * 2.0f, q.y + 0.7f)) * 2.0f);
        float tail = saturate(1.0f - length(float2(q.x * 3.0f, q.y - 0.2f)) * 1.2f);

        float shape = body * 0.8f + head * 0.8f + tail * 0.35f;

        float fade = saturate(1.0f - ageRatio);
        fade *= fade;

        alpha = saturate(shape * fade * input.color.a);

        float3 hotColor = float3(1.0f, 0.95f, 0.70f);
        color = lerp(input.color.rgb, hotColor, 0.55f) * intensity;
    }

    clip(alpha - 0.002f);

    return float4(color, alpha);
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

PS_MULTIPLE_RENDER_TARGETS_OUTPUT PSItemBillboardUnlitAlphaClip(
    VS_TEXTURED_LIGHTING_OUTPUT input,
    uint nPrimitiveID : SV_PrimitiveID)
{
    PS_MULTIPLE_RENDER_TARGETS_OUTPUT output;

    uint materialId = input.materialId;
    MATERIAL mat = gMaterials[materialId];

    float2 diffuseUV = GetDiffuseUV(materialId, input.uv);

    float4 diffuseSample = SampleTextureRGBA(
        mat.TextureIndices.x,
        diffuseUV,
        float4(1.0f, 1.0f, 1.0f, 1.0f)
    );

    float4 texColor = diffuseSample * mat.m_cDiffuse;

    clip(texColor.a - 0.5f);
    
    output.cTexture = texColor;
    output.cIllumination = texColor;
    output.color = texColor;
    
    output.normal = float4(0.5f, 0.5f, 1.0f, 1.0f);

    output.zDepth = input.position.z;

    return output;
}

PS_MULTIPLE_RENDER_TARGETS_OUTPUT PSItemBillboardUnlitTransparent(
    VS_TEXTURED_LIGHTING_OUTPUT input,
    uint nPrimitiveID : SV_PrimitiveID)
{
    PS_MULTIPLE_RENDER_TARGETS_OUTPUT output;

    uint materialId = input.materialId;
    MATERIAL mat = gMaterials[materialId];

    float2 diffuseUV = GetDiffuseUV(materialId, input.uv);

    float4 diffuseSample = SampleTextureRGBA(
        mat.TextureIndices.x,
        diffuseUV,
        float4(1.0f, 1.0f, 1.0f, 0.0f)
    );

    float4 texColor;
    texColor.rgb = diffuseSample.rgb * mat.m_cDiffuse.rgb;
    texColor.a = diffuseSample.a * mat.m_cDiffuse.a;

    clip(texColor.a - 0.001f);

    output.color = texColor;
    output.cTexture = texColor;
    output.cIllumination = texColor;
    output.normal = float4(0.5f, 0.5f, 1.0f, texColor.a);
    output.zDepth = input.position.z;

    return output;
}

float4 PSItemBillboardUnlitTransparentForward(
    VS_TEXTURED_LIGHTING_OUTPUT input,
    uint nPrimitiveID : SV_PrimitiveID) : SV_TARGET
{
    uint materialId = input.materialId;
    MATERIAL mat = gMaterials[materialId];

    float2 diffuseUV = GetDiffuseUV(materialId, input.uv);

    float4 diffuseSample = SampleTextureRGBA(
        mat.TextureIndices.x,
        diffuseUV,
        float4(1.0f, 1.0f, 1.0f, 0.0f)
    );

    float4 color;
    color.rgb = diffuseSample.rgb * mat.m_cDiffuse.rgb;
    color.a = diffuseSample.a * mat.m_cDiffuse.a;

    // 완전 투명한 픽셀만 제거.
    // 반투명 가장자리는 유지.
    clip(color.a - 0.001f);

    return color;
}

#endif