//Billboard.hlsl
#ifndef __BILLBOARD_HLSL__
#define __BILLBOARD_HLSL__

#include "Common.hlsl"
#include "MaterialTexture.hlsl"
#include "RenderTypes.hlsl"

#define BOSS_SUMMON_GLOW_MATERIAL_ID      (MAX_MATERIALS - 4)
#define BOSS_SHOCKWAVE_MATERIAL_ID        (MAX_MATERIALS - 5)
#define BOSS_SHOCKWAVE_WALL_MATERIAL_ID   (MAX_MATERIALS - 6)

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

float4 MakeBossSummonGlowColor(float2 uv, float4 materialDiffuse)
{
    float2 p = uv * 2.0f - 1.0f;
    float r = length(p);

    float circleMask = 1.0f - smoothstep(0.72f, 1.0f, r);

    float centerBoost = 1.0f - smoothstep(0.0f, 0.85f, r);
    float intensity = 0.75f + centerBoost * 0.25f;

    float alpha = materialDiffuse.a * circleMask;

    float3 color = saturate(materialDiffuse.rgb * intensity);

    return float4(color, alpha);
}

float4 MakeBossShockwaveColor(float2 uv, float4 materialDiffuse)
{
    float2 p = uv * 2.0f - 1.0f;
    float r = length(p);

    float circleMask = 1.0f - smoothstep(0.992f, 1.0f, r);

    float ringCenter = 0.94f;
    float ringWidth = 0.080f;

    float ring =
        1.0f - smoothstep(
            ringWidth * 0.45f,
            ringWidth,
            abs(r - ringCenter)
        );

    float innerTrail =
        smoothstep(0.48f, 0.82f, r) *
        (1.0f - smoothstep(0.82f, ringCenter, r));

    float ang = atan2(p.y, p.x);
    float breakup =
        0.88f +
        0.12f * sin(ang * 10.0f + r * 26.0f);

    float alpha =
        (ring * 0.82f + innerTrail * 0.30f) *
        breakup *
        circleMask *
        materialDiffuse.a;

    alpha = saturate(alpha);

    float dustShade = 0.88f + innerTrail * 0.18f;
    float3 rgb = materialDiffuse.rgb * dustShade;

    return float4(rgb, alpha);
}

float4 MakeBossShockwaveWallColor(float2 uv, float4 materialDiffuse)
{
    float x = abs(uv.x * 2.0f - 1.0f);

    float sideFade = 1.0f - smoothstep(0.35f, 1.0f, x);
    float topFade = 1.0f - smoothstep(0.55f, 1.0f, uv.y);
    float bottomBoost = smoothstep(1.0f, 0.0f, uv.y);

    float noise =
        0.82f +
        0.18f * sin(uv.x * 31.0f + uv.y * 11.0f) *
        sin(uv.x * 17.0f - uv.y * 23.0f);

    float alpha =
        sideFade *
        topFade *
        (0.45f + bottomBoost * 0.55f) *
        noise *
        materialDiffuse.a;

    alpha = saturate(alpha);

    float brightness = 0.92f + 0.08f * bottomBoost;
    float3 rgb = materialDiffuse.rgb * brightness;

    return float4(rgb, alpha);
}

PS_MULTIPLE_RENDER_TARGETS_OUTPUT PSItemBillboardUnlitAlphaClip(
    VS_TEXTURED_LIGHTING_OUTPUT input,
    uint nPrimitiveID : SV_PrimitiveID)
{
    PS_MULTIPLE_RENDER_TARGETS_OUTPUT output;

    uint materialId = input.materialId;
    MATERIAL mat = gMaterials[materialId];

    float2 diffuseUV = GetDiffuseUVFromMaterial(mat, input.uv);

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

    float4 texColor;

    if (materialId == BOSS_SUMMON_GLOW_MATERIAL_ID)
    {
        texColor = MakeBossSummonGlowColor(input.uv, mat.m_cDiffuse);
    }
    else if (materialId == BOSS_SHOCKWAVE_MATERIAL_ID)
    {
        texColor = MakeBossShockwaveColor(input.uv, mat.m_cDiffuse);
    }
    else
    {
        float2 diffuseUV = GetDiffuseUVFromMaterial(mat, input.uv);

        float4 diffuseSample = SampleTextureRGBA(
        mat.TextureIndices.x,
        diffuseUV,
        float4(1.0f, 1.0f, 1.0f, 0.0f)
    );

        texColor.rgb = diffuseSample.rgb * mat.m_cDiffuse.rgb;
        texColor.a = diffuseSample.a * mat.m_cDiffuse.a;
    }

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

    float4 color;

    if (materialId == BOSS_SUMMON_GLOW_MATERIAL_ID)
    {
        color = MakeBossSummonGlowColor(input.uv, mat.m_cDiffuse);
    }
    else if (materialId == BOSS_SHOCKWAVE_MATERIAL_ID)
    {
        color = MakeBossShockwaveColor(input.uv, mat.m_cDiffuse);
    }
    else if (materialId == BOSS_SHOCKWAVE_WALL_MATERIAL_ID)
    {
        color = MakeBossShockwaveWallColor(input.uv, mat.m_cDiffuse);
    }
    else
    {
        float2 diffuseUV = GetDiffuseUVFromMaterial(mat, input.uv);

        float4 diffuseSample = SampleTextureRGBA(
        mat.TextureIndices.x,
        diffuseUV,
        float4(1.0f, 1.0f, 1.0f, 0.0f)
    );

        color.rgb = diffuseSample.rgb * mat.m_cDiffuse.rgb;
        color.a = diffuseSample.a * mat.m_cDiffuse.a;
    }

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

    float ageRatio = saturate(input.params0.x);
    float intensity = input.params0.y;
    float seed = input.params0.w;

    float kind = input.params1.x;

    float alpha = 0.0f;
    float3 color = input.color.rgb;

    if (kind < 0.5f)
    {
        float angle = atan2(p.y, p.x);
        
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

        color *= lerp(float3(1.0f, 1.0f, 1.0f), input.color.rgb, 0.20f);

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

        color *= lerp(float3(1.0f, 1.0f, 1.0f), input.color.rgb, 0.25f);
        color *= min(intensity, 1.30f);
    }
    else if (kind < 3.5f)
    {
        // blood
        float2 q = p;

        float angle = atan2(p.y, p.x);
        float wobble =
        0.82f +
        0.18f * sin(angle * 5.0f + seed * 11.37f) +
        0.10f * sin(angle * 11.0f + seed * 3.91f);

        float rr = r / max(wobble, 0.15f);

        float blob = saturate(1.0f - rr * 1.55f);
        blob = pow(blob, 0.55f);

        float lobe =
        saturate(1.0f - length(float2(q.x * 1.5f, q.y * 2.4f + 0.35f)) * 1.5f);

        float shape = max(blob, lobe * 0.65f);

        float fade = saturate(1.0f - ageRatio);
        fade *= fade;

        alpha = saturate(shape * fade * input.color.a);

        float3 darkBlood = float3(0.18f, 0.0f, 0.0f);
        float3 redBlood = input.color.rgb;

        color = lerp(darkBlood, redBlood, saturate(shape * 1.4f));
        color *= input.params0.y;

        clip(alpha - 0.01f);

        return float4(color, alpha);
    }
    else if (kind < 4.5f)
    {
        // poison dust
        float2 q = p;

        float angle = atan2(q.y, q.x);

        float wobble =
        0.84f +
        0.12f * sin(angle * 3.0f + seed * 7.31f) +
        0.07f * sin(angle * 6.0f - seed * 2.17f);

        float rr = r / max(wobble, 0.22f);

        float body =
        1.0f - smoothstep(0.05f, 0.82f, rr);

        float softEdge =
        1.0f - smoothstep(0.46f, 1.18f, rr);

        float noise =
        0.72f +
        0.18f *
        sin(q.x * 7.0f + seed * 1.71f) *
        sin(q.y * 6.0f - seed * 0.93f) +
        0.10f *
        sin((q.x + q.y) * 4.5f + seed * 2.33f);

        noise = saturate(noise);

        float fade = saturate(1.0f - ageRatio);

        float softFade = fade * (0.65f + 0.35f * fade);

        alpha =
        saturate(
            (body * 0.28f + softEdge * 0.62f) *
            noise *
            softFade *
            input.color.a
        );

        float center =
        1.0f - smoothstep(0.0f, 0.58f, rr);

        float3 veryDarkGreen = float3(0.000f, 0.055f, 0.004f);
        float3 darkGreen = float3(0.010f, 0.145f, 0.012f);
        float3 dustGreen = input.color.rgb;

        color =
        lerp(
            veryDarkGreen,
            darkGreen,
            saturate(softEdge * 0.85f)
        );

        color =
        lerp(
            color,
            dustGreen,
            saturate(body * 0.35f + center * 0.12f)
        );

        color *= min(intensity, 0.65f);
    }
    else if (kind < 5.5f)
    {
        // boss melee slash
        float2 q = float2(p.x, -p.y);

        const float PI = 3.14159265f;

        float t = saturate((q.y + 0.92f) / 1.84f);

        float curveX =
            -0.92f +
            1.62f * t +
            0.18f * sin(t * PI) -
            0.04f * t * t;

        float curveY =
            -0.92f +
            1.84f * t;

        float2 curvePos = float2(curveX, curveY);
        float2 d = q - curvePos;

        d.x *= 0.72f;
        d.y *= 0.82f;
        
        float bladeWidth =
        lerp(0.40f, 0.16f, t);

        bladeWidth +=
            0.13f *
            (1.0f - smoothstep(0.00f, 0.32f, t));

        bladeWidth *=
            1.0f -
            0.34f * smoothstep(0.76f, 1.0f, t);

        float distToBlade = length(d);

        float bladeBody =
        1.0f -
        smoothstep(
            bladeWidth,
            bladeWidth + 0.075f,
            distToBlade
        );

        float bladeCore =
        1.0f -
        smoothstep(
            bladeWidth * 0.15f,
            bladeWidth * 0.52f,
            distToBlade
        );

        float2 tipLocal =
        q - float2(0.78f, 0.74f);

        tipLocal.x *= 1.10f;
        tipLocal.y *= 0.58f;

        float tipHook =
        1.0f -
        smoothstep(
            0.15f,
            0.30f,
            length(tipLocal)
        );

        tipHook *= smoothstep(0.62f, 0.90f, t);

        bladeBody = max(bladeBody, tipHook * 0.72f);
        bladeCore = max(bladeCore, tipHook * 0.42f);

        float slashCoord =
        saturate(
            (q.y + q.x * 0.62f + 1.55f) / 3.10f
        );

        float revealHead =
        saturate(ageRatio * 1.55f);

        float revealMask =
        1.0f -
        smoothstep(
            revealHead - 0.08f,
            revealHead + 0.10f,
            slashCoord
        );

        float birthFade = smoothstep(0.00f, 0.07f, ageRatio);

        float lifeFade =
        1.0f -
        smoothstep(0.72f, 1.00f, ageRatio);

        float breakup =
        0.88f +
        0.12f *
        sin(q.x * 16.0f + q.y * 7.0f + seed * 1.73f) *
        sin(q.y * 19.0f - seed * 0.91f);

        breakup = saturate(breakup);

        float outerGlow =
        1.0f -
        smoothstep(
            bladeWidth + 0.04f,
            bladeWidth + 0.30f,
            distToBlade
        );

        outerGlow *= revealMask;

        alpha =
        saturate(
            (
                bladeBody * 0.92f +
                outerGlow * 0.30f
            ) *
            revealMask *
            birthFade *
            lifeFade *
            breakup *
            input.color.a
        );

        float edgeFactor =
        saturate(bladeBody - bladeCore);

        float3 edgeGreen = float3(0.045f, 0.42f, 0.00f);
        float3 bodyGreen = input.color.rgb;
        float3 innerGreen = float3(0.92f, 1.00f, 0.62f);

        color =
        lerp(
            edgeGreen,
            bodyGreen,
            saturate(bladeBody)
        );

        color =
        lerp(
            color,
            innerGreen,
            saturate(bladeCore * 0.92f)
        );

        color =
        lerp(
            color,
            edgeGreen,
            saturate(edgeFactor * 0.20f)
        );

        color *= min(intensity, 1.45f);
    }
    else if (kind < 6.5f)
    {
    // magic circle glow
    // 총구화염과 완전히 다른 초록색 방사형 glow.
        float angle = atan2(p.y, p.x);

        float wobble =
        0.90f +
        0.06f * sin(angle * 5.0f + seed * 4.71f) +
        0.04f * sin(angle * 11.0f - seed * 1.93f);

        float rr = r / max(wobble, 0.20f);

        float core =
        1.0f - smoothstep(0.00f, 0.34f, rr);

        float innerGlow =
        1.0f - smoothstep(0.12f, 0.72f, rr);

        float outerGlow =
        1.0f - smoothstep(0.32f, 1.18f, rr);

        float rim =
        1.0f -
        saturate(
            abs(rr - 0.62f) / 0.26f
        );

        rim = smoothstep(0.0f, 1.0f, rim);

        float birthFade = smoothstep(0.00f, 0.10f, ageRatio);
        float deathFade = 1.0f - smoothstep(0.58f, 1.00f, ageRatio);

        float pulse =
        0.88f +
        0.12f * sin(seed * 3.17f + ageRatio * 10.0f);

        float shape =
            core * 0.20f +
            innerGlow * 0.34f +
            outerGlow * 0.42f +
            rim * 0.18f;

        alpha =
        saturate(
            shape *
            birthFade *
            deathFade *
            pulse *
            input.color.a
        );

        float3 deepGreen = float3(0.00f, 0.20f, 0.02f);
        float3 magicGreen = input.color.rgb;
        float3 hotGreen = float3(0.72f, 1.00f, 0.58f);

        color =
        lerp(
            deepGreen,
            magicGreen,
            saturate(innerGlow * 0.90f + rim * 0.35f)
        );

        color =
        lerp(
            color,
            hotGreen,
            saturate(core * 0.75f)
        );

    // 강한 additive glow.
        color *= min(intensity, 1.35f);
    }
    else
    {
    // magic circle afterimage
    // 작고 부유하는 초록 잔광. Spark/총구 불꽃 형태 아님.
        float2 q = p;

        float angle = atan2(q.y, q.x);

        float wobble =
        0.82f +
        0.10f * sin(angle * 4.0f + seed * 5.13f) +
        0.08f * sin(angle * 9.0f - seed * 2.61f);

        float rr = r / max(wobble, 0.18f);

        float body =
        1.0f - smoothstep(0.04f, 0.78f, rr);

        float soft =
        1.0f - smoothstep(0.30f, 1.12f, rr);

        float noise =
        0.78f +
        0.12f * sin(q.x * 8.0f + seed * 1.77f) *
        sin(q.y * 7.0f - seed * 0.84f) +
        0.10f * sin((q.x - q.y) * 5.0f + seed * 2.23f);

        noise = saturate(noise);

        float birthFade = smoothstep(0.00f, 0.12f, ageRatio);
        float deathFade = 1.0f - smoothstep(0.45f, 1.00f, ageRatio);

        alpha =
            saturate(
               (body * 0.24f + soft * 0.32f) *
               noise *
               birthFade *
               deathFade *
               input.color.a
           );

        float3 darkGreen = float3(0.00f, 0.14f, 0.015f);
        float3 magicGreen = input.color.rgb;
        float3 paleGreen = float3(0.58f, 1.00f, 0.48f);

        color =
        lerp(
            darkGreen,
            magicGreen,
            saturate(soft)
        );

        color =
        lerp(
            color,
            paleGreen,
            saturate(body * 0.45f)
        );

        color *= min(intensity, 1.10f);
    }

    clip(alpha - 0.002f);

    return float4(color, alpha);
}

VS_MUZZLE_FLASH_BILLBOARD_OUTPUT VSBossPoisonProjectileBillboardInstanced(
    VS_MUZZLE_FLASH_BILLBOARD_INPUT input)
{
    VS_MUZZLE_FLASH_BILLBOARD_OUTPUT output;

    float4x4 mtxInstanceWorld = float4x4(
        input.instWorld0,
        input.instWorld1,
        input.instWorld2,
        input.instWorld3
    );

    float3 localPos = input.position.xyz;

    float3 positionW =
        (float3) mul(float4(localPos, 1.0f), mtxInstanceWorld);

    output.position =
        mul(mul(float4(positionW, 1.0f), gmtxView), gmtxProjection);

    output.uv = input.uv;
    output.color = input.instColor;
    output.params0 = input.instParams0;
    output.params1 = input.instParams1;

    return output;
}

float4 PSBossPoisonProjectileProcedural(
    VS_MUZZLE_FLASH_BILLBOARD_OUTPUT input) : SV_TARGET
{
    float2 p = input.uv * 2.0f - 1.0f;

    float r = length(p);
    float angle = atan2(p.y, p.x);

    float coreDiameter = max(input.params0.y, 0.001f);
    float gasDiameter = max(input.params0.z, coreDiameter + 0.001f);
    float seed = input.params0.w;

    float coreRadiusUv = saturate(coreDiameter / gasDiameter);

    // 1. 보라색 코어
    float coreEdgeNoise =
        0.030f *
        (
            sin(angle * 5.0f + seed * 1.37f) * 0.45f +
            sin(angle * 9.0f - seed * 0.73f) * 0.35f +
            sin((p.x - p.y) * 6.5f + seed * 2.11f) * 0.20f
        );

    float noisyCoreRadius = coreRadiusUv + coreEdgeNoise;

    float coreInner =
        1.0f - smoothstep(
            noisyCoreRadius * 0.38f,
            noisyCoreRadius * 0.72f,
            r
        );

    float coreOuter =
        1.0f - smoothstep(
            noisyCoreRadius * 0.64f,
            noisyCoreRadius * 1.18f,
            r
        );

    float coreAlpha =
        saturate(coreInner * 0.88f + coreOuter * 0.36f);

    float coreCenter =
        1.0f - smoothstep(
            0.0f,
            noisyCoreRadius * 0.62f,
            r
        );

    float3 coreColorDark = float3(0.14f, 0.018f, 0.26f);
    float3 coreColorMid = float3(0.44f, 0.080f, 0.62f);

    float3 coreColor =
        lerp(
            coreColorDark,
            coreColorMid,
            saturate(coreCenter * 0.58f + coreOuter * 0.16f)
        );

     // 2. 연속형 초록 독가스
     float n1 =
        sin(p.x * 5.7f + seed * 1.91f) *
        sin(p.y * 4.9f - seed * 1.17f);

    float n2 =
        sin((p.x + p.y) * 7.3f + seed * 2.41f) *
        sin((p.x - p.y) * 6.1f - seed * 0.83f);

    float n3 =
        sin(angle * 6.0f + r * 7.0f + seed * 1.29f);

    float cloudNoise =
        saturate(
            0.58f +
            n1 * 0.20f +
            n2 * 0.13f +
            n3 * 0.09f
        );

    cloudNoise = smoothstep(0.22f, 0.92f, cloudNoise);

    float centeredGas =
        1.0f - smoothstep(
            0.10f,
            0.88f,
            r
        );

    float outerGas =
        1.0f - smoothstep(
            0.70f,
            1.06f,
            r
        );

    float gasShape =
        saturate(centeredGas * 0.72f + outerGas * 0.42f);

    float outerFade =
        1.0f - smoothstep(0.96f, 1.10f, r);

    gasShape *= outerFade;

    float gasAlpha =
        saturate(gasShape * cloudNoise * 0.70f);

    // 3. 코어 위에 올라오는 앞쪽 가스 베일
    float veilNoise =
        saturate(
            0.55f +
            sin(p.x * 8.3f + seed * 2.17f) * 0.16f +
            sin(p.y * 7.1f - seed * 1.43f) * 0.14f +
            sin((p.x + p.y) * 5.2f + seed * 0.77f) * 0.10f
        );

    veilNoise = smoothstep(0.28f, 0.88f, veilNoise);

    float veilArea =
        1.0f - smoothstep(
            coreRadiusUv * 0.05f,
            coreRadiusUv * 1.38f,
            r
        );

    float frontVeilAlpha =
        saturate(veilArea * veilNoise * 0.18f);

    // 4. 색상
    float gasShade =
        saturate(
            0.70f +
            0.30f *
            (
                0.5f +
                0.5f * sin(p.x * 10.0f + p.y * 8.0f + seed * 1.7f)
            )
        );

    float3 gasColor =
        lerp(
            float3(0.035f, 0.25f, 0.045f),
            float3(0.16f, 0.68f, 0.14f),
            gasShade
        );

    // 5. 최종 합성
    float gasOverCoreSuppression =
        lerp(1.0f, 0.16f, coreAlpha);

    float backGasBlend =
        saturate(gasAlpha * gasOverCoreSuppression);

    float3 outColor =
        lerp(coreColor, gasColor, backGasBlend);

    float frontGasBlend =
        saturate(frontVeilAlpha * 0.85f);

    outColor =
        lerp(outColor, gasColor, frontGasBlend);

    float outAlpha =
        saturate(
            coreAlpha * 0.94f +
            gasAlpha * 0.62f +
            frontVeilAlpha * 0.70f
        );

    outAlpha *= outerFade;
    outAlpha *= input.color.a;

    clip(outAlpha - 0.004f);

    return float4(outColor, outAlpha);
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

    float3 baseColor = saturate(input.color.rgb);

    float3 edgeColor = baseColor * 0.55f;
    float3 coreColor = saturate(baseColor * 1.45f + float3(0.10f, 0.08f, 0.03f));

    float3 color = lerp(edgeColor, coreColor, center);

    color *= lerp(0.85f, 1.15f, headBoost);

    return float4(color, alpha);
}

#endif