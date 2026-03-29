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

#include <wincodec.h>
#include <windowsx.h>
//#include <fmod.hpp>
#include <cassert>

#include "d3dx12.h"

//using namespace std;




using namespace DirectX;
using namespace DirectX::PackedVector;

using Microsoft::WRL::ComPtr;

#define FRAME_BUFFER_WIDTH		640
#define FRAME_BUFFER_HEIGHT		480

#define MAX_LIGHTS				8 
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

extern ID3D12Resource* CreateBufferResource(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, void* pData, UINT nBytes, D3D12_HEAP_TYPE d3dHeapType = D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATES d3dResourceStates = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, ID3D12Resource** ppd3dUploadBuffer = NULL);
extern ID3D12Resource* CreateTextureResourceFromDDSFile(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, const wchar_t* pszFileName, ID3D12Resource** ppd3dUploadBuffer, D3D12_RESOURCE_STATES d3dResourceStates = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
extern ID3D12Resource* CreateTexture2DResource(ID3D12Device* pd3dDevice, UINT nWidth, UINT nHeight, UINT nElements, UINT nMipLevels, DXGI_FORMAT dxgiFormat, D3D12_RESOURCE_FLAGS d3dResourceFlags, D3D12_RESOURCE_STATES d3dResourceStates, D3D12_CLEAR_VALUE* pd3dClearValue);

extern void SynchronizeResourceTransition(ID3D12GraphicsCommandList* pd3dCommandList, ID3D12Resource* pd3dResource, D3D12_RESOURCE_STATES d3dStateBefore, D3D12_RESOURCE_STATES d3dStateAfter);

#define RANDOM_COLOR	XMFLOAT4(rand()/ float(RAND_MAX), rand()/ float(RAND_MAX), rand()/ float(RAND_MAX), rand()/ float(RAND_MAX))

#define ROOT_PARAMETER_CAMERA			0
#define ROOT_PARAMETER_PLAYER			1
#define ROOT_PARAMETER_OBJECT			2
#define ROOT_PARAMETER_MATERIAL			3
#define ROOT_PARAMETER_LIGHT			4
#define ROOT_PARAMETER_MATERIAL_ID		7
#define ROOT_PARAMETER_BONE_PALETTE		8

#define EPSILON							1.0e-10f

inline bool IsZero(float fValue) { return((fabsf(fValue) < EPSILON)); }
inline bool IsEqual(float fA, float fB) { return(::IsZero(fA - fB)); }
inline float InverseSqrt(float fValue) { return 1.0f / sqrtf(fValue); }
inline void Swap(float* pfS, float* pfT) { float fTemp = *pfS; *pfS = *pfT; *pfT = fTemp; }

namespace Vector3
{
	inline bool IsZero(const XMFLOAT3& xmf3Vector)
	{
		if (::IsZero(xmf3Vector.x) && ::IsZero(xmf3Vector.y) && ::IsZero(xmf3Vector.z))return(true);
		return(false);
	}

	inline XMFLOAT3 XMVectorToFloat3(const XMVECTOR& xmvVector)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, xmvVector);
		return(xmf3Result);
	}

	inline XMFLOAT3 ScalarProduct(const XMFLOAT3& xmf3Vector, float fScalar, bool bNormalize = true)
	{
		XMFLOAT3 xmf3Result;
		if (bNormalize)
			XMStoreFloat3(&xmf3Result, XMVector3Normalize(XMLoadFloat3(&xmf3Vector)) * fScalar);
		else
			XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector) * fScalar);
		return(xmf3Result);
	}

	inline XMFLOAT3 Add(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector1) + XMLoadFloat3(&xmf3Vector2));
		return(xmf3Result);
	}

	inline XMFLOAT3 Add(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2, float fScalar)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector1) + (XMLoadFloat3(&xmf3Vector2) * fScalar));
		return(xmf3Result);
	}

	inline XMFLOAT3 Subtract(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector1) - XMLoadFloat3(&xmf3Vector2));
		return(xmf3Result);
	}

	inline float DotProduct(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMVector3Dot(XMLoadFloat3(&xmf3Vector1), XMLoadFloat3(&xmf3Vector2)));
		return(xmf3Result.x);
	}

	inline XMFLOAT3 CrossProduct(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2, bool bNormalize = true)
	{
		XMFLOAT3 xmf3Result;
		if (bNormalize)
			XMStoreFloat3(&xmf3Result, XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&xmf3Vector1), XMLoadFloat3(&xmf3Vector2))));
		else
			XMStoreFloat3(&xmf3Result, XMVector3Cross(XMLoadFloat3(&xmf3Vector1), XMLoadFloat3(&xmf3Vector2)));
		return(xmf3Result);
	}

	inline XMFLOAT3 Normalize(const XMFLOAT3& xmf3Vector)
	{
		XMFLOAT3 m_xmf3Normal;
		XMStoreFloat3(&m_xmf3Normal, XMVector3Normalize(XMLoadFloat3(&xmf3Vector)));
		return(m_xmf3Normal);
	}

	inline float Length(const XMFLOAT3& xmf3Vector)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMVector3Length(XMLoadFloat3(&xmf3Vector)));
		return(xmf3Result.x);
	}

	inline float Angle(const XMVECTOR& xmvVector1, const XMVECTOR& xmvVector2)
	{
		XMVECTOR xmvAngle = XMVector3AngleBetweenNormals(xmvVector1, xmvVector2);
		return(XMConvertToDegrees(acosf(XMVectorGetX(xmvAngle))));
	}

	inline float Angle(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
	{
		XMVECTOR f3Vector1 = XMLoadFloat3(&xmf3Vector1);
		XMVECTOR f3Vector2 = XMLoadFloat3(&xmf3Vector2);
		return(Angle(f3Vector1, f3Vector2));
	}

	inline XMFLOAT3 TransformNormal(const XMFLOAT3& xmf3Vector, const XMMATRIX& xmmtxTransform)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMVector3TransformNormal(XMLoadFloat3(&xmf3Vector), xmmtxTransform));
		return(xmf3Result);
	}

	inline XMFLOAT3 TransformCoord(const XMFLOAT3& xmf3Vector, const XMMATRIX& xmmtxTransform)
	{
		XMFLOAT3 xmf3Result;
		XMStoreFloat3(&xmf3Result, XMVector3TransformCoord(XMLoadFloat3(&xmf3Vector), xmmtxTransform));
		return(xmf3Result);
	}

	inline XMFLOAT3 TransformCoord(const XMFLOAT3& xmf3Vector, const XMFLOAT4X4& xmmtx4x4Matrix)
	{
		XMMATRIX mtxTransform = XMLoadFloat4x4(&xmmtx4x4Matrix);
		return(TransformCoord(xmf3Vector, mtxTransform));
	}

	inline XMVECTOR ClosestPointOnSegment(FXMVECTOR A, FXMVECTOR B, FXMVECTOR P)
	{
		XMVECTOR AB = B - A;	// A->B 방향과 길이를 가진 벡터
		XMVECTOR AP = P - A;	// A->P 방향과 길이를 가진 벡터

		float abLenSq = XMVectorGetX(XMVector3Dot(AB, AB));	 // AB를 AB로 내적한 것은 A->B 길이의 제곱과 같은 의미
		float t = 0.0f;

		if (abLenSq > 0.0f)
		{
			t = XMVectorGetX(XMVector3Dot(AP, AB)) / abLenSq;
			t = std::clamp(t, 0.0f, 1.0f);
		}

		return XMVectorAdd(A, XMVectorScale(AB, t));
	}

	inline float distPointToSegment(FXMVECTOR A, FXMVECTOR B, FXMVECTOR P)
	{
		XMVECTOR AB = B - A;	// A->B 방향과 길이를 가진 벡터
		XMVECTOR AP = P - A;	// A->P 방향과 길이를 가진 벡터

		float abLenSq = XMVectorGetX(XMVector3Dot(AB, AB));	 // AB를 AB로 내적한 것은 A->B 길이의 제곱과 같은 의미
		float t = 0.0f;

		if (abLenSq > 0.0f)
		{
			t = XMVectorGetX(XMVector3Dot(AP, AB)) / abLenSq;
			t = std::clamp(t, 0.0f, 1.0f);
		}
		XMVECTOR closest = XMVectorAdd(A, XMVectorScale(AB, t));
		
		return XMVectorGetX(XMVector3LengthSq(closest - P));
	}

	inline float distSegmentToSegment(FXMVECTOR A, FXMVECTOR B, FXMVECTOR V0, FXMVECTOR V1)
	{
		XMVECTOR AB = B - A;
		XMVECTOR V0V1 = V1 - V0;
		XMVECTOR AV0 = V0 - A;
		XMVECTOR P;
		XMVECTOR Q;

		float ABLensq = XMVectorGetX(XMVector3Dot(AB, AB)); // |AB|^2 
		float ABProjV0V1 = XMVectorGetX(XMVector3Dot(V0V1, AB)); //V0V1 . AB = |V0V1||AB|cos(theta)
		float V0V1Lensq = XMVectorGetX(XMVector3Dot(V0V1, V0V1)); // |V0V1|^2 
		float ABProjAV0 = XMVectorGetX(XMVector3Dot(AV0, AB)); // AV0 . AB = |AV0||AB|cos(theta)
		float V0V1ProjAV0 = XMVectorGetX(XMVector3Dot(AV0, V0V1)); // AV0 . V0V1 = |AV0||V0V1|cos(theta)

		float denom = ABLensq * V0V1Lensq - ABProjV0V1 * ABProjV0V1;
				// |AB|^2 * |V0V1|^2 - |V0V1|^2 * |AB|^2 * cos^2(theta)
				// |AB|^2 |V0V1|^2 (1 - cos^2(theta))
				// |AB|^2 |V0V1|^2 sin^2(theta) = (|AB||V0V1|sin(theta))^2
		float s = 0.0f;
		float t = 0.0f;

		if (ABLensq <= EPSILON && V0V1Lensq <= EPSILON) // AB = 점, V0V1 = 점
		{
			P = A;
			Q = V0;
			return XMVectorGetX(XMVector3LengthSq(P - Q));	// 따라서 그 점과 점까지의 거리 반환
		}

		if (ABLensq <= EPSILON) // AB = 점 V0V1 = 선분
		{
			s = 0.0f;
			t = std::clamp(-V0V1ProjAV0 / V0V1Lensq, 0.0f, 1.0f);
		}
		else if (V0V1Lensq <= EPSILON) // V0V1 = 점 AB = 선분
		{
			t = 0.0f;
			s = std::clamp(ABProjAV0 / ABLensq, 0.0f, 1.0f);
		}
		else
		{
			if (denom > EPSILON)
				s = std::clamp((ABProjV0V1 * V0V1ProjAV0 - V0V1Lensq * ABProjAV0) / denom, 0.0f, 1.0f);
				// |V0V1||AB|cos(theta) * |AV0||V0V1|cos(alpha) - |V0V1|^2 * |AV0||AB|cos(beta)
				// |V0V1|^2 |AB| |AV0| cos(theta) * cos(alpha) - |V0V1|^2 |AB| |AV0| cos(beta)
				// |V0V1|^2 |AB| |AV0| (cos(theta) * cos(alpha) - cos(beta))
				// |V0V1|^2 |AB| |AV0|cos(theta) * cos(alpha) - cos(beta) / |V0V1|^2 |AB|^2  (1 - cos^2(theta))
				// |AV0| cos(theta) * cos(alpha) - cos(beta) / |AB|(1 - cos^2(theta))
				// cos(beta): AV0 가 AB 방향으로 얼마나 놓여 있나
				// cos(alpha): AV0 가 V0V1 방향으로 얼마나 놓여 있나
				// cos(theta): 두 선분 방향 AB, V0V1 가 얼마나 비슷한가(평행한가?)
				// 1 - cos^2(theta): 두 방향이 얼마나 평행하지 않은가
				// AB 위 최근접점
			else
				s = 0.0f;

			t = (ABProjV0V1 * s + V0V1ProjAV0) / V0V1Lensq;
				// |AV0| (cos(alpha) - cos(theta)cos(beta))	/ (| V0V1 | (1 - cos ^ 2(theta)))
				// V0V1 위 최근접점
			if (t < 0.0f) // V0가 최근접점
			{
				t = 0.0f;
				s = std::clamp(ABProjAV0 / ABLensq, 0.0f, 1.0f);
			}
			else if (t > 1.0f)	// V1이 최근접점
			{
				t = 1.0f;
				s = std::clamp((ABProjV0V1 + ABProjAV0) / ABLensq, 0.0f, 1.0f);
			}
		}

		P = A + AB * s;
		Q = V0 + V0V1 * t;

		return XMVectorGetX(XMVector3LengthSq(P - Q));
	}

	inline bool PointInTriangle(FXMVECTOR P, FXMVECTOR V0, FXMVECTOR V1, FXMVECTOR V2)
	{
		XMVECTOR v0 = V2 - V0;
		XMVECTOR v1 = V1 - V0;
		XMVECTOR v2 = P - V0;

		float dot00 = XMVectorGetX(XMVector3Dot(v0, v0));
		float dot01 = XMVectorGetX(XMVector3Dot(v0, v1));
		float dot02 = XMVectorGetX(XMVector3Dot(v0, v2));
		float dot11 = XMVectorGetX(XMVector3Dot(v1, v1));
		float dot12 = XMVectorGetX(XMVector3Dot(v1, v2));

		float denom = dot00 * dot11 - dot01 * dot01;
		if (fabsf(denom) <= EPSILON)
			return false;

		float invDenom = 1.0f / denom;
		float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
		float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

		return (u >= 0.0f) && (v >= 0.0f) && (u + v <= 1.0f);
	}
	
	inline float distSegmentToFace(FXMVECTOR A, FXMVECTOR B, FXMVECTOR V0, FXMVECTOR V1, FXMVECTOR V2)
	{
		XMVECTOR AB = B - A;
		XMVECTOR E0 = V1 - V0;
		XMVECTOR E1 = V2 - V0;
		XMVECTOR N = XMVector3Cross(E0, E1);	// 삼각형 평면 법선 구하기
												// |E0||E1|sin(theta)
		float nLenSq = XMVectorGetX(XMVector3Dot(N, N));

		if (nLenSq <= EPSILON)
			return FLT_MAX; // degenerate triangle

		N = XMVector3Normalize(N);
		float f0 = XMVectorGetX(XMVector3Dot(A - V0, N));	// A가 평면에서 얼마나 멀어져 있는지 구하기
															// |A - V0|cos(alpha)
		float f1 = XMVectorGetX(XMVector3Dot(B - V0, N));	// B가 평면에서 얼마나 멀어져 있는지 구하기
															// |B - V0|cos(beta)

		float t = 0.0f;
		float denom = f0 - f1;
		if (fabsf(denom) > EPSILON)
			t = std::clamp(f0 / denom, 0.0f, 1.0f);
		else
			t = (fabsf(f0) < fabsf(f1)) ? 0.0f : 1.0f;

		XMVECTOR closest = A + AB * t;
		float signedDist = XMVectorGetX(XMVector3Dot(closest - V0, N));	// 캡슐 선분 위 최근접점이 평면에서 얼마나 멀어져 있는지 구하기
																		// |closest - V0|cos(gamma)
		XMVECTOR Q = closest - N * signedDist;	// 캡슐 선분 위 최근접점을 삼각형 평면에 수직으로 내린 점
												// closest - N * |closest - V0|cos(gamma)

		if (!PointInTriangle(Q, V0, V1, V2))
			return FLT_MAX; // 투영점이 면 내부가 아님

		return signedDist * signedDist; // 선분-삼각형 면 거리 제곱
	}
	inline bool distSegmentToAABB(FXMVECTOR A, FXMVECTOR B, FXMVECTOR Extents)
	{
		float ax = XMVectorGetX(A);
		float ay = XMVectorGetY(A);
		float az = XMVectorGetZ(A);

		float bx = XMVectorGetX(B);
		float by = XMVectorGetY(B);
		float bz = XMVectorGetZ(B);

		float ex = XMVectorGetX(Extents);
		float ey = XMVectorGetY(Extents);
		float ez = XMVectorGetZ(Extents);

		float dx = bx - ax;
		float dy = by - ay;
		float dz = bz - az;

		float tmin = 0.0f;
		float tmax = 1.0f;

		auto slab = [&](float a, float d, float e) -> bool
			{
				if (fabsf(d) <= EPSILON)
				{
					return (a >= -e && a <= e);
				}

				float invD = 1.0f / d;
				float t1 = (-e - a) * invD;
				float t2 = (e - a) * invD;

				if (t1 > t2)
					std::swap(t1, t2);

				tmin = max(tmin, t1);
				tmax = min(tmax, t2);

				return tmin <= tmax;
			};

		return slab(ax, dx, ex) && slab(ay, dy, ey) && slab(az, dz, ez);
	}
}

namespace Vector4
{
	inline XMFLOAT4 Add(const XMFLOAT4& xmf4Vector1, const XMFLOAT4& xmf4Vector2)
	{
		XMFLOAT4 xmf4Result;
		XMStoreFloat4(&xmf4Result, XMLoadFloat4(&xmf4Vector1) + XMLoadFloat4(&xmf4Vector2));
		return(xmf4Result);
	}

	inline XMFLOAT4 Multiply(const XMFLOAT4& xmf4Vector1, const XMFLOAT4& xmf4Vector2)
	{
		XMFLOAT4 xmf4Result;
		XMStoreFloat4(&xmf4Result, XMLoadFloat4(&xmf4Vector1) * XMLoadFloat4(&xmf4Vector2));
		return(xmf4Result);
	}

	inline XMFLOAT4 Multiply(float fScalar, const XMFLOAT4& xmf4Vector)
	{
		XMFLOAT4 xmf4Result;
		XMStoreFloat4(&xmf4Result, fScalar * XMLoadFloat4(&xmf4Vector));
		return(xmf4Result);
	}
}

namespace Matrix4x4
{
	inline XMFLOAT4X4 Identity()
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixIdentity());
		return(xmmtx4x4Result);
	}

	inline XMFLOAT4X4 Multiply(const XMFLOAT4X4& xmmtx4x4Matrix1, const XMFLOAT4X4& xmmtx4x4Matrix2)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMLoadFloat4x4(&xmmtx4x4Matrix1) * XMLoadFloat4x4(&xmmtx4x4Matrix2));
		return(xmmtx4x4Result);
	}

	inline XMFLOAT4X4 Multiply(const XMFLOAT4X4& xmmtx4x4Matrix1, const XMMATRIX& xmmtxMatrix2)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMLoadFloat4x4(&xmmtx4x4Matrix1) * xmmtxMatrix2);
		return(xmmtx4x4Result);
	}

	inline XMFLOAT4X4 Multiply(const XMMATRIX& xmmtxMatrix1, const XMFLOAT4X4& xmmtx4x4Matrix2)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, xmmtxMatrix1 * XMLoadFloat4x4(&xmmtx4x4Matrix2));
		return(xmmtx4x4Result);
	}

	inline XMFLOAT4X4 Inverse(const XMFLOAT4X4& xmmtx4x4Matrix)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixInverse(NULL, XMLoadFloat4x4(&xmmtx4x4Matrix)));
		return(xmmtx4x4Result);
	}

	inline XMFLOAT4X4 Transpose(const XMFLOAT4X4& xmmtx4x4Matrix)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixTranspose(XMLoadFloat4x4(&xmmtx4x4Matrix)));
		return(xmmtx4x4Result);
	}

	inline XMFLOAT4X4 PerspectiveFovLH(float FovAngleY, float AspectRatio, float NearZ, float FarZ)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixPerspectiveFovLH(FovAngleY, AspectRatio, NearZ, FarZ));
		return(xmmtx4x4Result);
	}

	inline XMFLOAT4X4 LookAtLH(const XMFLOAT3& xmf3EyePosition, const XMFLOAT3& xmf3LookAtPosition, const XMFLOAT3& xmf3UpDirection)
	{
		XMFLOAT4X4 xmmtx4x4Result;
		XMStoreFloat4x4(&xmmtx4x4Result, XMMatrixLookAtLH(XMLoadFloat3(&xmf3EyePosition), XMLoadFloat3(&xmf3LookAtPosition), XMLoadFloat3(&xmf3UpDirection)));
		return(xmmtx4x4Result);
	}
}

namespace Triangle
{
	inline bool Intersect(const XMFLOAT3& xmf3RayPosition, const XMFLOAT3& xmf3RayDirection, const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2, float& fHitDistance)
	{
		return(TriangleTests::Intersects(XMLoadFloat3(&xmf3RayPosition), XMLoadFloat3(&xmf3RayDirection), XMLoadFloat3(&v0), XMLoadFloat3(&v1), XMLoadFloat3(&v2), fHitDistance));
	}
}

namespace Plane
{
	inline XMFLOAT4 Normalize(XMFLOAT4& xmf4Plane)
	{
		XMFLOAT4 xmf4Result;
		XMStoreFloat4(&xmf4Result, XMPlaneNormalize(XMLoadFloat4(&xmf4Plane)));
		return(xmf4Result);
	}
}


static void DBG_PrintF(const char* fmt, ...)
{
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
	va_end(ap);
	OutputDebugStringA(buf);
}

enum class EColliderType : uint8_t
{
	None = 0,
	AABB,
	OOBB,
	BSphere,
	BCapsule
};

enum class EDirection : uint8_t
{
	X = 0,
	Y,
	Z
};