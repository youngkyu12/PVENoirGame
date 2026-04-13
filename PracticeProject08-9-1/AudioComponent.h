//-----------------------------------------------------------------------------
// File: AudioComponent.h
//-----------------------------------------------------------------------------

#pragma once

#include <string>

#include "Component.h"

class CAudioManager;

namespace FMOD
{
	class Channel;
}

class CAudioComponent final : public CComponentT<CAudioComponent>
{
public:
	explicit CAudioComponent(CGameObject* owner);
	~CAudioComponent() override;

public:
	void SetAudioManager(CAudioManager* audioManager) { m_audioManager = audioManager; }

	void SetDefaultSoundPath(const char* filePath);
	const std::string& GetDefaultSoundPath() const { return m_defaultSoundPath; }

	void SetAutoPlay(bool enable) { m_autoPlay = enable; }
	void SetLoop(bool enable) { m_loop = enable; }
	void Set3D(bool enable) { m_is3D = enable; }
	void SetStream(bool enable) { m_stream = enable; }

public:
	void OnCreate(ID3D12Device* dev, ID3D12GraphicsCommandList* cmd) override;
	void OnDestroy() override;
	void OnUpdate(float dt) override;

public:
	bool Play();
	bool PlayOneShot(const char* filePath = nullptr);
	void Stop();

	void SetVolume(float volume);
	void SetPitch(float pitch);

	bool IsPlaying() const;

private:
	void Update3DAttributes();

private:
	CAudioManager* m_audioManager = nullptr;

	std::string m_defaultSoundPath;
	FMOD::Channel* m_channel = nullptr;

	bool m_autoPlay = false;
	bool m_loop = false;
	bool m_is3D = true;
	bool m_stream = false;

	float m_volume = 1.0f;
	float m_pitch = 1.0f;

	XMFLOAT3 m_prevPosition = XMFLOAT3(0.0f, 0.0f, 0.0f);
	bool m_hasPrevPosition = false;
};