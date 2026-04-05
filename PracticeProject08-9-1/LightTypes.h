//-----------------------------------------------------------------------------
// File: LightTypes.h
//-----------------------------------------------------------------------------
#pragma once
#include "stdafx.h"

// MAX_LIGHTS / POINT_LIGHT / SPOT_LIGHT / DIRECTIONAL_LIGHT 는
// 기존에 쓰던 곳(Shader.h 등)에 이미 정의돼 있다고 가정.

struct LIGHT
{
    XMFLOAT4 m_xmf4Ambient;
    XMFLOAT4 m_xmf4Diffuse;
    XMFLOAT4 m_xmf4Specular;
    XMFLOAT3 m_xmf3Position;
    float    m_fFalloff;
    XMFLOAT3 m_xmf3Direction;
    float    m_fTheta; // cos(theta)
    XMFLOAT3 m_xmf3Attenuation;
    float    m_fPhi;   // cos(phi)
	UINT     m_bEnable;
    int      m_nType;
    float    m_fRange;
    float    padding;
};

struct LIGHTS
{
	LIGHT    m_pLights[MAX_LIGHTS];
	XMFLOAT4 m_xmf4GlobalAmbient;
};
