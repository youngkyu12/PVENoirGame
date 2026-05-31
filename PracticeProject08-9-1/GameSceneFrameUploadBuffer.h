//-----------------------------------------------------------------------------
// File: GameSceneFrameUploadBuffer.h
//-----------------------------------------------------------------------------

#pragma once

#include <array>
#include <utility>

#include <d3d12.h>
#include <wrl/client.h>

template <typename TVertex, UINT FrameCount>
class FrameUploadVertexBuffer
{
public:
	FrameUploadVertexBuffer() = default;
	~FrameUploadVertexBuffer()
	{
		Release();
	}

	FrameUploadVertexBuffer(const FrameUploadVertexBuffer&) = delete;
	FrameUploadVertexBuffer& operator=(const FrameUploadVertexBuffer&) = delete;

	FrameUploadVertexBuffer(FrameUploadVertexBuffer&&) = delete;
	FrameUploadVertexBuffer& operator=(FrameUploadVertexBuffer&&) = delete;

	template <typename CreateResourceFunc>
	bool Create(
		ID3D12Device* dev,
		ID3D12GraphicsCommandList* cmd,
		UINT capacity,
		CreateResourceFunc&& createResource)
	{
		Release();

		if ( !dev || !cmd )
			return false;

		if ( capacity == 0 )
			return false;

		m_capacity = capacity;

		const UINT bufferBytes =
			static_cast< UINT >(
				sizeof(TVertex) * static_cast< size_t >( capacity )
			);

		for ( UINT frameIndex = 0; frameIndex < FrameCount; ++frameIndex )
		{
			m_resources[frameIndex] =
				std::forward<CreateResourceFunc>(createResource)( bufferBytes );

			if ( !m_resources[frameIndex] )
			{
				Release();
				return false;
			}

			HRESULT hr =
				m_resources[frameIndex]->Map(
					0,
					nullptr,
					reinterpret_cast< void** >( &m_mapped[frameIndex] )
				);

			if ( FAILED(hr) || !m_mapped[frameIndex] )
			{
				Release();
				return false;
			}
		}

		return true;
	}

	void Release()
	{
		for ( UINT frameIndex = 0; frameIndex < FrameCount; ++frameIndex )
		{
			if ( m_resources[frameIndex] )
			{
				if ( m_mapped[frameIndex] )
				{
					m_resources[frameIndex]->Unmap(0, nullptr);
					m_mapped[frameIndex] = nullptr;
				}

				m_resources[frameIndex].Reset();
			}

			m_mapped[frameIndex] = nullptr;
		}

		m_capacity = 0;
	}

	ID3D12Resource* Resource(UINT frameIndex) const
	{
		if ( frameIndex >= FrameCount )
			return nullptr;

		return m_resources[frameIndex].Get();
	}

	TVertex* Mapped(UINT frameIndex) const
	{
		if ( frameIndex >= FrameCount )
			return nullptr;

		return m_mapped[frameIndex];
	}

	UINT Capacity() const
	{
		return m_capacity;
	}

	bool IsReady(UINT frameIndex) const
	{
		return Resource(frameIndex) != nullptr && Mapped(frameIndex) != nullptr;
	}

private:
	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, FrameCount> m_resources = {};
	std::array<TVertex*, FrameCount> m_mapped = {};
	UINT m_capacity = 0;
};