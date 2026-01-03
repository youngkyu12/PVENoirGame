#define MAX_GLOBAL_SRVS 1024

// Global Texture2D pool: t0 ~ t1023 in space0
Texture2D gtxtGlobalTextures[MAX_GLOBAL_SRVS] : register(t0);

cbuffer cbPlayerInfo : register(b0)
{
    matrix gmtxPlayerWorld : packoffset(c0);
    uint gnPlayerMaterialID : packoffset(c4.x);
    uint3 _padPlayer : packoffset(c4.y);
};


cbuffer cbCameraInfo : register(b1)
{
	matrix		gmtxView : packoffset(c0);
	matrix		gmtxProjection : packoffset(c4);
	float3		gvCameraPosition : packoffset(c8);
};

cbuffer cbGameObjectInfo : register(b2)
{
    matrix		gmtxGameObject : packoffset(c0);
    uint		gnObjectID     : packoffset(c4.x);
    uint		gnMaterialID   : packoffset(c4.y);
};

#include "Light.hlsl"

SamplerState gssDefaultSamplerState : register(s0);

cbuffer cbDrawOptions : register(b5)
{
    int4 gvDrawOptions; // x = 'T','L','N','D','Z'
    uint4 gvPostSrvIdx0; // x=T, y=L, z=N, w=D
    uint4 gvPostSrvIdx1; // x=Z, 나머지 패딩
};

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

float4 PSDiffused(VS_DIFFUSED_OUTPUT input): SV_TARGET
{
	return(input.color);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


struct VS_PLAYER_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    uint4 blendIndices : BLENDINDICES; // 지금은 미사용이어도 OK
    float4 blendWeights : BLENDWEIGHT; // 지금은 미사용이어도 OK
};

struct VS_PLAYER_OUTPUT
{
    float4 position : SV_POSITION;
    float3 positionW : POSITION;
    float3 normalW : NORMAL;
    float2 uv : TEXCOORD;
};

VS_PLAYER_OUTPUT VSPlayer(VS_PLAYER_INPUT input)
{
    VS_PLAYER_OUTPUT output;

    float4 posW = mul(float4(input.position, 1.0f), gmtxPlayerWorld);
    output.positionW = posW.xyz;
    output.position = mul(mul(float4(output.positionW, 1.0f), gmtxView), gmtxProjection);

    output.normalW = normalize(mul(input.normal, (float3x3) gmtxPlayerWorld));
    output.uv = input.uv;

    return output;
}

float4 PSPlayer(VS_PLAYER_OUTPUT input, uint nPrimitiveID : SV_PrimitiveID) : SV_TARGET
{
    uint diffuseIndex = gMaterials[gnPlayerMaterialID].TextureIndices.x;

    // 디버그 안전장치 (선택)
    if (diffuseIndex >= MAX_GLOBAL_SRVS)
        return float4(1, 0, 0, 1); // 잘못된 인덱스

    float4 cColor = gtxtGlobalTextures[diffuseIndex]
                        .Sample(gssDefaultSamplerState, input.uv);

    float4 cIllumination = Lighting(input.positionW, input.normalW);
    return cColor * cIllumination;
}




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
    uint diffuseIndex = gMaterials[gnMaterialID].TextureIndices.x; // 또는 gnPlayerMaterialID

    if (diffuseIndex == 0xFFFFFFFF || diffuseIndex >= MAX_GLOBAL_SRVS)
        return float4(1, 0, 1, 1); // 텍스처 없음/잘못된 인덱스
    
    return gtxtGlobalTextures[diffuseIndex].Sample(gssDefaultSamplerState, input.uv);

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
struct VS_TEXTURED_LIGHTING_INPUT
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
};

struct VS_TEXTURED_LIGHTING_OUTPUT
{
	float4 position : SV_POSITION;
	float3 positionW : POSITION;
	float3 normalW : NORMAL;
	float2 uv : TEXCOORD;
};

VS_TEXTURED_LIGHTING_OUTPUT VSTexturedLighting(VS_TEXTURED_LIGHTING_INPUT input)
{
	VS_TEXTURED_LIGHTING_OUTPUT output;

	output.normalW = mul(input.normal, (float3x3)gmtxGameObject);
	output.positionW = (float3)mul(float4(input.position, 1.0f), gmtxGameObject);
	output.position = mul(mul(float4(output.positionW, 1.0f), gmtxView), gmtxProjection);
	output.uv = input.uv;

	return(output);
}

float4 PSTexturedLighting(VS_TEXTURED_LIGHTING_OUTPUT input, uint nPrimitiveID : SV_PrimitiveID) : SV_TARGET
{
    uint diffuseIndex = gMaterials[gnMaterialID].TextureIndices.x;

    if (diffuseIndex >= MAX_GLOBAL_SRVS)
        return float4(1, 0, 0, 1); // bad index

    float4 cTexture = gtxtGlobalTextures[diffuseIndex].Sample(gssDefaultSamplerState, input.uv);

    input.normalW = normalize(input.normalW);
    float4 cIllumination = Lighting(input.positionW, input.normalW);

    return (cTexture * cIllumination);
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

PS_MULTIPLE_RENDER_TARGETS_OUTPUT PSTexturedLightingToMultipleRTs(VS_TEXTURED_LIGHTING_OUTPUT input, uint nPrimitiveID : SV_PrimitiveID)
{

    
    PS_MULTIPLE_RENDER_TARGETS_OUTPUT output;

    uint diffuseIndex = gMaterials[gnMaterialID].TextureIndices.x;
    if (diffuseIndex >= MAX_GLOBAL_SRVS)
    {
        output.color = float4(1, 0, 0, 1);
        output.cTexture = float4(1, 0, 0, 1);
        output.cIllumination = float4(0, 0, 0, 0);
        output.normal = float4(0, 0, 0, 1);
        output.zDepth = input.position.z;
        return output;
    }

    output.cTexture = gtxtGlobalTextures[diffuseIndex].Sample(gssDefaultSamplerState, input.uv);

    input.normalW = normalize(input.normalW);
    output.cIllumination = Lighting(input.positionW, input.normalW);

    output.color = output.cIllumination * output.cTexture;
    output.normal = float4(input.normalW.xyz * 0.5f + 0.5f, 1.0f);
    output.zDepth = input.position.z;
    
    /*
    if (gnMaterialID == 0)
        output.color = float4(0, 1, 0, 1); // 초록
    if (gnMaterialID == 1)
        output.color = float4(0, 0, 1, 1); // 파랑
    */
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

    // Depth 계열은 Load로 읽고 x만 사용
    if (gvDrawOptions.x == 68 || gvDrawOptions.x == 90)
    {
        float d = gtxtGlobalTextures[idx].Load(uint3((uint) input.position.x, (uint) input.position.y, 0)).x;
        return float4(d, d, d, 1);
    }

    // 나머지는 샘플링
    return gtxtGlobalTextures[idx].Sample(gssDefaultSamplerState, input.uv);
}
