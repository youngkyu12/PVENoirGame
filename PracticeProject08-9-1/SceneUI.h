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

		// 0: normal, 1: disabled/desaturated, 2: force white text, 3: solid rect, 4: poison edge overlay
		int effectKind = 0;

		XMFLOAT4 color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		XMFLOAT4 params0 = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	};

public:
	void ReleaseResources();

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
	int AddSolidRect(const char* name, const XMFLOAT4& rect, ELayer layer, bool visible, const XMFLOAT4& color, int effectKind, const XMFLOAT4& params0 = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));

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

	bool SetSpriteColor(int spriteIndex, const XMFLOAT4& color);
	bool SetSpriteParams0(int spriteIndex, const XMFLOAT4& params0);

	bool GetSpriteRect(int spriteIndex, XMFLOAT4& outRect) const;
	bool SetSpriteRect(int spriteIndex, const XMFLOAT4& rect);
	bool IsPointInSprite(int spriteIndex, POINT pt) const;

	const SpriteEntry* GetSprite(int spriteIndex) const;

	static XMFLOAT4 GetFullscreenRect();
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
};