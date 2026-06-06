//-----------------------------------------------------------------------------
// File: SceneUI.h
//-----------------------------------------------------------------------------

#pragma once

#include "Shader.h"
#include "Texture.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class CCamera;

class CSceneUI final
{
public:
	enum class ELayer : uint8_t
	{
		Background = 0,
		Frame = 1,
		Content = 2,
		Pause = 3,
		Count = 4
	};

	struct SpriteEntry
	{
		std::string name;
		std::shared_ptr<CTexture> texture;
		UINT srvIndex = UINT_MAX;

		// x=centerX, y=centerY, z=width, w=height
		XMFLOAT4 rect = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

		ELayer layer = ELayer::Content;
		bool visible = true;

		// 0: normal, 1: disabled/desaturated, 2: force white text
		int effectKind = 0;
	};

public:
	void ReleaseResources();
	void OnResize(int width, int height);

	void BuildShader(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		ID3D12RootSignature* rootSignature
	);

	int AddSprite(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		const char* name,
		const wchar_t* texturePath,
		const XMFLOAT4& rect,
		ELayer layer,
		bool visible = true
	);
	int AddSolidRect(const char* name, const XMFLOAT4& rect, ELayer layer, bool visible = true);

	int AddFitSprite(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		const char* name,
		const wchar_t* texturePath,
		float centerX,
		float centerY,
		float maxW,
		float maxH,
		ELayer layer,
		bool visible = true
	);

	void RenderAll(ID3D12GraphicsCommandList* cmd, CCamera* camera);
	void RenderSprite(ID3D12GraphicsCommandList* cmd, CCamera* camera, int spriteIndex);

	void SetSpriteVisible(int spriteIndex, bool visible);
	void SetLayerVisible(ELayer layer, bool visible);
	bool SetSpriteEffectKind(int spriteIndex, int effectKind);

	bool GetSpriteRect(int spriteIndex, XMFLOAT4& outRect) const;
	bool SetSpriteRect(int spriteIndex, const XMFLOAT4& rect);
	bool IsPointInSprite(int spriteIndex, POINT pt) const;

	const SpriteEntry* GetSprite(int spriteIndex) const;

	static XMFLOAT4 GetFullscreenRect(int width, int height);
	static XMFLOAT4 MakeFitRect(
		const std::shared_ptr<CTexture>& texture,
		float centerX,
		float centerY,
		float maxW,
		float maxH
	);

	static bool IsPointInRect(POINT pt, const XMFLOAT4& rect);

private:
	bool IsValidSpriteIndex(int spriteIndex) const;

private:
	std::shared_ptr<CRectUIShader> m_shader;
	std::vector<SpriteEntry> m_sprites;

	std::array<bool, static_cast< size_t >(ELayer::Count)> m_layerVisible =
	{
		true,
		true,
		true,
		true
	};
	float m_screenWidth = FRAME_BUFFER_WIDTH;
	float m_screenHeight = FRAME_BUFFER_HEIGHT;
};