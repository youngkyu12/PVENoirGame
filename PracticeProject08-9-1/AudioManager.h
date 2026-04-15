//-----------------------------------------------------------------------------
// File: AudioManager.h
//-----------------------------------------------------------------------------

#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "stdafx.h"

class CMusicDirector;

namespace FMOD
{
	class System;
	class Sound;
	class Channel;
	class ChannelGroup;
}

enum class EMusicState : uint8_t
{
	None = 0,
	Menu,
	Gameplay,
	Boss,
	Pause
};

class CAudioManager
{
public:
	CAudioManager();
	~CAudioManager();

public:
	bool Initialize(int maxChannels = 256);
	void Shutdown();
	void Update();

	bool IsInitialized() const { return m_initialized; }

public:
	CMusicDirector* GetMusicDirector() const { return m_musicDirector.get(); }

public:
	// Sound cache
	FMOD::Sound* LoadSound(
		const char* filePath,
		bool is3D,
		bool loop,
		bool stream
	);

	void UnloadAllSounds();

public:
	// 2D / 3D play helpers
	FMOD::Channel* PlaySound2D(
		const char* filePath,
		bool loop = false,
		bool stream = false,
		float volume = 1.0f,
		bool startPaused = false
	);

	FMOD::Channel* PlaySound3D(
		const char* filePath,
		const XMFLOAT3& worldPos,
		bool loop = false,
		bool stream = false,
		float volume = 1.0f,
		bool startPaused = false
	);

public:
	// channel helpers
	void StopChannel(FMOD::Channel*& channel);
	void SetChannelPaused(FMOD::Channel* channel, bool paused);
	void SetChannelVolume(FMOD::Channel* channel, float volume);
	void SetChannelPitch(FMOD::Channel* channel, float pitch);

	void SetChannel3DAttributes(
		FMOD::Channel* channel,
		const XMFLOAT3& pos,
		const XMFLOAT3& vel = XMFLOAT3(0.0f, 0.0f, 0.0f)
	);

	bool IsChannelPlaying(FMOD::Channel* channel) const;

public:
	// listener
	void SetListenerAttributes(
		const XMFLOAT3& pos,
		const XMFLOAT3& vel,
		const XMFLOAT3& forward,
		const XMFLOAT3& up
	);

public:
	// groups
	FMOD::ChannelGroup* GetMasterGroup() const { return m_masterGroup; }
	FMOD::ChannelGroup* GetBgmGroup() const { return m_bgmGroup; }
	FMOD::ChannelGroup* GetSfxGroup() const { return m_sfxGroup; }

	void SetGroupVolume(FMOD::ChannelGroup* group, float volume);
	void StopGroup(FMOD::ChannelGroup* group);

private:
	FMOD::Channel* PlaySoundInternal(
		const char* filePath,
		bool is3D,
		const XMFLOAT3* worldPos,
		bool loop,
		bool stream,
		float volume,
		bool startPaused,
		FMOD::ChannelGroup* group
	);

	std::string BuildCacheKey(
		const char* filePath,
		bool is3D,
		bool loop,
		bool stream
	) const;

private:
	bool m_initialized = false;

	FMOD::System* m_system = nullptr;

	FMOD::ChannelGroup* m_masterGroup = nullptr;
	FMOD::ChannelGroup* m_bgmGroup = nullptr;
	FMOD::ChannelGroup* m_sfxGroup = nullptr;

	std::unordered_map<std::string, FMOD::Sound*> m_soundCache;

	std::unique_ptr<CMusicDirector> m_musicDirector;
};