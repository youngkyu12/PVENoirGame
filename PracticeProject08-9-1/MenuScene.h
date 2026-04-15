#pragma once
#include "Scene.h"
#include "Shader.h"
#include "Texture.h"

class CMenuScene final : public CScene
{
public:
	CMenuScene() = default;
	~CMenuScene() override = default;

private:
	std::shared_ptr<CRectUIShader>      m_menuShader;
	std::shared_ptr<CTexture>           m_menuTex;
	UINT                                m_menuSrvIndex = UINT_MAX;

	std::shared_ptr<CTexture>           m_startButtonTex;
	UINT                                m_startButtonSrvIndex = UINT_MAX;

	std::shared_ptr<CTexture>           m_loadingTex;
	UINT                                m_loadingSrvIndex = UINT_MAX;

public:
	void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) override;
	void CreateMainCamera(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd, CGameObject* target) override;

	void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera) override;
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera = NULL) override;

	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam) override;
	bool ConsumeSceneRequest(ESceneRequest& outReq) override;

private:
	bool m_startGameRequested = false;
	bool m_showLoading = false;

private:
	XMFLOAT4 GetFitRect(const std::shared_ptr<CTexture>& tex, float centerX, float centerY, float maxW, float maxH) const;
	XMFLOAT4 GetFullscreenRect() const;
	XMFLOAT4 GetStartButtonRect() const;
	XMFLOAT4 GetLoadingRect() const;
	bool IsPointInRect(POINT pt, const XMFLOAT4& rect) const;
	void DrawUiTexture(ID3D12GraphicsCommandList* cmd, CCamera* camera, UINT srvIndex, const XMFLOAT4& rect);
};