#include "Common.hlsl"
#include "MaterialTexture.hlsl"
#include "Lighting.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float4 TangentL : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;
	
    // Transform to world space.
    float4 posW = mul(float4(vin.PosL, 1.0f), gmtxGameObject);
    
    // Transform to homogeneous clip space.
    float4 posV = mul(posW, gmtxView);
    vout.PosH = mul(posV, gmtxProjection);
	
	// Output vertex attributes for interpolation across triangle.
    vout.TexC = vin.TexC;
	
    return vout;
}

// This is only used for alpha cut out geometry, so that shadows 
// show up correctly.  Geometry that does not need to sample a
// texture can use a NULL pixel shader for depth pass.
void PS(VertexOut pin)
{
	// Dynamically look up the texture in the array.
    float4 diffuseAlbedo = gtxtGlobalTextures[1].Sample(gsamLinearWrap, pin.TexC);


#ifdef ALPHA_TEST
    // Discard pixel if texture alpha < 0.1.  We do this test as soon 
    // as possible in the shader so that we can potentially exit the
    // shader early, thereby skipping the rest of the shader code.
    clip(diffuseAlbedo.a - 0.1f);
#endif
}
