#include "stdafx.h"
#include "Mesh.h"

CMesh::CMesh(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	::ZeroMemory(&m_d3dVertexBufferView, sizeof(D3D12_VERTEX_BUFFER_VIEW));
}

CMesh::~CMesh()
{
}


