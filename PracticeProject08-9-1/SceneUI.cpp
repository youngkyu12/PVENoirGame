//-----------------------------------------------------------------------------
// File: SceneUI.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "SceneUI.h"

#include "Scene.h"
#include "Camera.h"
#include "GlobalValues.h"

#include <algorithm>

void CSceneUI::ReleaseResources()
{
	if ( m_shader )
		m_shader->ReleaseShaderVariables();

	m_shader.reset();
	m_sprites.clear();

	for ( bool& visible : m_layerVisible )
		visible = true;
}

void CSceneUI::BuildShader(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	ID3D12RootSignature* rootSignature)
{
	ReleaseResources();

	if ( !dev || !cmd || !rootSignature )
		return;

	m_shader = std::make_shared<CRectUIShader>();

	DXGI_FORMAT rtv = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT dsv = DXGI_FORMAT_UNKNOWN;

	m_shader->CreateShader(
		dev,
		rootSignature,
		1,
		&rtv,
		dsv
	);

	m_shader->CreateShaderVariables(dev, cmd);
}

int CSceneUI::AddSprite(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	const char* name,
	const wchar_t* texturePath,
	const XMFLOAT4& rect,
	ELayer layer,
	bool visible)
{
	if ( !dev || !cmd || !texturePath )
		return -1;

	SpriteEntry entry{};
	if ( name )
		entry.name = name;

	entry.rect = rect;
	entry.layer = layer;
	entry.visible = visible;

	entry.texture = std::make_shared<CTexture>(1, RESOURCE_TEXTURE2D, 0, 0);
	entry.texture->LoadTextureFromFile(
		dev,
		cmd,
		texturePath,
		RESOURCE_TEXTURE2D,
		0
	);

	if ( !CScene::m_pDescriptorHeap )
		return -1;

	CScene::m_pDescriptorHeap->CreateShaderResourceViewsOther(
		dev,
		entry.texture.get(),
		ROOT_PARAMETER_GLOBAL_SRV
	);

	entry.srvIndex = entry.texture->GetSrvIndex(0);

	if ( entry.rect.z <= 0.0f )
		entry.rect.z = static_cast< float >( entry.texture->GetTextureWidth(0) );

	if ( entry.rect.w <= 0.0f )
		entry.rect.w = static_cast< float >( entry.texture->GetTextureHeight(0) );

	m_sprites.push_back(std::move(entry));
	return static_cast< int >( m_sprites.size() - 1 );
}

int CSceneUI::AddSolidRect(const char* name, const XMFLOAT4& rect, ELayer layer, bool visible)
{
	return AddSolidRect(name, rect, layer, visible, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.55f), 3, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f));
}

int CSceneUI::AddSolidRect(const char* name, const XMFLOAT4& rect, ELayer layer, bool visible, const XMFLOAT4& color, int effectKind, const XMFLOAT4& params0)
{
	SpriteEntry entry{};

	if ( name )
		entry.name = name;

	entry.texture = nullptr;
	entry.srvIndex = UINT_MAX;
	entry.rect = rect;
	entry.layer = layer;
	entry.visible = visible;
	entry.effectKind = effectKind;
	entry.color = color;
	entry.params0 = params0;

	m_sprites.push_back(std::move(entry));
	return static_cast< int >( m_sprites.size() - 1 );
}

int CSceneUI::AddFitSprite(
	ID3D12Device* dev,
	ID3D12GraphicsCommandList* cmd,
	const char* name,
	const wchar_t* texturePath,
	float centerX,
	float centerY,
	float maxW,
	float maxH,
	ELayer layer,
	bool visible)
{
	const int spriteIndex = AddSprite(
		dev,
		cmd,
		name,
		texturePath,
		XMFLOAT4(centerX, centerY, 0.0f, 0.0f),
		layer,
		visible
	);

	if ( !IsValidSpriteIndex(spriteIndex) )
		return spriteIndex;

	SpriteEntry& sprite = m_sprites[static_cast< size_t >( spriteIndex )];
	sprite.rect = MakeFitRect(sprite.texture, centerX, centerY, maxW, maxH);

	return spriteIndex;
}

void CSceneUI::RenderAll(ID3D12GraphicsCommandList* cmd, CCamera* camera)
{
	if ( !cmd || !m_shader )
		return;

	m_shader->ResetDrawOptionWriteIndex();

	for ( int layerIndex = 0; layerIndex < static_cast< int >(ELayer::Count); ++layerIndex )
	{
		const ELayer layer = static_cast< ELayer >(layerIndex);

		if ( !m_layerVisible[static_cast< size_t >(layer)] )
			continue;

		for ( int spriteIndex = 0; spriteIndex < static_cast< int >(m_sprites.size()); ++spriteIndex )
		{
			const SpriteEntry& sprite = m_sprites[static_cast< size_t >(spriteIndex)];

			if ( sprite.layer != layer )
				continue;

			if ( !sprite.visible )
				continue;

			RenderSprite(cmd, camera, spriteIndex);
		}
	}
}

void CSceneUI::RenderSprite(ID3D12GraphicsCommandList* cmd, CCamera* camera, int spriteIndex)
{
	if ( !cmd || !m_shader )
		return;

	if ( !IsValidSpriteIndex(spriteIndex) )
		return;

	const SpriteEntry& sprite = m_sprites[static_cast< size_t >( spriteIndex )];

	if ( !sprite.visible )
		return;

	const bool solidRect = ( sprite.effectKind == 3 || sprite.effectKind == 4 );

	if ( !solidRect && sprite.srvIndex == UINT_MAX )
		return;

	PS_CB_DRAW_OPTIONS opt{};
	opt.m_xmn4DrawOptions = XMINT4(solidRect ? 'S' : 'T', sprite.effectKind, 0, 0);
	opt.m_xmu4PostSrvIdx0 = XMUINT4(solidRect ? 0 : sprite.srvIndex, 0, 0, 0);
	opt.m_xmu4PostSrvIdx1 = XMUINT4(0, 0, 0, 0);
	opt.m_xmf4UiRect = sprite.rect;
	opt.m_xmf4Viewport = XMFLOAT4(static_cast< float >( FRAME_BUFFER_WIDTH ), static_cast< float >( FRAME_BUFFER_HEIGHT ), 1.0f / static_cast< float >( FRAME_BUFFER_WIDTH ), 1.0f / static_cast< float >( FRAME_BUFFER_HEIGHT ));
	opt.m_xmf4Color = sprite.color;
	opt.m_xmf4Params0 = sprite.params0;

	m_shader->Render(cmd, camera, &opt);
}

void CSceneUI::SetSpriteVisible(int spriteIndex, bool visible)
{
	if ( !IsValidSpriteIndex(spriteIndex) )
		return;

	m_sprites[static_cast< size_t >( spriteIndex )].visible = visible;
}

void CSceneUI::SetLayerVisible(ELayer layer, bool visible)
{
	const size_t index = static_cast< size_t >( layer );

	if ( index >= m_layerVisible.size() )
		return;

	m_layerVisible[index] = visible;
}

bool CSceneUI::SetSpriteEffectKind(int spriteIndex, int effectKind)
{
	if ( !IsValidSpriteIndex(spriteIndex) )
		return false;

	if ( effectKind < 0 )
		effectKind = 0;

	m_sprites[static_cast< size_t >(spriteIndex)].effectKind = effectKind;
	return true;
}

bool CSceneUI::SetSpriteColor(int spriteIndex, const XMFLOAT4& color)
{
	if ( !IsValidSpriteIndex(spriteIndex) )
		return false;

	m_sprites[static_cast< size_t >( spriteIndex )].color = color;
	return true;
}

bool CSceneUI::SetSpriteParams0(int spriteIndex, const XMFLOAT4& params0)
{
	if ( !IsValidSpriteIndex(spriteIndex) )
		return false;

	m_sprites[static_cast< size_t >( spriteIndex )].params0 = params0;
	return true;
}

bool CSceneUI::GetSpriteRect(int spriteIndex, XMFLOAT4& outRect) const
{
	if ( !IsValidSpriteIndex(spriteIndex) )
		return false;

	const SpriteEntry& sprite = m_sprites[static_cast< size_t >( spriteIndex )];

	if ( !sprite.texture || sprite.srvIndex == UINT_MAX )
		return false;

	outRect = sprite.rect;
	return true;
}

bool CSceneUI::SetSpriteRect(int spriteIndex, const XMFLOAT4& rect)
{
	if ( spriteIndex < 0 )
		return false;

	if ( spriteIndex >= static_cast< int >(m_sprites.size()) )
		return false;

	m_sprites[static_cast< size_t >(spriteIndex)].rect = rect;
	return true;
}

bool CSceneUI::IsPointInSprite(int spriteIndex, POINT pt) const
{
	XMFLOAT4 rect{};
	if ( !GetSpriteRect(spriteIndex, rect) )
		return false;

	return IsPointInRect(pt, rect);
}

const CSceneUI::SpriteEntry* CSceneUI::GetSprite(int spriteIndex) const
{
	if ( !IsValidSpriteIndex(spriteIndex) )
		return nullptr;

	return &m_sprites[static_cast< size_t >( spriteIndex )];
}

XMFLOAT4 CSceneUI::GetFullscreenRect()
{
	return XMFLOAT4(
		FRAME_BUFFER_WIDTH * 0.5f,
		FRAME_BUFFER_HEIGHT * 0.5f,
		static_cast< float >( FRAME_BUFFER_WIDTH ),
		static_cast< float >( FRAME_BUFFER_HEIGHT )
	);
}

XMFLOAT4 CSceneUI::MakeFitRect(
	const std::shared_ptr<CTexture>& texture,
	float centerX,
	float centerY,
	float maxW,
	float maxH)
{
	float w = texture ? static_cast< float >( texture->GetTextureWidth(0) ) : 0.0f;
	float h = texture ? static_cast< float >( texture->GetTextureHeight(0) ) : 0.0f;

	if ( w <= 0.0f || h <= 0.0f )
	{
		w = 256.0f;
		h = 128.0f;
	}

	float scale = 1.0f;

	if ( w > maxW )
	{
		const float candidate = maxW / w;
		if ( candidate < scale )
			scale = candidate;
	}

	if ( h > maxH )
	{
		const float candidate = maxH / h;
		if ( candidate < scale )
			scale = candidate;
	}

	return XMFLOAT4(centerX, centerY, w * scale, h * scale);
}

bool CSceneUI::IsPointInRect(POINT pt, const XMFLOAT4& rect)
{
	const float left = rect.x - rect.z * 0.5f;
	const float right = rect.x + rect.z * 0.5f;
	const float top = rect.y - rect.w * 0.5f;
	const float bottom = rect.y + rect.w * 0.5f;

	return
		( pt.x >= left && pt.x <= right ) &&
		( pt.y >= top && pt.y <= bottom );
}

bool CSceneUI::IsValidSpriteIndex(int spriteIndex) const
{
	return
		( spriteIndex >= 0 ) &&
		( spriteIndex < static_cast< int >(m_sprites.size()) );
}