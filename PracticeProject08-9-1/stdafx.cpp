// stdafx.cpp : 표준 포함 파일만 들어 있는 소스 파일입니다.
// LabProject03-1.pch는 미리 컴파일된 헤더가 됩니다.
// stdafx.obj에는 미리 컴파일된 형식 정보가 포함됩니다.

#include "stdafx.h"

ClientServiceRef g_clientService = nullptr;

#include "DDSTextureLoader12.h"

UINT gnCbvSrvDescriptorIncrementSize = 0;
UINT gnRtvDescriptorIncrementSize = 0;

#if defined(_DEBUG) || defined(DEBUG)

namespace
{
	struct D3D12ResourceCreationStats
	{
		size_t resourceCount = 0;
		size_t bufferCount = 0;
		size_t texture2DCount = 0;

		size_t totalBytes = 0;
		size_t defaultBytes = 0;
		size_t uploadBytes = 0;
		size_t readbackBytes = 0;
		size_t otherHeapBytes = 0;

		size_t bufferBytes = 0;
		size_t texture2DBytes = 0;
	};

	static D3D12ResourceCreationStats g_d3d12ResourceCreationStats{};

	static double DBG_BytesToMiB(size_t bytes)
	{
		return static_cast< double >( bytes ) / ( 1024.0 * 1024.0 );
	}

	static size_t DBG_EstimateResourceBytes(
		ID3D12Device* device,
		const D3D12_RESOURCE_DESC& desc)
	{
		if ( desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER )
			return static_cast< size_t >( desc.Width );

		if ( device )
		{
			const D3D12_RESOURCE_ALLOCATION_INFO info =
				device->GetResourceAllocationInfo(0, 1, &desc);

			return static_cast< size_t >( info.SizeInBytes );
		}

		return 0;
	}

	static void DBG_RecordD3D12CreatedResource(
		ID3D12Device* device,
		const D3D12_RESOURCE_DESC& desc,
		D3D12_HEAP_TYPE heapType)
	{
		const size_t bytes = DBG_EstimateResourceBytes(device, desc);

		++g_d3d12ResourceCreationStats.resourceCount;
		g_d3d12ResourceCreationStats.totalBytes += bytes;

		if ( desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER )
		{
			++g_d3d12ResourceCreationStats.bufferCount;
			g_d3d12ResourceCreationStats.bufferBytes += bytes;
		}
		else if ( desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D )
		{
			++g_d3d12ResourceCreationStats.texture2DCount;
			g_d3d12ResourceCreationStats.texture2DBytes += bytes;
		}

		switch ( heapType )
		{
		case D3D12_HEAP_TYPE_DEFAULT:
			g_d3d12ResourceCreationStats.defaultBytes += bytes;
			break;
		case D3D12_HEAP_TYPE_UPLOAD:
			g_d3d12ResourceCreationStats.uploadBytes += bytes;
			break;
		case D3D12_HEAP_TYPE_READBACK:
			g_d3d12ResourceCreationStats.readbackBytes += bytes;
			break;
		default:
			g_d3d12ResourceCreationStats.otherHeapBytes += bytes;
			break;
		}
	}
}

void DBG_ResetD3D12ResourceCreationStats()
{
	g_d3d12ResourceCreationStats = {};
}

void DBG_DumpD3D12ResourceCreationStats()
{
	const auto& s = g_d3d12ResourceCreationStats;

	char buf[1024];
	sprintf_s(
		buf,
		"[D3D12ResourceCreateStats] resources=%zu buffers=%zu textures2D=%zu "
		"total=%.3f MiB default=%.3f MiB upload=%.3f MiB readback=%.3f MiB otherHeap=%.3f MiB "
		"bufferBytes=%.3f MiB texture2DBytes=%.3f MiB\n",
		s.resourceCount,
		s.bufferCount,
		s.texture2DCount,
		DBG_BytesToMiB(s.totalBytes),
		DBG_BytesToMiB(s.defaultBytes),
		DBG_BytesToMiB(s.uploadBytes),
		DBG_BytesToMiB(s.readbackBytes),
		DBG_BytesToMiB(s.otherHeapBytes),
		DBG_BytesToMiB(s.bufferBytes),
		DBG_BytesToMiB(s.texture2DBytes)
	);
	OutputDebugStringA(buf);
}

#endif

// TODO: 필요한 추가 헤더는
// 이 파일이 아닌 STDAFX.H에서 참조합니다.

ComPtr<ID3D12Resource> CreateBufferResource(
	ID3D12Device *pd3dDevice, 
	ID3D12GraphicsCommandList *pd3dCommandList,
	void *pData, 
	UINT nBytes, 
	D3D12_HEAP_TYPE d3dHeapType,
	D3D12_RESOURCE_STATES d3dResourceStates,
	ID3D12Resource **ppd3dUploadBuffer)
{
	ComPtr<ID3D12Resource>pd3dBuffer;

	D3D12_HEAP_PROPERTIES d3dHeapPropertiesDesc;
	::ZeroMemory(&d3dHeapPropertiesDesc, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapPropertiesDesc.Type = d3dHeapType;
	d3dHeapPropertiesDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapPropertiesDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapPropertiesDesc.CreationNodeMask = 1;
	d3dHeapPropertiesDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC d3dResourceDesc;
	::ZeroMemory(&d3dResourceDesc, sizeof(D3D12_RESOURCE_DESC));
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	d3dResourceDesc.Alignment = 0;
	d3dResourceDesc.Width = nBytes;
	d3dResourceDesc.Height = 1;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	d3dResourceDesc.SampleDesc.Count = 1;
	d3dResourceDesc.SampleDesc.Quality = 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_RESOURCE_STATES d3dResourceInitialStates = D3D12_RESOURCE_STATE_COMMON;
	if (d3dHeapType == D3D12_HEAP_TYPE_UPLOAD)
		d3dResourceInitialStates = D3D12_RESOURCE_STATE_GENERIC_READ;
	else if (d3dHeapType == D3D12_HEAP_TYPE_READBACK)
		d3dResourceInitialStates = D3D12_RESOURCE_STATE_COPY_DEST;

	HRESULT hResult = pd3dDevice->CreateCommittedResource(
		&d3dHeapPropertiesDesc,
		D3D12_HEAP_FLAG_NONE, 
		&d3dResourceDesc, 
		d3dResourceInitialStates,
		NULL, 
		__uuidof(ID3D12Resource),
		(void **)&pd3dBuffer);

#if defined(_DEBUG) || defined(DEBUG)
	if ( SUCCEEDED(hResult) && pd3dBuffer )
	{
		DBG_RecordD3D12CreatedResource(
			pd3dDevice,
			d3dResourceDesc,
			d3dHeapType
		);
	}
#endif

	if (pData)
	{
		switch (d3dHeapType)
		{
		case D3D12_HEAP_TYPE_DEFAULT:
		{
			if (ppd3dUploadBuffer)
			{
				d3dHeapPropertiesDesc.Type = D3D12_HEAP_TYPE_UPLOAD;
				HRESULT uploadHr = pd3dDevice->CreateCommittedResource(
					&d3dHeapPropertiesDesc,
					D3D12_HEAP_FLAG_NONE,
					&d3dResourceDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					NULL,
					__uuidof( ID3D12Resource ),
					( void** ) ppd3dUploadBuffer
				);

#if defined(_DEBUG) || defined(DEBUG)
				if ( SUCCEEDED(uploadHr) && ppd3dUploadBuffer && *ppd3dUploadBuffer )
				{
					DBG_RecordD3D12CreatedResource(
						pd3dDevice,
						d3dResourceDesc,
						D3D12_HEAP_TYPE_UPLOAD
					);
				}
#endif

#ifdef _WITH_MAPPING
				D3D12_RANGE d3dReadRange = { 0, 0 };
				UINT8 *pBufferDataBegin = NULL;
				(*ppd3dUploadBuffer)->Map(0, &d3dReadRange, (void **)&pBufferDataBegin);
				memcpy(pBufferDataBegin, pData, nBytes);
				(*ppd3dUploadBuffer)->Unmap(0, NULL);

				pd3dCommandList->CopyResource(pd3dBuffer, *ppd3dUploadBuffer);
#else
				D3D12_SUBRESOURCE_DATA d3dSubResourceData;
				::ZeroMemory(&d3dSubResourceData, sizeof(D3D12_SUBRESOURCE_DATA));
				d3dSubResourceData.pData = pData;
				d3dSubResourceData.SlicePitch = d3dSubResourceData.RowPitch = nBytes;
				::UpdateSubresources<1>(pd3dCommandList, pd3dBuffer.Get(), *ppd3dUploadBuffer, 0, 0, 1, &d3dSubResourceData);

#endif
				D3D12_RESOURCE_BARRIER d3dResourceBarrier;
				::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
				d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				d3dResourceBarrier.Transition.pResource = pd3dBuffer.Get();
				d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
				d3dResourceBarrier.Transition.StateAfter = d3dResourceStates;
				d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);
			}
			break;
		}
		case D3D12_HEAP_TYPE_UPLOAD:
		{
			D3D12_RANGE d3dReadRange = { 0, 0 };
			UINT8 *pBufferDataBegin = NULL;
			pd3dBuffer->Map(0, &d3dReadRange, (void **)&pBufferDataBegin);
			memcpy(pBufferDataBegin, pData, nBytes);
			pd3dBuffer->Unmap(0, NULL);
			break;
		}
		case D3D12_HEAP_TYPE_READBACK:
			break;
		}
	}
	return(pd3dBuffer);
}

ComPtr<ID3D12Resource> CreateTextureResourceFromDDSFile(
	ID3D12Device *pd3dDevice, 
	ID3D12GraphicsCommandList *pd3dCommandList, 
	const wchar_t *pszFileName, 
	ID3D12Resource **ppd3dUploadBuffer, 
	D3D12_RESOURCE_STATES d3dResourceStates)
{
	ComPtr<ID3D12Resource> pd3dTexture;
	std::unique_ptr<uint8_t[]> ddsData;
	std::vector<D3D12_SUBRESOURCE_DATA> vSubresources;
	DDS_ALPHA_MODE ddsAlphaMode = DDS_ALPHA_MODE_UNKNOWN;
	bool bIsCubeMap = false;

	HRESULT hResult = DirectX::LoadDDSTextureFromFileEx(
		pd3dDevice, 
		pszFileName, 
		0, 
		D3D12_RESOURCE_FLAG_NONE, 
		DDS_LOADER_DEFAULT, 
		&pd3dTexture, 
		ddsData, 
		vSubresources, 
		&ddsAlphaMode, 
		&bIsCubeMap);

#if defined(_DEBUG) || defined(DEBUG)
	if ( SUCCEEDED(hResult) && pd3dTexture )
	{
		DBG_RecordD3D12CreatedResource(
			pd3dDevice,
			pd3dTexture->GetDesc(),
			D3D12_HEAP_TYPE_DEFAULT
		);
	}
#endif

	D3D12_HEAP_PROPERTIES d3dHeapPropertiesDesc;
	::ZeroMemory(&d3dHeapPropertiesDesc, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapPropertiesDesc.Type = D3D12_HEAP_TYPE_UPLOAD;
	d3dHeapPropertiesDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapPropertiesDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapPropertiesDesc.CreationNodeMask = 1;
	d3dHeapPropertiesDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC d3dTextureResourceDesc = pd3dTexture->GetDesc();
	UINT nSubResources = (UINT)vSubresources.size();
	UINT64 nBytes = GetRequiredIntermediateSize(pd3dTexture.Get(), 0, nSubResources);
	//	UINT nSubResources = d3dTextureResourceDesc.DepthOrArraySize * d3dTextureResourceDesc.MipLevels;
	//	UINT64 nBytes = 0;
	//	pd3dDevice->GetCopyableFootprints(&d3dTextureResourceDesc, 0, nSubResources, 0, NULL, NULL, NULL, &nBytes);

	D3D12_RESOURCE_DESC d3dBufferResourceDesc;
	::ZeroMemory(&d3dBufferResourceDesc, sizeof(D3D12_RESOURCE_DESC));
	d3dBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; //Upload Heap에는 텍스쳐를 생성할 수 없음
	d3dBufferResourceDesc.Alignment = 0;
	d3dBufferResourceDesc.Width = nBytes;
	d3dBufferResourceDesc.Height = 1;
	d3dBufferResourceDesc.DepthOrArraySize = 1;
	d3dBufferResourceDesc.MipLevels = 1;
	d3dBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	d3dBufferResourceDesc.SampleDesc.Count = 1;
	d3dBufferResourceDesc.SampleDesc.Quality = 0;
	d3dBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	d3dBufferResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT uploadHr = pd3dDevice->CreateCommittedResource(
		&d3dHeapPropertiesDesc,
		D3D12_HEAP_FLAG_NONE,
		&d3dBufferResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		NULL,
		__uuidof( ID3D12Resource ),
		( void** ) ppd3dUploadBuffer
	);

#if defined(_DEBUG) || defined(DEBUG)
	if ( SUCCEEDED(uploadHr) && ppd3dUploadBuffer && *ppd3dUploadBuffer )
	{
		DBG_RecordD3D12CreatedResource(
			pd3dDevice,
			d3dBufferResourceDesc,
			D3D12_HEAP_TYPE_UPLOAD
		);
	}
#endif

	//UINT nSubResources = (UINT)vSubresources.size();
	//D3D12_SUBRESOURCE_DATA *pd3dSubResourceData = new D3D12_SUBRESOURCE_DATA[nSubResources];
	//for (UINT i = 0; i < nSubResources; i++)pd3dSubResourceData[i] = vSubresources.at(i);

	//	std::vector<D3D12_SUBRESOURCE_DATA>::pointer ptr = &vSubresources[0];
	UINT64 nBytesUpdated = ::UpdateSubresources(
		pd3dCommandList, 
		pd3dTexture.Get(),
		*ppd3dUploadBuffer,
		0, 
		0, 
		nSubResources, 
		&vSubresources[0]);

	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource = pd3dTexture.Get();
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	d3dResourceBarrier.Transition.StateAfter = d3dResourceStates;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

	//	delete[] pd3dSubResourceData;

	return(pd3dTexture);
}

ID3D12Resource* CreateTexture2DResource(
	ID3D12Device *pd3dDevice, 
	UINT nWidth, 
	UINT nHeight, 
	UINT nElements, 
	UINT nMipLevels, 
	DXGI_FORMAT dxgiFormat, 
	D3D12_RESOURCE_FLAGS d3dResourceFlags, 
	D3D12_RESOURCE_STATES d3dResourceStates, 
	D3D12_CLEAR_VALUE *pd3dClearValue)
{
	ID3D12Resource *pd3dTexture = NULL;

	D3D12_HEAP_PROPERTIES d3dHeapPropertiesDesc;
	::ZeroMemory(&d3dHeapPropertiesDesc, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapPropertiesDesc.Type = D3D12_HEAP_TYPE_DEFAULT;
	d3dHeapPropertiesDesc.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapPropertiesDesc.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapPropertiesDesc.CreationNodeMask = 1;
	d3dHeapPropertiesDesc.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC d3dTextureResourceDesc;
	::ZeroMemory(&d3dTextureResourceDesc, sizeof(D3D12_RESOURCE_DESC));
	d3dTextureResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3dTextureResourceDesc.Alignment = 0;
	d3dTextureResourceDesc.Width = nWidth;
	d3dTextureResourceDesc.Height = nHeight;
	d3dTextureResourceDesc.DepthOrArraySize = nElements;
	d3dTextureResourceDesc.MipLevels = nMipLevels;
	d3dTextureResourceDesc.Format = dxgiFormat;
	d3dTextureResourceDesc.SampleDesc.Count = 1;
	d3dTextureResourceDesc.SampleDesc.Quality = 0;
	d3dTextureResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dTextureResourceDesc.Flags = d3dResourceFlags;

	HRESULT hResult = pd3dDevice->CreateCommittedResource(
		&d3dHeapPropertiesDesc,
		D3D12_HEAP_FLAG_NONE,
		&d3dTextureResourceDesc,
		d3dResourceStates,
		pd3dClearValue,
		__uuidof( ID3D12Resource ),
		( void** ) &pd3dTexture);

#if defined(_DEBUG) || defined(DEBUG)
	if ( SUCCEEDED(hResult) && pd3dTexture )
	{
		DBG_RecordD3D12CreatedResource(
			pd3dDevice,
			d3dTextureResourceDesc,
			D3D12_HEAP_TYPE_DEFAULT
		);
	}
#endif

	return( pd3dTexture );
}

void SynchronizeResourceTransition(ID3D12GraphicsCommandList *pd3dCommandList, ID3D12Resource *pd3dResource, D3D12_RESOURCE_STATES d3dStateBefore, D3D12_RESOURCE_STATES d3dStateAfter)
{
	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource = pd3dResource;
	d3dResourceBarrier.Transition.StateBefore = d3dStateBefore;
	d3dResourceBarrier.Transition.StateAfter = d3dStateAfter;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);
}
