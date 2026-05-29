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
	
	void SetHealthRatio(float ratio);

	void SetInactiveOverlayVisible(bool visible);
	bool IsInactiveOverlayVisible() const { return m_inactiveOverlayVisible; }

	bool GetPauseOverlayRect(XMFLOAT4& outRect) const;
	bool GetResumeButtonRect(XMFLOAT4& outRect) const;
	bool GetExitButtonRect(XMFLOAT4& outRect) const;

	bool IsPointInPauseOverlay(POINT clientPt) const;
	bool IsPointInResumeButton(POINT clientPt) const;
	bool IsPointInExitButton(POINT clientPt) const;

private:
	CSceneUI m_ui;

	int m_pauseSpriteIndex = -1;
	int m_resumeSpriteIndex = -1;
	int m_exitSpriteIndex = -1;

	int m_hpFillSpriteIndex = -1;

	static constexpr int kInventorySlotCount = 5;
	std::array<int, kInventorySlotCount> m_inventorySpriteIndices = { -1, -1, -1, -1, -1 };

	XMFLOAT4 m_hpFillOriginalRect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	float m_healthRatio = 1.0f;

	bool m_inactiveOverlayVisible = false;
};