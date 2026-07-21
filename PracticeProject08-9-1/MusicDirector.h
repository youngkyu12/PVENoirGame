//-----------------------------------------------------------------------------
// File: MusicDirector.h
//-----------------------------------------------------------------------------

#pragma once

#include <string>
#include <unordered_map>
#include <cstdint>

#include "AudioManager.h"

namespace FMOD
{
	class Channel;
}

class CAudioManager;

class CMusicDirector
{
public:
	CMusicDirector();
	~CMusicDirector();

public:
	void Initialize(CAudioManager* audioManager);
	void Shutdown();

	// 매 프레임 호출
	void Update();

	// 음악 등록
	void RegisterMusic(EMusicState state, const char* filePath);
	void RegisterSeaLayerMusic(const char* filePath);

	// Scene이 호출
	void RequestState(EMusicState nextState, bool immediate = false);

	// "새 Scene 첫 렌더 후" 호출용
	void BeginPendingTransition();

public:
	EMusicState GetCurrentState() const { return m_currentState; }
	EMusicState GetRequestedState() const { return m_requestedState; }

	bool HasPendingTransition() const { return m_hasPendingTransition; }

	void SetCrossFadeSeconds(float sec) { m_crossFadeSeconds = ( sec < 0.0f ) ? 0.0f : sec; }
	void SetGameplaySeaBlend(float seaBlend);

private:
	void StartImmediate(EMusicState state);
	void StartCrossFade(EMusicState state);
	void StopSeaLayer();
	void UpdateSeaLayerVolumes(float dt);

private:
	CAudioManager* m_audioManager = nullptr;

	std::unordered_map<EMusicState, std::string> m_musicFileTable;
	std::string m_seaLayerFilePath;

	EMusicState m_currentState = EMusicState::None;
	EMusicState m_requestedState = EMusicState::None;

	FMOD::Channel* m_currentChannel = nullptr;
	FMOD::Channel* m_nextChannel = nullptr;
	FMOD::Channel* m_seaLayerChannel = nullptr;

	float m_crossFadeSeconds = 1.0f;
	float m_transitionElapsed = 0.0f;
	float m_seaLayerBlend = 0.0f;
	float m_seaLayerVolume = 0.0f;
	bool m_isCrossFading = false;

	bool m_hasPendingTransition = false;
	bool m_pendingImmediate = false;
};
