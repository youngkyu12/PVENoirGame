//Lighting.hlsl
#ifndef __LIGHTING_HLSL__
#define __LIGHTING_HLSL__

#include "Common.hlsl"
#include "MaterialTexture.hlsl"

struct LIGHT
{
    float4 m_cAmbient;
    float4 m_cDiffuse;
    float4 m_cSpecular;
    float3 m_vPosition;
    float m_fFalloff;
    float3 m_vDirection;
    float m_fTheta;
    float3 m_vAttenuation;
    float m_fPhi;
    uint m_bEnable;
    int m_nType;
    float m_fRange;
    float padding;
};

cbuffer cbLights : register(b4)
{
    LIGHT gLights[MAX_LIGHTS];
    float4 gcGlobalAmbientLight;
};

float3 SchlickFresnel(float3 R0, float3 vNormal, float3 vToLight)
{
    float cosIncidentAngle = saturate(dot(vNormal, vToLight));
    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);
    return reflectPercent;
}

float3 BlinnPhong(
    float3 fDiffuseFactor,
    float3 vToLight,
    float3 vNormal,
    float3 vToCamera,
    float4 texColor,
    float3 specularColor,
    float shininess)
{
    if (shininess <= 0.0f ||
        max(max(specularColor.r, specularColor.g), specularColor.b) <= 0.0f)
    {
        return texColor.rgb * fDiffuseFactor;
    }

    float3 vHalf = normalize(vToCamera + vToLight);

    float roughnessFactor =
        (shininess + 8.0f) *
        pow(max(dot(vHalf, vNormal), 0.0f), shininess) *
        0.125f;

    float3 fresnelFactor = SchlickFresnel(specularColor, vHalf, vToLight);
    float3 specAlbedo = fresnelFactor * roughnessFactor;
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (texColor.rgb + specAlbedo) * fDiffuseFactor;
}

float4 DirectionalLight(
    int nIndex,
    float3 N,
    float3 vToCamera,
    float4 texColor,
    float3 specularColor,
    float shininess,
    float4 shadowPosH)
{
    float3 vToLight = normalize(-gLights[nIndex].m_vDirection);
    float ndotl = saturate(dot(vToLight, N));

    float3 lightStrength = gLights[nIndex].m_cDiffuse.rgb * ndotl;

    float3 litColor = BlinnPhong(
        lightStrength,
        vToLight,
        N,
        vToCamera,
        texColor,
        specularColor,
        shininess
    );

    float shadowFactor = CalcShadowFactor(shadowPosH, N, vToLight);

    float3 ambientColor =
        gLights[nIndex].m_cAmbient.rgb *
        texColor.rgb;

    return float4(ambientColor + litColor * shadowFactor, 0.0f);
}

float4 PointLight(
    int nIndex,
    float3 vPosition,
    float3 N,
    float3 vToCamera,
    float4 texColor,
    float3 specularColor,
    float shininess)
{
    float3 vToLight = gLights[nIndex].m_vPosition - vPosition;

    float distSq = dot(vToLight, vToLight);
    float range = gLights[nIndex].m_fRange;
    float rangeSq = range * range;

    if (distSq <= rangeSq && distSq > 1e-12f)
    {
        float invDistance = rsqrt(distSq);
        float fDistance = distSq * invDistance;

        vToLight *= invDistance;

        float fDiffuseFactor = saturate(dot(vToLight, N));
        float3 lightStrength = gLights[nIndex].m_cDiffuse.rgb * fDiffuseFactor;

        float3 litColor = BlinnPhong(
            lightStrength,
            vToLight,
            N,
            vToCamera,
            texColor,
            specularColor,
            shininess
        );

        float3 ambientColor =
            gLights[nIndex].m_cAmbient.rgb *
            texColor.rgb;

        float fAttenuationFactor =
            rcp(dot(
                gLights[nIndex].m_vAttenuation,
                float3(1.0f, fDistance, fDistance * fDistance)
            ));

        return float4(ambientColor + litColor, 0.0f) * fAttenuationFactor;
    }

    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

float4 SpotLight(
    int nIndex,
    float3 vPosition,
    float3 N,
    float3 vToCamera,
    float4 texColor,
    float3 specularColor,
    float shininess)
{
    float3 vToLight = gLights[nIndex].m_vPosition - vPosition;

    float distSq = dot(vToLight, vToLight);
    float range = gLights[nIndex].m_fRange;
    float rangeSq = range * range;

    if (distSq <= rangeSq && distSq > 1e-12f)
    {
        float invDistance = rsqrt(distSq);
        float fDistance = distSq * invDistance;

        vToLight *= invDistance;

        float fDiffuseFactor = saturate(dot(vToLight, N));
        float3 lightStrength = gLights[nIndex].m_cDiffuse.rgb * fDiffuseFactor;

        float3 litColor = BlinnPhong(
            lightStrength,
            vToLight,
            N,
            vToCamera,
            texColor,
            specularColor,
            shininess
        );

        float3 lightDir = normalize(gLights[nIndex].m_vDirection);

#ifdef _WITH_THETA_PHI_CONES
        float fAlpha = saturate(dot(-vToLight, lightDir));
        float fSpotFactor = pow(
            max(
                (fAlpha - gLights[nIndex].m_fPhi) /
                (gLights[nIndex].m_fTheta - gLights[nIndex].m_fPhi),
                0.0f
            ),
            gLights[nIndex].m_fFalloff
        );
#else
        float fSpotFactor = pow(
            saturate(dot(-vToLight, lightDir)),
            gLights[nIndex].m_fFalloff
        );
#endif

        float3 ambientColor =
            gLights[nIndex].m_cAmbient.rgb *
            texColor.rgb;

        float fAttenuationFactor =
            rcp(dot(
                gLights[nIndex].m_vAttenuation,
                float3(1.0f, fDistance, fDistance * fDistance)
            ));

        return float4(ambientColor + litColor, 0.0f) * fAttenuationFactor * fSpotFactor;
    }

    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}

float4 Lighting(
    uint materialId,
    float3 vPosition,
    float3 vNormal,
    float4 texColor,
    float3 emissiveColor,
    float3 specularColor,
    float shininess,
    float4 shadowPosH)
{
    float3 vToCamera = normalize(gvCameraPosition - vPosition);
    float3 N = normalize(vNormal);

    float4 cColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

    [unroll]
    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (!gLights[i].m_bEnable)
            continue;

        if (gLights[i].m_nType == DIRECTIONAL_LIGHT)
        {
            cColor += DirectionalLight(
                i,
                N,
                vToCamera,
                texColor,
                specularColor,
                shininess,
                shadowPosH
            );
        }
        else if (gLights[i].m_nType == POINT_LIGHT)
        {
            cColor += PointLight(
                i,
                vPosition,
                N,
                vToCamera,
                texColor,
                specularColor,
                shininess
            );
        }
        else if (gLights[i].m_nType == SPOT_LIGHT)
        {
            cColor += SpotLight(
                i,
                vPosition,
                N,
                vToCamera,
                texColor,
                specularColor,
                shininess
            );
        }
    }

    cColor.rgb += gcGlobalAmbientLight.rgb * texColor.rgb;
    cColor.rgb += emissiveColor;
    cColor.a = texColor.a;

    return cColor;
}

#endif
