//Billboard.hlsl
#ifndef __BILLBOARD_HLSL__
#define __BILLBOARD_HLSL__

#include "Common.hlsl"
#include "MaterialTexture.hlsl"
#include "RenderTypes.hlsl"

// -----------------------------------------------------------------------------
// Item Billboard
// -----------------------------------------------------------------------------

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

VS_TEXTURED_LIGHTING_OUTPUT VSItemBillboardInstanced(
    VS_ITEM_BILLBOARD_INSTANCED_INPUT input)
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

    output.materialId = input.instMaterialId;
    output.shadowPosH = mul(float4(output.positionW, 1.0f), gmtxShadowTransform);

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

    clip(color.a - 0.001f);

    return color;
}

// -----------------------------------------------------------------------------
// Muzzle Flash Billboard
// -----------------------------------------------------------------------------

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

    if (kind < 0.5f)
    {
        float core = saturate(1.0f - r * 2.8f);
        core = pow(core, 1.35f);

        float rays = abs(cos(angle * 6.0f + seed * 3.17f));
        rays = pow(rays, 18.0f);
        rays *= saturate(1.0f - r * 0.85f);

        float flicker = 0.85f + 0.15f * sin(angle * 13.0f + seed * 17.0f);
        float shape = core * 1.45f + rays * flicker * 1.25f;

        float fade = saturate(1.0f - ageRatio);
        fade *= fade;

        alpha = saturate(shape * fade * input.color.a);

        // 불꽃 팔레트:
        // 중심은 노란 불꽃, 중간은 주황, 외곽은 붉은 불꽃.
        float radial = saturate(r);

        float3 hotYellow = float3(1.0f, 0.78f, 0.22f);
        float3 flameOrange = float3(1.0f, 0.34f, 0.04f);
        float3 emberRed = float3(0.72f, 0.08f, 0.015f);

        color = lerp(
            hotYellow,
            flameOrange,
            smoothstep(0.10f, 0.55f, radial)
        );

        color = lerp(
            color,
            emberRed,
            smoothstep(0.55f, 1.0f, radial)
        );

        // 입력 색상으로 약간 tint만 준다.
        color *= lerp(float3(1.0f, 1.0f, 1.0f), input.color.rgb, 0.20f);

        // additive 포화로 하얘지는 것을 줄이기 위해 상한을 둔다.
        color *= min(intensity, 1.55f);
    }
    else if (kind < 1.5f)
    {
        float ringRadius = lerp(0.15f, 0.70f, ageRatio);
        float ringWidth = lerp(0.14f, 0.05f, ageRatio);

        float ring = 1.0f - saturate(abs(r - ringRadius) / max(ringWidth, 0.001f));
        ring = smoothstep(0.0f, 1.0f, ring);

        float fade = saturate(1.0f - ageRatio);
        alpha = ring * fade * 0.45f * input.color.a;

        float3 ringInner = float3(1.0f, 0.45f, 0.06f);
        float3 ringOuter = float3(0.75f, 0.08f, 0.015f);

        color = lerp(
            ringInner,
            ringOuter,
            smoothstep(0.25f, 1.0f, r)
        );

        color *= min(intensity, 1.25f);
    }
    else if (kind < 2.5f)
    {
        float2 q = p;

        float body =
            exp(-abs(q.x) * 10.0f) *
            saturate(1.1f - abs(q.y) * 1.6f);

        float head =
            saturate(1.2f - length(float2(q.x * 2.0f, q.y + 0.7f)) * 2.0f);

        float tail =
            saturate(1.0f - length(float2(q.x * 3.0f, q.y - 0.2f)) * 1.2f);

        float shape = body * 0.8f + head * 0.8f + tail * 0.35f;

        float fade = saturate(1.0f - ageRatio);
        fade *= fade;

        alpha = saturate(shape * fade * input.color.a);

        float sparkHot = saturate(head + body * 0.45f);

        float3 sparkYellow = float3(1.0f, 0.66f, 0.14f);
        float3 sparkOrange = float3(1.0f, 0.24f, 0.025f);

        color = lerp(sparkOrange, sparkYellow, sparkHot);

        // 입력 색상 반영은 약하게. 너무 많이 곱하면 다시 하얘질 수 있음.
        color *= lerp(float3(1.0f, 1.0f, 1.0f), input.color.rgb, 0.25f);
        color *= min(intensity, 1.30f);
    }
    else
    {
        // blood
        float2 q = p;

        // seed 기반으로 모양을 약간 찌그러뜨린다.
        float wobble =
        0.82f +
        0.18f * sin(angle * 5.0f + seed * 11.37f) +
        0.10f * sin(angle * 11.0f + seed * 3.91f);

        float rr = r / max(wobble, 0.15f);

        float blob = saturate(1.0f - rr * 1.55f);
        blob = pow(blob, 0.55f);

        // 중심보다 한쪽에 살짝 뭉친 핏방울 느낌.
        float lobe =
        saturate(1.0f - length(float2(q.x * 1.5f, q.y * 2.4f + 0.35f)) * 1.5f);

        float shape = max(blob, lobe * 0.65f);

        float fade = saturate(1.0f - ageRatio);
        fade *= fade;

        alpha = saturate(shape * fade * input.color.a);

        float3 darkBlood = float3(0.18f, 0.0f, 0.0f);
        float3 redBlood = input.color.rgb;

        // 중심은 조금 더 선명한 붉은색, 가장자리는 어둡게.
        color = lerp(darkBlood, redBlood, saturate(shape * 1.4f));
        color *= input.params0.y;

        clip(alpha - 0.01f);

        return float4(color, alpha);
    }

    clip(alpha - 0.002f);

    return float4(color, alpha);
}

// -----------------------------------------------------------------------------
// Sword / Axe Trail Billboard Ribbon
// -----------------------------------------------------------------------------

struct VS_SWORD_TRAIL_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

struct VS_SWORD_TRAIL_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VS_SWORD_TRAIL_OUTPUT VSSwordTrail(VS_SWORD_TRAIL_INPUT input)
{
    VS_SWORD_TRAIL_OUTPUT output;

    output.position =
        mul(mul(float4(input.position, 1.0f), gmtxView), gmtxProjection);

    output.uv = input.uv;
    output.color = input.color;

    return output;
}

float4 PSSwordTrailProcedural(VS_SWORD_TRAIL_OUTPUT input) : SV_TARGET
{
    float along = saturate(input.uv.x);
    float across = saturate(input.uv.y);

    float center = 1.0f - abs(across * 2.0f - 1.0f);
    center = saturate(center);
    center = pow(center, 0.45f);

    float tailFade = smoothstep(0.0f, 0.25f, along);
    float headBoost = smoothstep(0.35f, 1.0f, along);

    float alpha = input.color.a * center * tailFade;
    alpha = saturate(alpha);

    clip(alpha - 0.002f);

    // 기존처럼 완전 흰색 core로 가지 않고,
    // 입력 색을 밝게 만든 정도로만 중심부를 만든다.
    float3 baseColor = saturate(input.color.rgb);

    float3 edgeColor = baseColor * 0.55f;
    float3 coreColor = saturate(baseColor * 1.45f + float3(0.10f, 0.08f, 0.03f));

    float3 color = lerp(edgeColor, coreColor, center);

    // 검 끝 쪽 강조도 색을 유지한 채 밝기만 조금 올린다.
    color *= lerp(0.85f, 1.15f, headBoost);

    return float4(color, alpha);
}

#endif