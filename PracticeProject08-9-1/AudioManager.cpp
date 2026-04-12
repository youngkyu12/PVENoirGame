//-----------------------------------------------------------------------------
// File: AudioManager.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "AudioManager.h"
#include "MusicDirector.h"

#include <fmod.hpp>
#include <fmod_errors.h>

namespace
{
	inline void LogFmodError(FMOD_RESULT result, const char* context)
	{
		if ( result == FMOD_OK ) return;

		char buffer[512] = {};
		sprintf_s(buffer, "[FMOD] %s failed: %s\n", context, FMOD_ErrorString(result));
		OutputDebugStringA(buffer);
	}

	inline FMOD_VECTOR ToFmodVector(const XMFLOAT3& v)
	{
		FMOD_VECTOR out{};
		out.x = v.x;
		out.y = v.y;
		out.z = v.z;
		return out;
	}
}

CAudioManager::CAudioManager()
{
}

CAudioManager::~CAudioManager()
{
	Shutdown();
}

bool CAudioManager::Initialize(int maxChannels)
{
	if ( m_initialized )
		return true;

	FMOD_RESULT fr = FMOD::System_Create(&m_system);
	if ( fr != FMOD_OK || !m_system )
	{
		LogFmodError(fr, "FMOD::System_Create");
		return false;
	}

	fr = m_system->init(maxChannels, FMOD_INIT_NORMAL | FMOD_INIT_3D_RIGHTHANDED, nullptr);
	if ( fr != FMOD_OK )
	{
		LogFmodError(fr, "System::init");
		return false;
	}

	fr = m_system->set3DSettings(1.0f, 1.0f, 1.0f);
	LogFmodError(fr, "System::set3DSettings");

	fr = m_system->getMasterChannelGroup(&m_masterGroup);
	LogFmodError(fr, "System::getMasterChannelGroup");

	if ( m_masterGroup )
	{
		fr = m_system->createChannelGroup("BGM", &m_bgmGroup);
		LogFmodError(fr, "System::createChannelGroup(BGM)");

		fr = m_system->createChannelGroup("SFX", &m_sfxGroup);
		LogFmodError(fr, "System::createChannelGroup(SFX)");

		if ( m_bgmGroup )
		{
			fr = m_masterGroup->addGroup(m_bgmGroup);
			LogFmodError(fr, "MasterGroup::addGroup(BGM)");
		}

		if ( m_sfxGroup )
		{
			fr = m_masterGroup->addGroup(m_sfxGroup);
			LogFmodError(fr, "MasterGroup::addGroup(SFX)");
		}
	}

	m_musicDirector = std::make_unique<CMusicDirector>();
	m_musicDirector->Initialize(this);

	m_initialized = true;
	return true;
}

void CAudioManager::Shutdown()
{
	if ( m_musicDirector )
	{
		m_musicDirector->Shutdown();
		m_musicDirector.reset();
	}

	UnloadAllSounds();

	if ( m_bgmGroup )
	{
		m_bgmGroup->release();
		m_bgmGroup = nullptr;
	}

	if ( m_sfxGroup )
	{
		m_sfxGroup->release();
		m_sfxGroup = nullptr;
	}

	m_masterGroup = nullptr;

	if ( m_system )
	{
		m_system->close();
		m_system->release();
		m_system = nullptr;
	}

	m_initialized = false;
}

void CAudioManager::Update()
{
	if ( !m_system )
		return;

	if ( m_musicDirector )
		m_musicDirector->Update();

	FMOD_RESULT fr = m_system->update();
	LogFmodError(fr, "System::update");
}

std::string CAudioManager::BuildCacheKey(
	const char* filePath,
	bool is3D,
	bool loop,
	bool stream
) const
{
	std::string key = filePath ? filePath : "";
	key += is3D ? "|3D" : "|2D";
	key += loop ? "|Loop" : "|OneShot";
	key += stream ? "|Stream" : "|Sample";
	return key;
}

FMOD::Sound* CAudioManager::LoadSound(
	const char* filePath,
	bool is3D,
	bool loop,
	bool stream
)
{
	if ( !m_system || !filePath || !filePath[0] )
		return nullptr;

	const std::string key = BuildCacheKey(filePath, is3D, loop, stream);

	auto it = m_soundCache.find(key);
	if ( it != m_soundCache.end() )
		return it->second;

	FMOD_MODE mode = FMOD_DEFAULT;

	mode |= is3D ? FMOD_3D : FMOD_2D;
	mode |= loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
	mode |= stream ? FMOD_CREATESTREAM : FMOD_CREATECOMPRESSEDSAMPLE;

	FMOD::Sound* sound = nullptr;
	FMOD_RESULT fr = m_system->createSound(filePath, mode, nullptr, &sound);
	if ( fr != FMOD_OK || !sound )
	{
		LogFmodError(fr, "System::createSound");
		return nullptr;
	}

	m_soundCache.emplace(key, sound);
	return sound;
}

void CAudioManager::UnloadAllSounds()
{
	for ( auto& pair : m_soundCache )
	{
		if ( pair.second )
			pair.second->release();
	}
	m_soundCache.clear();
}

FMOD::Channel* CAudioManager::PlaySoundInternal(
	const char* filePath,
	bool is3D,
	const XMFLOAT3* worldPos,
	bool loop,
	bool stream,
	float volume,
	bool startPaused,
	FMOD::ChannelGroup* group
)
{
	if ( !m_system || !filePath || !filePath[0] )
		return nullptr;

	FMOD::Sound* sound = LoadSound(filePath, is3D, loop, stream);
	if ( !sound )
		return nullptr;

	FMOD::Channel* channel = nullptr;
	FMOD_RESULT fr = m_system->playSound(sound, group, true, &channel);
	if ( fr != FMOD_OK || !channel )
	{
		LogFmodError(fr, "System::playSound");
		return nullptr;
	}

	if ( is3D && worldPos )
	{
		SetChannel3DAttributes(channel, *worldPos);
	}

	SetChannelVolume(channel, volume);
	SetChannelPaused(channel, startPaused);

	if ( !startPaused )
	{
		fr = channel->setPaused(false);
		LogFmodError(fr, "Channel::setPaused(false)");
	}

	return channel;
}

FMOD::Channel* CAudioManager::PlaySound2D(
	const char* filePath,
	bool loop,
	bool stream,
	float volume,
	bool startPaused
)
{
	return PlaySoundInternal(
		filePath,
		false,
		nullptr,
		loop,
		stream,
		volume,
		startPaused,
		m_sfxGroup
	);
}

FMOD::Channel* CAudioManager::PlaySound3D(
	const char* filePath,
	const XMFLOAT3& worldPos,
	bool loop,
	bool stream,
	float volume,
	bool startPaused
)
{
	return PlaySoundInternal(
		filePath,
		true,
		&worldPos,
		loop,
		stream,
		volume,
		startPaused,
		m_sfxGroup
	);
}

void CAudioManager::StopChannel(FMOD::Channel*& channel)
{
	if ( !channel )
		return;

	FMOD_RESULT fr = channel->stop();
	LogFmodError(fr, "Channel::stop");
	channel = nullptr;
}

void CAudioManager::SetChannelPaused(FMOD::Channel* channel, bool paused)
{
	if ( !channel ) return;

	FMOD_RESULT fr = channel->setPaused(paused);
	LogFmodError(fr, "Channel::setPaused");
}

void CAudioManager::SetChannelVolume(FMOD::Channel* channel, float volume)
{
	if ( !channel ) return;

	FMOD_RESULT fr = channel->setVolume(volume);
	LogFmodError(fr, "Channel::setVolume");
}

void CAudioManager::SetChannelPitch(FMOD::Channel* channel, float pitch)
{
	if ( !channel ) return;

	FMOD_RESULT fr = channel->setPitch(pitch);
	LogFmodError(fr, "Channel::setPitch");
}

void CAudioManager::SetChannel3DAttributes(
	FMOD::Channel* channel,
	const XMFLOAT3& pos,
	const XMFLOAT3& vel
)
{
	if ( !channel ) return;

	FMOD_VECTOR fpos = ToFmodVector(pos);
	FMOD_VECTOR fvel = ToFmodVector(vel);

	FMOD_RESULT fr = channel->set3DAttributes(&fpos, &fvel);
	LogFmodError(fr, "Channel::set3DAttributes");
}

bool CAudioManager::IsChannelPlaying(FMOD::Channel* channel) const
{
	if ( !channel )
		return false;

	bool playing = false;
	FMOD_RESULT fr = channel->isPlaying(&playing);
	if ( fr != FMOD_OK )
		return false;

	return playing;
}

void CAudioManager::SetListenerAttributes(
	const XMFLOAT3& pos,
	const XMFLOAT3& vel,
	const XMFLOAT3& forward,
	const XMFLOAT3& up
)
{
	if ( !m_system )
		return;

	FMOD_VECTOR fpos = ToFmodVector(pos);
	FMOD_VECTOR fvel = ToFmodVector(vel);
	FMOD_VECTOR ffwd = ToFmodVector(forward);
	FMOD_VECTOR fup = ToFmodVector(up);

	FMOD_RESULT fr = m_system->set3DListenerAttributes(0, &fpos, &fvel, &ffwd, &fup);
	LogFmodError(fr, "System::set3DListenerAttributes");
}

void CAudioManager::SetGroupVolume(FMOD::ChannelGroup* group, float volume)
{
	if ( !group ) return;

	FMOD_RESULT fr = group->setVolume(volume);
	LogFmodError(fr, "ChannelGroup::setVolume");
}

void CAudioManager::StopGroup(FMOD::ChannelGroup* group)
{
	if ( !group ) return;

	FMOD_RESULT fr = group->stop();
	LogFmodError(fr, "ChannelGroup::stop");
}