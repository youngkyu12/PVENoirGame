//-----------------------------------------------------------------------------
// File: Light.hlsl
//-----------------------------------------------------------------------------
#define MAX_LIGHTS			4 
#define MAX_MATERIALS		256

#define POINT_LIGHT			1
#define SPOT_LIGHT			2
#define DIRECTIONAL_LIGHT	3

#define _WITH_LOCAL_VIEWER_HIGHLIGHTING
#define _WITH_THETA_PHI_CONES
//#define _WITH_REFLECT

#define MAX_GLOBAL_SRVS 8192

struct LIGHT
{
	float4				m_cAmbient;
	float4				m_cDiffuse;
	float4				m_cSpecular;
	float3				m_vPosition;
	float 				m_fFalloff;
	float3				m_vDirection;
	float 				m_fTheta; //cos(m_fTheta)
	float3				m_vAttenuation;
	float				m_fPhi; //cos(m_fPhi)
	bool				m_bEnable;
	int 				m_nType;
	float				m_fRange;
	float				padding;
};

struct MATERIAL
{
    float4 m_cAmbient;
    float4 m_cDiffuse;
    float4 m_cSpecular; // rgb=specular, a=shininess
    float4 m_cEmissive;

    // x=diffuse, y=normal, z=emissive, w=specular
    uint4 TextureIndices;

    // x=scaleU, y=scaleV, z=offsetU, w=offsetV
    float4 DiffuseUVST;
    float4 NormalUVST;
    float4 EmissiveUVST;
    float4 SpecularUVST;

    // x=diffuseU, y=diffuseV, z=normalU, w=normalV
    uint4 WrapModes0;

    // x=emissiveU, y=emissiveV, z=specularU, w=specularV
    uint4 WrapModes1;
};

cbuffer cbCameraInfo : register(b1)
{
    matrix gmtxView : packoffset(c0);
    matrix gmtxProjection : packoffset(c4);
    float3 gvCameraPosition : packoffset(c8);
};

cbuffer cbMaterial : register(b3)
{
    MATERIAL			gMaterials[MAX_MATERIALS];
};

cbuffer cbLights : register(b4)
{
	LIGHT				gLights[MAX_LIGHTS];
	float4				gcGlobalAmbientLight;
};

// ==============================
// Per-draw Material ID (Root Constants)
// RootSig: RootConstants(num32BitConstants=1, b6)
// ==============================
cbuffer cbPerDrawMaterialId : register(b6)
{
    uint gnMaterialID;
};

float3 SchlickFresnel(float3 R0, float3 vNormal, float3 vToLight)
{
    float cosIncidentAngle = saturate(dot(vNormal, vToLight));

    float f0 = 1.0f - cosIncidentAngle;
    float3 reflectPercent = R0 + (1.0f - R0) * (f0 * f0 * f0 * f0 * f0);

    return reflectPercent;
}

float3 BlinnPhong(float3 fDiffuseFactor, float3 vToLight, float3 vNormal, float3 vToCamera, float4 texColor)
{
    MATERIAL mat = gMaterials[gnMaterialID];

    const float shininess = mat.m_cSpecular.a;
    const float3 specularColor = mat.m_cSpecular.rgb;

    if (shininess <= 0.0f)
        return texColor.rgb * fDiffuseFactor;

    float3 vHalf = normalize(vToCamera + vToLight);

    float roughnessFactor =
        (shininess + 8.0f) *
        pow(max(dot(vHalf, vNormal), 0.0f), shininess) / 8.0f;

    float3 fresnelFactor = SchlickFresnel(specularColor, vHalf, vToLight);
    float3 specAlbedo = fresnelFactor * roughnessFactor;
    specAlbedo = specAlbedo / (specAlbedo + 1.0f);

    return (texColor.rgb + specAlbedo) * fDiffuseFactor;
}

float4 DirectionalLight(int nIndex, float3 vNormal, float3 vToCamera, float4 texColor)
{
    MATERIAL mat = gMaterials[gnMaterialID];

    float3 vToLight = -gLights[nIndex].m_vDirection;
    float ndotl = max(dot(vToLight, vNormal), 0.0f);

    float3 lightStrength = gLights[nIndex].m_cDiffuse.rgb * ndotl;
    float3 litColor = BlinnPhong(lightStrength, vToLight, vNormal, vToCamera, texColor);

    float3 ambientColor = gLights[nIndex].m_cAmbient.rgb * mat.m_cAmbient.rgb;

    return float4(ambientColor + litColor, 0.0f);
	// 원래 코드
//    float3 vToLight = -gLights[nIndex].m_vDirection;
//    float fDiffuseFactor = dot(vToLight, vNormal);
//    float fSpecularFactor = 0.0f;
//    if (fDiffuseFactor > 0.0f)
//    {
//        if (gMaterials[gnMaterialID].m_cSpecular.a != 0.0f)
//        {
//#ifdef _WITH_REFLECT
//			float3 vReflect = reflect(-vToLight, vNormal);
//			fSpecularFactor = pow(max(dot(vReflect, vToCamera), 0.0f), gMaterials[gnMaterialID].m_cSpecular.a);
//#else
//#ifdef _WITH_LOCAL_VIEWER_HIGHLIGHTING
//            float3 vHalf = normalize(vToCamera + vToLight);
//#else
//				float3 vHalf = float3(0.0f, 1.0f, 0.0f);
//#endif
//            fSpecularFactor = pow(max(dot(vHalf, vNormal), 0.0f), gMaterials[gnMaterialID].m_cSpecular.a);
//#endif
//        }
//    }

//    return ((gLights[nIndex].m_cAmbient * gMaterials[gnMaterialID].m_cAmbient) + (gLights[nIndex].m_cDiffuse * fDiffuseFactor * gMaterials[gnMaterialID].m_cDiffuse) + (gLights[nIndex].m_cSpecular * fSpecularFactor * gMaterials[gnMaterialID].m_cSpecular));
}

float4 PointLight(int nIndex, float3 vPosition, float3 vNormal, float3 vToCamera)
{
	float3 vToLight = gLights[nIndex].m_vPosition - vPosition;
	float fDistance = length(vToLight);
	if (fDistance <= gLights[nIndex].m_fRange)
	{
		float fSpecularFactor = 0.0f;
		vToLight /= fDistance;
		float fDiffuseFactor = dot(vToLight, vNormal);
		if (fDiffuseFactor > 0.0f)
		{
			if (gMaterials[gnMaterialID].m_cSpecular.a != 0.0f)
			{
#ifdef _WITH_REFLECT
				float3 vReflect = reflect(-vToLight, vNormal);
				fSpecularFactor = pow(max(dot(vReflect, vToCamera), 0.0f), gMaterials[gnMaterialID].m_cSpecular.a);
#else
#ifdef _WITH_LOCAL_VIEWER_HIGHLIGHTING
				float3 vHalf = normalize(vToCamera + vToLight);
#else
				float3 vHalf = float3(0.0f, 1.0f, 0.0f);
#endif
				fSpecularFactor = pow(max(dot(vHalf, vNormal), 0.0f), gMaterials[gnMaterialID].m_cSpecular.a);
#endif
			}
		}
		float fAttenuationFactor = 1.0f / dot(gLights[nIndex].m_vAttenuation, float3(1.0f, fDistance, fDistance*fDistance));

		return(((gLights[nIndex].m_cAmbient * gMaterials[gnMaterialID].m_cAmbient) + (gLights[nIndex].m_cDiffuse * fDiffuseFactor * gMaterials[gnMaterialID].m_cDiffuse) + (gLights[nIndex].m_cSpecular * fSpecularFactor * gMaterials[gnMaterialID].m_cSpecular)) * fAttenuationFactor);
	}
	return(float4(0.0f, 0.0f, 0.0f, 0.0f));
}

float4 SpotLight(int nIndex, float3 vPosition, float3 vNormal, float3 vToCamera)
{
	float3 vToLight = gLights[nIndex].m_vPosition - vPosition;
	float fDistance = length(vToLight);
	if (fDistance <= gLights[nIndex].m_fRange)
	{
		float fSpecularFactor = 0.0f;
		vToLight /= fDistance;
		float fDiffuseFactor = dot(vToLight, vNormal);
		if (fDiffuseFactor > 0.0f)
		{
			if (gMaterials[gnMaterialID].m_cSpecular.a != 0.0f)
			{
#ifdef _WITH_REFLECT
				float3 vReflect = reflect(-vToLight, vNormal);
				fSpecularFactor = pow(max(dot(vReflect, vToCamera), 0.0f), gMaterials[gnMaterialID].m_cSpecular.a);
#else
#ifdef _WITH_LOCAL_VIEWER_HIGHLIGHTING
				float3 vHalf = normalize(vToCamera + vToLight);
#else
				float3 vHalf = float3(0.0f, 1.0f, 0.0f);
#endif
				fSpecularFactor = pow(max(dot(vHalf, vNormal), 0.0f), gMaterials[gnMaterialID].m_cSpecular.a);
#endif
			}
		}
#ifdef _WITH_THETA_PHI_CONES
		float fAlpha = max(dot(-vToLight, gLights[nIndex].m_vDirection), 0.0f);
		float fSpotFactor = pow(max(((fAlpha - gLights[nIndex].m_fPhi) / (gLights[nIndex].m_fTheta - gLights[nIndex].m_fPhi)), 0.0f), gLights[nIndex].m_fFalloff);
#else
		float fSpotFactor = pow(max(dot(-vToLight, gLights[i].m_vDirection), 0.0f), gLights[i].m_fFalloff);
#endif
		float fAttenuationFactor = 1.0f / dot(gLights[nIndex].m_vAttenuation, float3(1.0f, fDistance, fDistance*fDistance));

		return(((gLights[nIndex].m_cAmbient * gMaterials[gnMaterialID].m_cAmbient) + (gLights[nIndex].m_cDiffuse * fDiffuseFactor * gMaterials[gnMaterialID].m_cDiffuse) + (gLights[nIndex].m_cSpecular * fSpecularFactor * gMaterials[gnMaterialID].m_cSpecular)) * fAttenuationFactor * fSpotFactor);
	}
	return(float4(0.0f, 0.0f, 0.0f, 0.0f));
}

float4 Lighting(float3 vPosition, float3 vNormal, float4 texColor)
{
    MATERIAL mat = gMaterials[gnMaterialID];

    float3 vCameraPosition = float3(gvCameraPosition.x, gvCameraPosition.y, gvCameraPosition.z);
    float3 vToCamera = normalize(vCameraPosition - vPosition);

    float4 cColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (gLights[i].m_bEnable)
        {
            if (gLights[i].m_nType == DIRECTIONAL_LIGHT)
            {
                cColor += DirectionalLight(i, vNormal, vToCamera, texColor);
            }
            //else if (gLights[i].m_nType == POINT_LIGHT)
            //{
            //    cColor += PointLight(i, vPosition, vNormal, vToCamera);
            //}
            //else if (gLights[i].m_nType == SPOT_LIGHT)
            //{
            //    cColor += SpotLight(i, vPosition, vNormal, vToCamera);
            //}
        }
    }

    cColor += (gcGlobalAmbientLight * mat.m_cAmbient);
    cColor.a = mat.m_cDiffuse.a;

    return cColor;
}

