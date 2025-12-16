#include "stdafx.h"
#include "Mesh.h"

CPolygon::CPolygon(int nVertices)
{
	m_nVertices = nVertices;
	m_pVertices = new CVertex[nVertices];
}

CPolygon::~CPolygon()
{
	if (m_pVertices) delete[] m_pVertices;
}

CMesh::CMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	::ZeroMemory(&m_d3dVertexBufferView, sizeof(D3D12_VERTEX_BUFFER_VIEW));
	::ZeroMemory(&m_d3dIndexBufferView, sizeof(D3D12_INDEX_BUFFER_VIEW));
}

void CMesh::ReleaseObjects()
{
	if (m_pd3dVertexBuffer) m_pd3dVertexBuffer.Reset();
	if (m_pd3dIndexBuffer) m_pd3dIndexBuffer.Reset();
}

void CMesh::ReleaseUploadBuffers()
{
	if (m_pd3dVertexUploadBuffer) 
		m_pd3dVertexUploadBuffer.Reset();

	if (m_pd3dIndexUploadBuffer) 
		m_pd3dIndexUploadBuffer.Reset();
}
