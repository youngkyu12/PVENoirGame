#include "stdafx.h"
#include "Camera.h"
#include "Player.h"

void CCamera::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	UINT ncbElementBytes = ((sizeof(VS_CB_CAMERA_INFO) + 255) & ~255); //256ÀÇ ¹è¼ö
	m_pd3dcbCamera = ::CreateBufferResource(
		pd3dDevice, 
		pd3dCommandList, 
		NULL, 
		ncbElementBytes, 
		D3D12_HEAP_TYPE_UPLOAD, 
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 
		NULL
	);
	m_pd3dcbCamera->Map(0, NULL, (void**)&m_pcbMappedCamera);
}