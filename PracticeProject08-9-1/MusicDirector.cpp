//-----------------------------------------------------------------------------
// File: MusicDirector.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "MusicDirector.h"
#include "AudioManager.h"

#include <fmod.hpp>

CMusicDirector::CMusicDirector()
{
}

CMusicDirector::~CMusicDirector()
{
	Shutdown();
}

void CMusicDirector::Initialize(CAudioManager* audioManager)
{
	m_audioManager = audioManager;
}

void CMusicDirector::Shutdown()
{
	if ( m_audioManager )
	{
		m_audioManager->StopChannel(m_nextChannel);
		m_audioManager->StopChannel(m_currentChannel);
	}

	m_musicFileTable.clear();

	m_currentState = EMusicState::None;
	m_requestedState = EMusicState::None;
	m_transitionElapsed = 0.0f;
	m_isCrossFading = false;
	m_hasPendingTransition = false;
	m_pendingImmediate = false;
	m_audioManager = nullptr;
}

void CMusicDirector::RegisterMusic(EMusicState state, const char* filePath)
{
	if ( !filePath || !filePath[0] )
		return;

	m_musicFileTable[state] = filePath;
}

void CMusicDirector::RequestState(EMusicState nextState, bool immediate)
{
	m_requestedState = nextState;
	m_pendingImmediate = immediate;
	m_hasPendingTransition = true;
}

void CMusicDirector::BeginPendingTransition()
{
	if ( !m_hasPendingTransition )
		return;

	m_hasPendingTransition = false;

	if ( m_requestedState == m_currentState && m_currentChannel && m_audioManager->IsChannelPlaying(m_currentChannel) )
		return;

	if ( m_pendingImmediate || !m_currentChannel )
	{
		StartImmediate(m_requestedState);
		return;
	}

	StartCrossFade(m_requestedState);
}

void CMusicDirector::StartImmediate(EMusicState state)
{
	if ( !m_audioManager )
		return;

	m_audioManager->StopChannel(m_nextChannel);
	m_audioManager->StopChannel(m_currentChannel);

	auto it = m_musicFileTable.find(state);
	if ( it == m_musicFileTable.end() )
	{
		m_currentState = EMusicState::None;
		return;
	}

	m_currentChannel = m_audioManager->PlaySound2D(
		it->second.c_str(),
		true,   // loop
		true,   // stream
		1.0f,
		false
	);

	if ( m_currentChannel )
	{
		FMOD::ChannelGroup* bgmGroup = m_audioManager->GetBgmGroup();
		if ( bgmGroup )
			m_currentChannel->setChannelGroup(bgmGroup);
	}

	m_currentState = state;
	m_transitionElapsed = 0.0f;
	m_isCrossFading = false;
}

void CMusicDirector::StartCrossFade(EMusicState state)
{
	if ( !m_audioManager )
		return;

	m_audioManager->StopChannel(m_nextChannel);

	auto it = m_musicFileTable.find(state);
	if ( it == m_musicFileTable.end() )
		return;

	m_nextChannel = m_audioManager->PlaySound2D(
		it->second.c_str(),
		true,   // loop
		true,   // stream
		0.0f,   // start at silent
		false
	);

	if ( m_nextChannel )
	{
		FMOD::ChannelGroup* bgmGroup = m_audioManager->GetBgmGroup();
		if ( bgmGroup )
			m_nextChannel->setChannelGroup(bgmGroup);
	}

	m_transitionElapsed = 0.0f;
	m_isCrossFading = ( m_nextChannel != nullptr );
}

void CMusicDirector::Update()
{
	if ( !m_audioManager )
		return;

	if ( m_hasPendingTransition )
		return;

	if ( !m_isCrossFading )
		return;

	// 프로젝트 타이머와 연결해서 바꾸면 더 좋다.
	const float dt = 1.0f / 60.0f;
	m_transitionElapsed += dt;

	float t = 1.0f;
	if ( m_crossFadeSeconds > 0.0f )
		t = std::clamp(m_transitionElapsed / m_crossFadeSeconds, 0.0f, 1.0f);

	if ( m_currentChannel )
		m_audioManager->SetChannelVolume(m_currentChannel, 1.0f - t);

	if ( m_nextChannel )
		m_audioManager->SetChannelVolume(m_nextChannel, t);

	if ( t >= 1.0f )
	{
		m_audioManager->StopChannel(m_currentChannel);
		m_currentChannel = m_nextChannel;
		m_nextChannel = nullptr;

		m_currentState = m_requestedState;
		m_transitionElapsed = 0.0f;
		m_isCrossFading = false;
	}
}