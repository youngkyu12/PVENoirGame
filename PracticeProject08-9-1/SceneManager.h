//-----------------------------------------------------------------------------
// File: SceneManager.h
//-----------------------------------------------------------------------------

#pragma once
#include <memory>
#include <cstdint>

// forward
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

class CScene;
class CAudioManager;

enum class ESceneId : uint8_t
{
	Menu = 0,
	Wait = 1,
	Game = 2,
};

class CSceneManager
{
public:
    CSceneManager() = default;
    ~CSceneManager() = default;

	CScene* GetScene() const { return m_pScene.get(); }
	ESceneId GetSceneId() const { return m_sceneId; }

	void SetAudioManager(CAudioManager* audioManager) { m_pAudioManager = audioManager; }

	void ReleaseCurrent();

    // cmd는 Reset된 상태로 들어온다고 가정 (Framework에서 관리)
    void BuildScene(ESceneId id, ID3D12Device* dev, ID3D12GraphicsCommandList* cmd);

private:
	std::unique_ptr<CScene> m_pScene;
	ESceneId                m_sceneId = ESceneId::Menu;
	CAudioManager*			m_pAudioManager = nullptr;
};