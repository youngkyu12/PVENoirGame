//-----------------------------------------------------------------------------
// File: GameSceneHUD.h
//-----------------------------------------------------------------------------

#pragma once

#include "SceneUI.h"

class CCamera;

class CGameSceneHUD final
{
public:
	void ReleaseResources();

	void BuildResources(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		ID3D12RootSignature* rootSignature
	);

	void Render(ID3D12GraphicsCommandList* cmd, CCamera* camera);

	void SetInactiveOverlayVisible(bool visible);
	bool IsInactiveOverlayVisible() const { return m_inactiveOverlayVisible; }

	bool GetPauseOverlayRect(XMFLOAT4& outRect) const;
	bool IsPointInPauseOverlay(POINT clientPt) const;

private:
	CSceneUI m_ui;

	int m_pauseSpriteIndex = -1;
	bool m_inactiveOverlayVisible = false;
};