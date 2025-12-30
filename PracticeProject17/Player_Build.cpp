#include "stdafx.h"
#include "Player.h"
#include "Shader.h"

void CPlayer::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (m_pCamera) 
		m_pCamera->CreateShaderVariables(pd3dDevice, pd3dCommandList);
}
