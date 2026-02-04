#define MAX_GLOBAL_SRVS 1024

// Global Texture2D pool: t0 ~ t1023 in space0
Texture2D gtxtGlobalTextures[MAX_GLOBAL_SRVS] : register(t0);

cbuffer cbPlayerInfo : register(b0)
{
    matrix gmtxPlayerWorld;
};


cbuffer cbGameObjectInfo : register(b2)
{
    matrix gmtxGameObject : packoffset(c0);
    uint gnObjectID : packoffset(c4.x);
    uint gnUnused0 : packoffset(c4.y); // (��)gnMaterialID �ڸ�. ���� �� ��.
};

SamplerState gssDefaultSamplerState : register(s0);

cbuffer cbDrawOptions : register(b5)
{
    int4 gvDrawOptions; // x = 'T','L','N','D','Z'
    uint4 gvPostSrvIdx0; // x=T, y=L, z=N, w=D
    uint4 gvPostSrvIdx1; // x=Z, ������ �е�
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

    float3 T = normalize(tangentW_in.xyz);
    // N�� ����ȭ(����)
    T = normalize(T - N * dot(T, N));

    float3 B = normalize(cross(N, T) * tangentW_in.w);

    // tangent-space -> world
    float3 nW = normalize(T * nTS.x + B * nTS.y + N * nTS.z);
    return nW;
}



struct VS_DIFFUSED_INPUT
{
	float3 position : POSITION;
	float4 color : COLOR;
};

struct VS_DIFFUSED_OUTPUT
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

VS_DIFFUSED_OUTPUT VSDiffused(VS_DIFFUSED_INPUT input)
{
	VS_DIFFUSED_OUTPUT output;

	output.position = mul(mul(mul(float4(input.position, 1.0f), gmtxGameObject), gmtxView), gmtxProjection);
	output.color = input.color;

	return(output);
}

float4 PSDiffused(VS_DIFFUSED_OUTPUT input) : SV_TARGET
{
    float4 color = input.color;

    // ���� ���� ����
    color.rgb = pow(color.rgb, 1.0 / 2.2);

    return color;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


struct VS_PLAYER_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    uint4 blendIndices : BLENDINDICES; // ������ �̻���̾ OK
    float4 blendWeights : BLENDWEIGHT; // ������ �̻���̾ OK
};

struct VS_PLAYER_OUTPUT
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

	return(output);
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
//
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

	output.normalW = mul(input.normal, (float3x3)gmtxGameObject);
	output.positionW = (float3)mul(float4(input.position, 1.0f), gmtxGameObject);
	output.position = mul(mul(float4(output.positionW, 1.0f), gmtxView), gmtxProjection);
	output.uv = input.uv;
    float3 tW = mul(input.tangent.xyz, (float3x3) gmtxGameObject);
    output.tangentW = float4(tW, input.tangent.w);

    return (output);
}

float4 PSTexturedLighting(VS_TEXTURED_LIGHTING_OUTPUT input, uint nPrimitiveID : SV_PrimitiveID) : SV_TARGET
{
    uint packedD = gMaterials[gnMaterialID].TextureIndices.x;
    if (packedD == 0)
        return float4(1, 0, 1, 1);
    uint diffuseIndex = packedD - 1;
    if (diffuseIndex >= MAX_GLOBAL_SRVS)
        return float4(1, 0, 0, 1);
    
    float4 cTexture = gtxtGlobalTextures[diffuseIndex].Sample(gssDefaultSamplerState, input.uv);
    
    uint packedN = gMaterials[gnMaterialID].TextureIndices.y;
    float3 normalW = GetNormalWFromMap(packedN, input.normalW, input.tangentW, input.uv);

    float4 cIllumination = Lighting(input.positionW, normalW);
    return cTexture * cIllumination;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
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

    // =========================================================
    // 1. packed SRV index �ؼ�
    // =========================================================
    uint packed = gMaterials[gnMaterialID].TextureIndices.x;

    // �ؽ�ó ����
    if (packed == 0)
    {
        output.color = float4(1, 0, 1, 1); // magenta
        output.cTexture = output.color;
        output.cIllumination = float4(0, 0, 0, 0);
        output.normal = float4(0, 0, 1, 1);
        output.zDepth = input.position.z;
        return output;
    }

    uint diffuseIndex = packed - 1;
    
    if (diffuseIndex >= MAX_GLOBAL_SRVS)
    {
        output.color = float4(1, 0, 0, 1); // red (bad index)
        output.cTexture = output.color;
        output.cIllumination = float4(0, 0, 0, 0);
        output.normal = float4(0, 0, 1, 1);
        output.zDepth = input.position.z;
        return output;
    }

    // =========================================================
    // 2. �ؽ�ó ���ø�
    // =========================================================
    float4 texColor =
        gtxtGlobalTextures[diffuseIndex]
            .Sample(gssDefaultSamplerState, input.uv);

    // =========================================================
    // 3. ���� ���
    // =========================================================
    uint packedN = gMaterials[gnMaterialID].TextureIndices.y;
    float3 normalW = GetNormalWFromMap(packedN, input.normalW, input.tangentW, input.uv);
    float4 illumination = Lighting(input.positionW, normalW);

    // =========================================================
    // 4. MRT ���
    // =========================================================
    output.cTexture = texColor;
    output.cIllumination = illumination;
    //output.color = texColor * illumination;
    output.color = texColor;
    output.normal = float4(normalW * 0.5f + 0.5f, 1.0f);
    output.zDepth = input.position.z;

    return output;
}


//////////////////////////////////////////////////////////////////////////////////////////////////

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

// position
    float4 posL = float4(input.position, 1.0f);
    float4 posSkinned = mul(posL, gBoneTransforms[bi]);
    float4 posW = mul(posSkinned, gmtxGameObject);

    output.positionW = posW.xyz;
    output.position = mul(mul(posW, gmtxView), gmtxProjection);
    output.uv = input.uv;

// normal (direction: w=0)
    float3 nSkinned = mul(float4(input.normal, 0.0f), gBoneTransforms[bi]).xyz;
    float3 nW = mul(nSkinned, (float3x3) gmtxGameObject);
    output.normalW = nW;

// tangent (direction: w=0)
    float3 tSkinned = mul(float4(input.tangent.xyz, 0.0f), gBoneTransforms[bi]).xyz;
    float3 tW = mul(tSkinned, (float3x3) gmtxGameObject);
    output.tangentW = float4(tW, input.tangent.w);

    return output;
}

// �÷��̾�� ��Ű�� VS (cbPlayerInfo(b0) ���)
VS_SKINNED_OUTPUT VSSkinnedPlayer(VS_SKINNED_INPUT input)
{
    VS_SKINNED_OUTPUT output;

    uint bi = input.blendIndices.x;

    float4 posL = float4(input.position, 1.0f);
    float4 posSkinned = mul(posL, gBoneTransforms[bi]);
    float4 posW = mul(posSkinned, gmtxPlayerWorld); // b0�� ���� ���

    output.positionW = posW.xyz;
    output.position = mul(mul(posW, gmtxView), gmtxProjection);
    output.uv = input.uv;

    float3 nSkinned = mul(float4(input.normal, 0.0f), gBoneTransforms[bi]).xyz;
    output.normalW = mul(nSkinned, (float3x3) gmtxPlayerWorld);

    float3 tSkinned = mul(float4(input.tangent.xyz, 0.0f), gBoneTransforms[bi]).xyz;
    float3 tW = mul(tSkinned, (float3x3) gmtxPlayerWorld);
    output.tangentW = float4(tW, input.tangent.w);

    return output;
}



//////////////////////////////////////////////////////////////////////////////////////////////////
//
float4 VSPostProcessing(uint nVertexID : SV_VertexID): SV_POSITION
{
	if (nVertexID == 0)	return(float4(-1.0f, +1.0f, 0.0f, 1.0f));
	if (nVertexID == 1)	return(float4(+1.0f, +1.0f, 0.0f, 1.0f));
	if (nVertexID == 2)	return(float4(+1.0f, -1.0f, 0.0f, 1.0f));

	if (nVertexID == 3)	return(float4(-1.0f, +1.0f, 0.0f, 1.0f));
	if (nVertexID == 4)	return(float4(+1.0f, -1.0f, 0.0f, 1.0f));
	if (nVertexID == 5)	return(float4(-1.0f, -1.0f, 0.0f, 1.0f));

	return(float4(0, 0, 0, 0));
}

float4 PSPostProcessing(float4 position : SV_POSITION): SV_Target
{
	return(float4(0.0f, 0.0f, 0.0f, 1.0f));
}

///////////////////////////////////////////////////////////////////////////////
//
struct VS_SCREEN_RECT_TEXTURED_OUTPUT
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

VS_SCREEN_RECT_TEXTURED_OUTPUT VSScreenRectSamplingTextured(uint nVertexID : SV_VertexID)
{
	VS_SCREEN_RECT_TEXTURED_OUTPUT output = (VS_TEXTURED_OUTPUT)0;

	if (nVertexID == 0) { output.position = float4(-1.0f, +1.0f, 0.0f, 1.0f); output.uv = float2(0.0f, 0.0f); }
	else if (nVertexID == 1) { output.position = float4(+1.0f, +1.0f, 0.0f, 1.0f); output.uv = float2(1.0f, 0.0f); }
	else if (nVertexID == 2) { output.position = float4(+1.0f, -1.0f, 0.0f, 1.0f); output.uv = float2(1.0f, 1.0f); }
							 
	else if (nVertexID == 3) { output.position = float4(-1.0f, +1.0f, 0.0f, 1.0f); output.uv = float2(0.0f, 0.0f); }
	else if (nVertexID == 4) { output.position = float4(+1.0f, -1.0f, 0.0f, 1.0f); output.uv = float2(1.0f, 1.0f); }
	else if (nVertexID == 5) { output.position = float4(-1.0f, -1.0f, 0.0f, 1.0f); output.uv = float2(0.0f, 1.0f); }

	return(output);
}

float4 GetColorFromDepth(float fDepth)
{
	float4 cColor = float4(0.0f, 0.0f, 0.0f, 1.0f);

	if (fDepth >= 1.0f) cColor = float4(1.0f, 1.0f, 1.0f, 1.0f);
	else if (fDepth < 0.00625f) cColor = float4(1.0f, 0.0f, 0.0f, 1.0f);
	else if (fDepth < 0.0125f) cColor = float4(0.0f, 1.0f, 0.0f, 1.0f);
	else if (fDepth < 0.025f) cColor = float4(0.0f, 0.0f, 1.0f, 1.0f);
	else if (fDepth < 0.05f) cColor = float4(1.0f, 1.0f, 0.0f, 1.0f);
	else if (fDepth < 0.075f) cColor = float4(0.0f, 1.0f, 1.0f, 1.0f);
	else if (fDepth < 0.1f) cColor = float4(1.0f, 0.5f, 0.5f, 1.0f);
	else if (fDepth < 0.4f) cColor = float4(0.5f, 1.0f, 1.0f, 1.0f);
	else if (fDepth < 0.6f) cColor = float4(1.0f, 0.0f, 1.0f, 1.0f);
	else if (fDepth < 0.8f) cColor = float4(0.5f, 0.5f, 1.0f, 1.0f);
	else if (fDepth < 0.9f) cColor = float4(0.5f, 1.0f, 0.5f, 1.0f);
	else cColor = float4(0.0f, 0.0f, 0.0f, 1.0f);

	return(cColor);
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
        return float4(1, 0, 1, 1); // bad / missing

    // Depth �迭�� Load�� �а� x�� ���
    if (gvDrawOptions.x == 68 || gvDrawOptions.x == 90)
    {
        float d = gtxtGlobalTextures[idx].Load(uint3((uint) input.position.x, (uint) input.position.y, 0)).x;
        return float4(d, d, d, 1);
    }

    // �������� ���ø�
    return gtxtGlobalTextures[idx].Sample(gssDefaultSamplerState, input.uv);
}

///////////////////////////////////////////////////////////////////////////
// ����(�̿�)

struct VS_TERRAIN_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

struct VS_TERRAIN_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv0 : TEXCOORD0;
    float2 uv1 : TEXCOORD1;
};

VS_TERRAIN_OUTPUT VSTerrain(VS_TERRAIN_INPUT input)
{
    VS_TERRAIN_OUTPUT output;

    output.position = mul(mul(mul(float4(input.position, 1.0f), gmtxGameObject), gmtxView), gmtxProjection);
    output.color = input.color;
    output.uv0 = input.uv0;
    output.uv1 = input.uv1;

    return (output);
}

float4 PSTerrain(VS_TERRAIN_OUTPUT input) : SV_TARGET
{
    float4 cBaseTexColor = gtxtTerrainBaseTexture.Sample(gssDefaultSamplerState, input.uv0);
//	float fAlpha = gtxtTerrainAlphaTexture.Sample(gSamplerState, input.uv0);
    float fAlpha = gtxtTerrainAlphaTexture.Sample(gssDefaultSamplerState, input.uv0).w;

    float4 cDetailTexColors[3];
    cDetailTexColors[0] = gtxtGlobalTextures[0].Sample(gssDefaultSamplerState, input.uv1 * 2.0f);
    cDetailTexColors[1] = gtxtGlobalTextures[1].Sample(gssDefaultSamplerState, input.uv1 * 0.125f);
    cDetailTexColors[2] = gtxtGlobalTextures[2].Sample(gssDefaultSamplerState, input.uv1);

    float4 cColor = cBaseTexColor * cDetailTexColors[0];
    cColor += lerp(cDetailTexColors[1] * 0.25f, cDetailTexColors[2], 1.0f - fAlpha);
/* 
	cColor = lerp(cDetailTexColors[0], cDetailTexColors[2], 1.0f - fAlpha) ;
	cColor = lerp(cBaseTexColor, cColor, 0.3f) + cDetailTexColors[1] * (1.0f - fAlpha);
*/
/*
	if (fAlpha < 0.35f) cColor = cDetailTexColors[2];
	else if (fAlpha > 0.8975f) cColor = cDetailTexColors[0];
	else cColor = cDetailTexColors[1];
*/
    return (cColor);
}

struct VS_WATER_INPUT
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VS_WATER_OUTPUT
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VS_WATER_OUTPUT VSTerrainWater(VS_WATER_INPUT input)
{
    VS_WATER_OUTPUT output;

    output.position = mul(mul(mul(float4(input.position, 1.0f), gmtxGameObject), gmtxView), gmtxProjection);
    output.uv = input.uv;

    return (output);
}