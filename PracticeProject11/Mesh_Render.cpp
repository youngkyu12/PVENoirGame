#include "stdafx.h"
#include "Mesh.h"

void CMesh::Render(ID3D12GraphicsCommandList* cmd)
{
    cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (auto& sm : m_SubMeshes)
    {
        // VB / IB
        cmd->IASetVertexBuffers(0, 1, &sm.vbView);
        cmd->IASetIndexBuffer(&sm.ibView);

        // Draw
        cmd->DrawIndexedInstanced(
            static_cast<UINT>(sm.indices.size()),
            1, 0, 0, 0
        );
    }
}
