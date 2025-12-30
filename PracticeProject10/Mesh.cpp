#include "stdafx.h"
#include "Mesh.h"

CMesh::CMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	::ZeroMemory(&m_d3dVertexBufferView, sizeof(D3D12_VERTEX_BUFFER_VIEW));
}

void CMesh::ReleaseObjects()
{
	if (m_pd3dVertexBuffer) m_pd3dVertexBuffer.Reset();
	if (m_pd3dIndexBuffer) m_pd3dIndexBuffer.Reset();
}

void CMesh::ReleaseUploadBuffers()
{
	// 정점 버퍼를 위한 업로드 버퍼를 소멸시킨다.
	if (m_pd3dVertexUploadBuffer) m_pd3dVertexUploadBuffer.Reset();

	// 인덱스 버퍼를 위한 업로드 버퍼를 소멸시킨다.
	if (m_pd3dIndexUploadBuffer) m_pd3dIndexUploadBuffer.Reset();
}
