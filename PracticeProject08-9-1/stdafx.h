// stdafx.h : 자주 사용하지만 자주 변경되지는 않는
// 표준 시스템 포함 파일 및 프로젝트 관련 포함 파일이
// 들어 있는 포함 파일입니다.
//

#pragma once

#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용은 Windows 헤더에서 제외합니다.

#define USING_NETWORK					// 네트워크 사용 여부

//ServerCore
#ifdef _DEBUG
#pragma comment(lib, "ServerCore\\Debug\\ServerCore.lib")
#pragma comment(lib, "Protobuf\\Debug\\libprotobufd.lib")
#else
#pragma comment(lib, "ServerCore\\Release\\ServerCore.lib")
#pragma comment(lib, "Protobuf\\Release\\libprotobuf.lib")
#endif


// Windows 헤더 파일:
//#include <windows.h>
#define _HAS_STD_BYTE 0  
#include "CorePch.h"



extern ClientServiceRef g_clientService;

// C의 런타임 헤더 파일입니다.
#include <stdlib.h>
#include <malloc.h>
#include <memory>
#include <tchar.h>
#include <math.h>
#include <fstream>
#include <functional>

#include <sstream>
#include <string>
#include <shellapi.h>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include "MathHelper.h"

#include <DXGIDebug.h>

#include <Mmsystem.h>

#include <assert.h>
#include <algorithm>
#include <memory.h>
#include <wrl.h>

#include <array>
#include <vector>
#include <unordered_map>
#include <random>
#include <chrono>

#include <wincodec.h>
#include <windowsx.h>
#include <fmod.hpp>
#include <cassert>

#include "d3dx12.h"

//using namespace std;




using namespace DirectX;
using namespace DirectX::PackedVector;

using Microsoft::WRL::ComPtr;

#define FRAME_BUFFER_WIDTH		640
#define FRAME_BUFFER_HEIGHT		480

#define MAX_LIGHTS				4
#define MAX_MATERIALS			256 
#define MAX_BONES				256

#define POINT_LIGHT				1
#define SPOT_LIGHT				2
#define DIRECTIONAL_LIGHT		3


//#define _WITH_CB_GAMEOBJECT_32BIT_CONSTANTS
//#define _WITH_CB_GAMEOBJECT_ROOT_DESCRIPTOR
#define _WITH_CB_WORLD_MATRIX_DESCRIPTOR_TABLE

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

/*#pragma comment(lib, "DirectXTex.lib")*/

// TODO: 프로그램에 필요한 추가 헤더는 여기에서 참조합니다.

extern UINT	gnCbvSrvDescriptorIncrementSize;
extern UINT gnRtvDescriptorIncrementSize;

extern ComPtr<ID3D12Resource> CreateBufferResource(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, void* pData, UINT nBytes, D3D12_HEAP_TYPE d3dHeapType = D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATES d3dResourceStates = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, ID3D12Resource** ppd3dUploadBuffer = NULL);
extern ComPtr<ID3D12Resource> CreateTextureResourceFromDDSFile(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const wchar_t* pszFileName, ID3D12Resource** ppd3dUploadBuffer, D3D12_RESOURCE_STATES d3dResourceStates = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
extern ID3D12Resource* CreateTexture2DResource(ID3D12Device* pd3dDevice, UINT nWidth, UINT nHeight, UINT nElements, UINT nMipLevels, DXGI_FORMAT dxgiFormat, D3D12_RESOURCE_FLAGS d3dResourceFlags, D3D12_RESOURCE_STATES d3dResourceStates, D3D12_CLEAR_VALUE* pd3dClearValue);

extern void SynchronizeResourceTransition(ID3D12GraphicsCommandList* pd3dCommandList, ID3D12Resource* pd3dResource, D3D12_RESOURCE_STATES d3dStateBefore, D3D12_RESOURCE_STATES d3dStateAfter);

#define RANDOM_COLOR	XMFLOAT4(rand()/ float(RAND_MAX), rand()/ float(RAND_MAX), rand()/ float(RAND_MAX), rand()/ float(RAND_MAX))

#define ROOT_PARAMETER_CAMERA			0
#define ROOT_PARAMETER_PLAYER			1
#define ROOT_PARAMETER_OBJECT			2
#define ROOT_PARAMETER_MATERIAL			3
#define ROOT_PARAMETER_LIGHT			4
#define ROOT_PARAMETER_DRAW_OPTIONS		5
#define ROOT_PARAMETER_GLOBAL_SRV		6
#define ROOT_PARAMETER_MATERIAL_ID		7
#define ROOT_PARAMETER_BONE_PALETTE		8
#define ROOT_PARAMETER_FOG				9
#define ROOT_PARAMETER_SHADOW			10
#define ROOT_PARAMETER_SHADOW_PASS		10 //쓰면 안되긴 하는데, 일단 지금은 컴파일 에러 막기용.

#define EPSILON							1.0e-10f


static void DBG_PrintF(const char* fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
	va_end(ap);
	OutputDebugStringA(buf);
}

// ============================================================================
// Render profiling log
// ============================================================================
// 1 = 켬, 0 = 끔
#define LOG_RENDER_PROFILE 0

// 너무 작은 함수까지 전부 찍기 싫으면 0.05f, 0.1f 같은 값으로 올리면 됨.
// 일단 병목 찾는 단계에서는 0.0f 권장.
#define LOG_RENDER_PROFILE_MIN_MS 0.0

#if LOG_RENDER_PROFILE

class CScopedRenderProfile
{
public:
	using Clock = std::chrono::high_resolution_clock;

	explicit CScopedRenderProfile(const char* name)
		: m_name(name)
		, m_begin(Clock::now())
	{
	}

	~CScopedRenderProfile()
	{
		const auto end = Clock::now();
		const double elapsedMs =
			std::chrono::duration<double, std::milli>(end - m_begin).count();

		if ( elapsedMs >= LOG_RENDER_PROFILE_MIN_MS && elapsedMs >= 1.0f)
		{
			DBG_PrintF("[RenderProfile] %-55s : %.3f ms\n", m_name, elapsedMs);
		}
	}

private:
	const char* m_name = "";
	Clock::time_point m_begin;
};

#define PROFILE_CONCAT_IMPL(a, b) a##b
#define PROFILE_CONCAT(a, b) PROFILE_CONCAT_IMPL(a, b)
#define PROFILE_RENDER_SCOPE(name) CScopedRenderProfile PROFILE_CONCAT(_renderProfileScope_, __LINE__)(name)

#else

#define PROFILE_RENDER_SCOPE(name) ((void)0)

#endif
