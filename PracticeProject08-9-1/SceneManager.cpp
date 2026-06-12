//-----------------------------------------------------------------------------
// File: SceneManager.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "SceneManager.h"

#include "Scene.h"
#include "MenuScene.h"
#include "WaitScene.h"
#include "GameScene.h"

#include "GlobalValues.h"

void CSceneManager::ReleaseCurrent()
{
    if (m_pScene)
    {
        m_pScene->ReleaseObjects();
        m_pScene.reset();
    }
}

void CSceneManager::BuildScene(ESceneId id, ID3D12Device* dev, ID3D12GraphicsCommandList* cmd)
{
	ReleaseCurrent();

	switch ( id )
	{
	case ESceneId::Menu:
		m_pScene = std::make_unique<CMenuScene>();
		break;
	case ESceneId::Wait:
		m_pScene = std::make_unique<CWaitScene>();
		break;
	case ESceneId::Game:
		m_pScene = std::make_unique<CGameScene>();
		break;
	default:
		m_pScene = std::make_unique<CMenuScene>();
		id = ESceneId::Menu;
		break;
	}

	m_sceneId = id;

	if ( m_pScene ) m_pScene->SetAudioManager(m_pAudioManager);

	m_pScene->BuildObjects(dev, cmd);
}