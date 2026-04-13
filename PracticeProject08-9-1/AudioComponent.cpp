//-----------------------------------------------------------------------------
// File: AudioComponent.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "AudioComponent.h"
#include "AudioManager.h"
#include "Object.h"

#include <fmod.hpp>

CAudioComponent::CAudioComponent(CGameObject* owner)
	: CComponentT(owner)
{
}

CAudioComponent::~CAudioComponent()
{
	OnDestroy();
}

void CAudioComponent::SetDefaultSoundPath(const char* filePath)
{
	m_defaultSoundPath = filePath ? filePath : "";
}

void CAudioComponent::OnCreate(ID3D12Device* /*dev*/, ID3D12GraphicsCommandList* /*cmd*/)
{
	if ( m_autoPlay )
	{
		Play();
	}
}

void CAudioComponent::OnDestroy()
{
	Stop();
}

void CAudioComponent::OnUpdate(float /*dt*/)
{
	if ( !m_audioManager || !m_channel )
		return;

	if ( !m_audioManager->IsChannelPlaying(m_channel) )
	{
		m_channel = nullptr;
		return;
	}

	if ( m_is3D )
	{
		Update3DAttributes();
	}
}

bool CAudioComponent::Play()
{
	if ( !m_audioManager )
		return false;

	if ( m_defaultSoundPath.empty() )
		return false;

	Stop();

	if ( m_is3D )
	{
		m_channel = m_audioManager->PlaySound3D(
			m_defaultSoundPath.c_str(),
			m_pOwner ? m_pOwner->GetPosition() : XMFLOAT3(0.0f, 0.0f, 0.0f),
			m_loop,
			m_stream,
			m_volume,
			false
		);
	}
	else
	{
		m_channel = m_audioManager->PlaySound2D(
			m_defaultSoundPath.c_str(),
			m_loop,
			m_stream,
			m_volume,
			false
		);
	}

	if ( m_channel )
	{
		m_audioManager->SetChannelPitch(m_channel, m_pitch);

		if ( m_is3D )
			Update3DAttributes();

		return true;
	}

	return false;
}

bool CAudioComponent::PlayOneShot(const char* filePath)
{
	if ( !m_audioManager )
		return false;

	const char* path = filePath ? filePath : m_defaultSoundPath.c_str();
	if ( !path || !path[0] )
		return false;

	FMOD::Channel* tempChannel = nullptr;

	if ( m_is3D )
	{
		tempChannel = m_audioManager->PlaySound3D(
			path,
			m_pOwner ? m_pOwner->GetPosition() : XMFLOAT3(0.0f, 0.0f, 0.0f),
			false,
			false,
			m_volume,
			false
		);
	}
	else
	{
		tempChannel = m_audioManager->PlaySound2D(
			path,
			false,
			false,
			m_volume,
			false
		);
	}

	if ( tempChannel )
	{
		m_audioManager->SetChannelPitch(tempChannel, m_pitch);
		return true;
	}

	return false;
}

void CAudioComponent::Stop()
{
	if ( !m_audioManager )
	{
		m_channel = nullptr;
		return;
	}

	m_audioManager->StopChannel(m_channel);
}

void CAudioComponent::SetVolume(float volume)
{
	m_volume = volume;

	if ( m_audioManager && m_channel )
		m_audioManager->SetChannelVolume(m_channel, volume);
}

void CAudioComponent::SetPitch(float pitch)
{
	m_pitch = pitch;

	if ( m_audioManager && m_channel )
		m_audioManager->SetChannelPitch(m_channel, pitch);
}

bool CAudioComponent::IsPlaying() const
{
	if ( !m_audioManager || !m_channel )
		return false;

	return m_audioManager->IsChannelPlaying(m_channel);
}

void CAudioComponent::Update3DAttributes()
{
	if ( !m_audioManager || !m_channel || !m_pOwner )
		return;

	const XMFLOAT3 pos = m_pOwner->GetPosition();

	XMFLOAT3 vel(0.0f, 0.0f, 0.0f);
	if ( m_hasPrevPosition )
	{
		vel.x = pos.x - m_prevPosition.x;
		vel.y = pos.y - m_prevPosition.y;
		vel.z = pos.z - m_prevPosition.z;
	}

	m_audioManager->SetChannel3DAttributes(m_channel, pos, vel);

	m_prevPosition = pos;
	m_hasPrevPosition = true;
}