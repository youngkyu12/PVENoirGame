//-----------------------------------------------------------------------------
// File: WaitScene.h
//-----------------------------------------------------------------------------
#pragma once

#include "Scene.h"
#include "SceneUI.h"

class CWaitScene final : public CScene
{
public:
	CWaitScene() = default;
	~CWaitScene() override = default;

private:
	CSceneUI m_waitUI;
	int m_waitBackgroundSpriteIndex = -1;
	std::array<int, 4> m_weaponSelectedSpriteIndices = { -1, -1, -1, -1 };
	std::array<int, 4> m_weaponFrameSpriteIndices = { -1, -1, -1, -1 };
	std::array<int, 4> m_weaponSpriteIndices = { -1, -1, -1, -1 };
	std::array<int, 4> m_playerMarkerSpriteIndices = { -1, -1, -1, -1 };
	std::array<int, 4> m_playerSelectedWeaponSlots = { -1, -1, -1, -1 };
	std::array<bool, 4> m_playerWeaponSelectionKnown = { false, false, false, false };
	int m_localPlayerIndex = 0;
	int m_hoveredWeaponSlot = -1;
	int m_startButtonSpriteIndex = -1;
	int m_loadingSpriteIndex = -1;
	bool m_isReady = false;

public:
	void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) override;
	void ReleaseObjects() override;
	void CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* target) override;
	void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera) override;
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera = NULL) override;
	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam) override;
	bool ConsumeSceneRequest(ESceneRequest& outReq) override;
	void OnResize(int width, int height) override;
	void SetLocalPlayerIndex(int playerIndex);
	void SetPlayerWeaponSelection(int playerIndex, int weaponSlot);

private:
	XMFLOAT4 GetStartButtonRect() const;
	XMFLOAT4 GetWeaponGridRect() const;
	XMFLOAT4 GetWeaponFrameRect(int frameSlot) const;
	XMFLOAT4 GetWeaponSpriteRect(int frameSlot) const;
	XMFLOAT4 GetWeaponSelectedRect(int frameSlot) const;
	XMFLOAT4 GetPlayerMarkerRect(int playerIndex, int weaponSlot) const;
	int GetWeaponSlotAtPoint(POINT ptClient) const;
	void UpdateHoveredWeaponSlot(POINT ptClient);
	void UpdatePlayerMarkerSpriteRect(int playerIndex);
	bool m_startGameRequested = false;
	bool m_showLoading = false;
};