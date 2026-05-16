///-----------------------------------------------------------------------------
// File: Shader.cpp
//-----------------------------------------------------------------------------

#include "stdafx.h"
#include "Shader.h"
#include "DDSTextureLoader12.h"
#include "Scene.h"
#include "Material.h"
#include "AssetManager.h"
#include "AnimController.h"

#include <random>

CShader::CShader()
{
}

CShader::~CShader()
{
	if (m_pd3dPipelineState)
		m_pd3dPipelineState.Reset();

	if (m_pd3dGraphicsRootSignature)
		m_pd3dGraphicsRootSignature.Reset();
}

void CShader::ReleaseShaderVariables()
{
}

void CShader::ReleaseUploadBuffers()
{
}

D3D12_SHADER_BYTECODE CShader::CompileShaderFromFile(
	const WCHAR* pszFileName,
	LPCSTR pszShaderName,
	LPCSTR pszShaderProfile,
	ID3DBlob** ppd3dShaderBlob)
{
	D3D12_SHADER_BYTECODE d3dShaderByteCode{};
	d3dShaderByteCode.BytecodeLength = 0;
	d3dShaderByteCode.pShaderBytecode = nullptr;

	if ( ppd3dShaderBlob )
		*ppd3dShaderBlob = nullptr;

	UINT nCompileFlags = 0;
#if defined(_DEBUG)
	nCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ComPtr<ID3DBlob> pd3dErrorBlob;

	HRESULT hr = ::D3DCompileFromFile(
		pszFileName,
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		pszShaderName,
		pszShaderProfile,
		nCompileFlags,
		0,
		ppd3dShaderBlob,
		&pd3dErrorBlob
	);

	if ( pd3dErrorBlob )
	{
		OutputDebugStringA("=== HLSL Compile Log Begin ===\n");
		OutputDebugStringA(( const char* ) pd3dErrorBlob->GetBufferPointer());
		OutputDebugStringA("\n=== HLSL Compile Log End ===\n");
	}

	if ( FAILED(hr) || !ppd3dShaderBlob || !( *ppd3dShaderBlob ) )
	{
		char debugText[512] = {};
		sprintf_s(
			debugText,
			"[Shader Compile Failed] file=%ws entry=%s profile=%s hr=0x%08X\n",
			pszFileName,
			pszShaderName,
			pszShaderProfile,
			static_cast< unsigned int >( hr )
		);
		OutputDebugStringA(debugText);

		return d3dShaderByteCode;
	}

	d3dShaderByteCode.BytecodeLength = ( *ppd3dShaderBlob )->GetBufferSize();
	d3dShaderByteCode.pShaderBytecode = ( *ppd3dShaderBlob )->GetBufferPointer();

	return d3dShaderByteCode;
}

D3D12_SHADER_BYTECODE CShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	D3D12_SHADER_BYTECODE d3dShaderByteCode;
	d3dShaderByteCode.BytecodeLength = 0;
	d3dShaderByteCode.pShaderBytecode = nullptr;

	return(d3dShaderByteCode);
}

D3D12_SHADER_BYTECODE CShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	D3D12_SHADER_BYTECODE d3dShaderByteCode;
	d3dShaderByteCode.BytecodeLength = 0;
	d3dShaderByteCode.pShaderBytecode = nullptr;

	return(d3dShaderByteCode);
}

D3D12_INPUT_LAYOUT_DESC CShader::CreateInputLayout()
{
	UINT nInputElementDescs = 3;
	D3D12_INPUT_ELEMENT_DESC* pd3dInputElementDescs =
		new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	pd3dInputElementDescs[0] = {
		"POSITION",
		0,
		DXGI_FORMAT_R32G32B32_FLOAT,
		0,
		0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	};

	pd3dInputElementDescs[1] = {
		"NORMAL",
		0,
		DXGI_FORMAT_R32G32B32_FLOAT,
		0,
		12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	};

	pd3dInputElementDescs[2] = {
		"TEXCOORD",
		0,
		DXGI_FORMAT_R32G32_FLOAT,
		0,
		24,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	};

	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
	d3dInputLayoutDesc.pInputElementDescs = pd3dInputElementDescs;
	d3dInputLayoutDesc.NumElements = nInputElementDescs;

	return d3dInputLayoutDesc;
}

D3D12_RASTERIZER_DESC CShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC d3dRasterizerDesc;
	::ZeroMemory(&d3dRasterizerDesc, sizeof(D3D12_RASTERIZER_DESC));
	d3dRasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	d3dRasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	d3dRasterizerDesc.FrontCounterClockwise = FALSE;
	d3dRasterizerDesc.DepthBias = 0;
	d3dRasterizerDesc.DepthBiasClamp = 0.0f;
	d3dRasterizerDesc.SlopeScaledDepthBias = 0.0f;
	d3dRasterizerDesc.DepthClipEnable = TRUE;
	d3dRasterizerDesc.MultisampleEnable = FALSE;
	d3dRasterizerDesc.AntialiasedLineEnable = FALSE;
	d3dRasterizerDesc.ForcedSampleCount = 0;
	d3dRasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	return(d3dRasterizerDesc);
}

D3D12_DEPTH_STENCIL_DESC CShader::CreateDepthStencilState()
{
	D3D12_DEPTH_STENCIL_DESC d3dDepthStencilDesc;
	::ZeroMemory(&d3dDepthStencilDesc, sizeof(D3D12_DEPTH_STENCIL_DESC));
	d3dDepthStencilDesc.DepthEnable = TRUE;
	d3dDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	d3dDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	d3dDepthStencilDesc.StencilEnable = FALSE;
	d3dDepthStencilDesc.StencilReadMask = 0x00;
	d3dDepthStencilDesc.StencilWriteMask = 0x00;
	d3dDepthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	d3dDepthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;

	return(d3dDepthStencilDesc);
}

D3D12_BLEND_DESC CShader::CreateBlendState()
{
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	return(d3dBlendDesc);
}

void CShader::CreateShader(
	ID3D12Device* pd3dDevice, 
	ID3D12RootSignature* pd3dGraphicsRootSignature, 
	UINT nRenderTargets,
	DXGI_FORMAT* pdxgiRtvFormats, 
	DXGI_FORMAT dxgiDsvFormat)
{
	HRESULT hResult;

	ComPtr<ID3DBlob> pd3dVertexShaderBlob;
	ComPtr<ID3DBlob> pd3dPixelShaderBlob;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC d3dPipelineStateDesc;
	::ZeroMemory(&d3dPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	d3dPipelineStateDesc.pRootSignature = pd3dGraphicsRootSignature;
	d3dPipelineStateDesc.VS = CreateVertexShader(&pd3dVertexShaderBlob);
	d3dPipelineStateDesc.PS = CreatePixelShader(&pd3dPixelShaderBlob);
	d3dPipelineStateDesc.RasterizerState = CreateRasterizerState();
	d3dPipelineStateDesc.BlendState = CreateBlendState();
	d3dPipelineStateDesc.DepthStencilState = CreateDepthStencilState();
	d3dPipelineStateDesc.InputLayout = CreateInputLayout();
	d3dPipelineStateDesc.SampleMask = UINT_MAX;
	d3dPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	d3dPipelineStateDesc.NumRenderTargets = nRenderTargets;
	for (UINT i = 0; i < nRenderTargets; i++)
		d3dPipelineStateDesc.RTVFormats[i] = (pdxgiRtvFormats) ? pdxgiRtvFormats[i] : DXGI_FORMAT_R8G8B8A8_UNORM;
	d3dPipelineStateDesc.DSVFormat = dxgiDsvFormat;
	d3dPipelineStateDesc.SampleDesc.Count = 1;
	d3dPipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	hResult = pd3dDevice->CreateGraphicsPipelineState(
		&d3dPipelineStateDesc,
		IID_PPV_ARGS(&m_pd3dPipelineState));
	if (FAILED(hResult))
	{
		OutputDebugStringA("CreateGraphicsPipelineState FAILED\n");
	}

	if (pd3dVertexShaderBlob)
		pd3dVertexShaderBlob.Reset();

	if (pd3dPixelShaderBlob)
		pd3dPixelShaderBlob.Reset();

	if (d3dPipelineStateDesc.InputLayout.pInputElementDescs)
		delete[] d3dPipelineStateDesc.InputLayout.pInputElementDescs;
}

void CShader::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
}

void CShader::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext)
{
}

void CShader::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (m_pd3dGraphicsRootSignature)
		pd3dCommandList->SetGraphicsRootSignature(m_pd3dGraphicsRootSignature.Get());

	if (m_pd3dPipelineState)
		pd3dCommandList->SetPipelineState(m_pd3dPipelineState.Get());
}

void CShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext)
{
	OnPrepareRender(pd3dCommandList);
	UpdateShaderVariables(pd3dCommandList, pContext);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
D3D12_INPUT_LAYOUT_DESC CDiffusedShader::CreateInputLayout()
{
	UINT nInputElementDescs = 2;
	D3D12_INPUT_ELEMENT_DESC* pd3dInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	pd3dInputElementDescs[0] = {
		"POSITION",
		0,
		DXGI_FORMAT_R32G32B32_FLOAT,
		0,
		0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	};

	pd3dInputElementDescs[1] = {
		"COLOR",
		0,
		DXGI_FORMAT_R32G32_FLOAT,
		0,
		12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	};

	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
	d3dInputLayoutDesc.pInputElementDescs = pd3dInputElementDescs;
	d3dInputLayoutDesc.NumElements = nInputElementDescs;

	return( d3dInputLayoutDesc );
}

D3D12_SHADER_BYTECODE CDiffusedShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return( CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSDiffused", "vs_5_1", ppd3dShaderBlob) );
}

D3D12_SHADER_BYTECODE CDiffusedShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return( CShader::CompileShaderFromFile(L"Shaders.hlsl", "PSDiffused", "ps_5_1", ppd3dShaderBlob) );
}

D3D12_RASTERIZER_DESC CDiffusedShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC d3dRasterizerDesc;
	::ZeroMemory(&d3dRasterizerDesc, sizeof(D3D12_RASTERIZER_DESC));
	d3dRasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
	d3dRasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	d3dRasterizerDesc.FrontCounterClockwise = FALSE;
	d3dRasterizerDesc.DepthBias = 0;
	d3dRasterizerDesc.DepthBiasClamp = 0.0f;
	d3dRasterizerDesc.SlopeScaledDepthBias = 0.0f;
	d3dRasterizerDesc.DepthClipEnable = TRUE;
	d3dRasterizerDesc.MultisampleEnable = FALSE;
	d3dRasterizerDesc.AntialiasedLineEnable = FALSE;
	d3dRasterizerDesc.ForcedSampleCount = 0;
	d3dRasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	return( d3dRasterizerDesc );
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CTexturedShader::CTexturedShader()
{
}

CTexturedShader::~CTexturedShader()
{
}

D3D12_INPUT_LAYOUT_DESC CTexturedShader::CreateInputLayout()
{
	UINT nInputElementDescs = 2;
	D3D12_INPUT_ELEMENT_DESC* pd3dInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	pd3dInputElementDescs[0] = {
		"POSITION",
		0,
		DXGI_FORMAT_R32G32B32_FLOAT,
		0,
		0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	};

	pd3dInputElementDescs[1] = {
		"TEXCOORD",
		0,
		DXGI_FORMAT_R32G32_FLOAT,
		0,
		12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	};

	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
	d3dInputLayoutDesc.pInputElementDescs = pd3dInputElementDescs;
	d3dInputLayoutDesc.NumElements = nInputElementDescs;

	return(d3dInputLayoutDesc);
}

D3D12_SHADER_BYTECODE CTexturedShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSTextured", "vs_5_1", ppd3dShaderBlob));
}

D3D12_SHADER_BYTECODE CTexturedShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "PSTextured", "ps_5_1", ppd3dShaderBlob));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CIlluminatedTexturedShader::CIlluminatedTexturedShader()
{
}

CIlluminatedTexturedShader::~CIlluminatedTexturedShader()
{
}

D3D12_INPUT_LAYOUT_DESC CIlluminatedTexturedShader::CreateInputLayout()
{
	UINT nInputElementDescs = 4;
	auto* desc = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	desc[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	desc[1] = { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	desc[2] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	desc[3] = { "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };

	D3D12_INPUT_LAYOUT_DESC layout{};
	layout.pInputElementDescs = desc;
	layout.NumElements = nInputElementDescs;
	return layout;
}

D3D12_SHADER_BYTECODE CIlluminatedTexturedShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSTexturedLighting", "vs_5_1", ppd3dShaderBlob));
}

D3D12_SHADER_BYTECODE CIlluminatedTexturedShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "PSTexturedLighting", "ps_5_1", ppd3dShaderBlob));
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CStaticObjectsShader::CStaticObjectsShader()
{
}

CStaticObjectsShader::~CStaticObjectsShader()
{
}

D3D12_INPUT_LAYOUT_DESC CStaticObjectsShader::CreateInputLayout()
{
	UINT nInputElementDescs = 9;
	auto* desc = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	desc[0] = { "POSITION",           0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[1] = { "NORMAL",             0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[2] = { "TEXCOORD",           0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[3] = { "TANGENT",            0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };

	desc[4] = { "INSTANCE_WORLD",     0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[5] = { "INSTANCE_WORLD",     1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[6] = { "INSTANCE_WORLD",     2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[7] = { "INSTANCE_WORLD",     3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[8] = { "INSTANCE_OBJECT_ID", 0, DXGI_FORMAT_R32_UINT,           1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };

	D3D12_INPUT_LAYOUT_DESC layout{};
	layout.pInputElementDescs = desc;
	layout.NumElements = nInputElementDescs;
	return layout;
}

D3D12_SHADER_BYTECODE CStaticObjectsShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return( CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSTexturedLightingInstanced", "vs_5_1", ppd3dShaderBlob) );
}

void CStaticObjectsShader::CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature, UINT nRenderTargets, DXGI_FORMAT* pdxgiRtvFormats, DXGI_FORMAT dxgiDsvFormat)
{
#ifdef _WITH_SCENE_ROOT_SIGNATURE
	m_pd3dGraphicsRootSignature = pd3dGraphicsRootSignature;
#else
	CreateGraphicsRootSignature(pd3dDevice);
#endif

	CShader::CreateShader(
		pd3dDevice,
		m_pd3dGraphicsRootSignature.Get(),
		nRenderTargets,
		pdxgiRtvFormats,
		dxgiDsvFormat);
}

D3D12_SHADER_BYTECODE CStaticObjectsShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "PSTexturedLightingToMultipleRTs", "ps_5_1", ppd3dShaderBlob));
}

void CStaticObjectsShader::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext)
{
	(void)pd3dCommandList;
	(void)pContext;
}

void CStaticObjectsShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext)
{
	CIlluminatedTexturedShader::Render(pd3dCommandList, pCamera, pContext);

#ifdef _WITH_BATCH_MATERIAL
	auto* b = reinterpret_cast<SCENE_STATIC_BATCH*>(pContext);
	if (b && b->material && b->material->NeedsLegacyBinding())
		b->material->UpdateShaderVariables(pd3dCommandList);
#endif
}

D3D12_SHADER_BYTECODE CTreeStaticObjectsShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(L"Shaders.hlsl","PSTexturedLightingToMultipleRTs_AlphaClip","ps_5_1",ppd3dShaderBlob);
}

D3D12_RASTERIZER_DESC CTreeStaticObjectsShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CShader::CreateRasterizerState();
	rs.CullMode = D3D12_CULL_MODE_NONE;
	return rs;
}

CSkinnedObjectsShader::CSkinnedObjectsShader()
{
}

CSkinnedObjectsShader::~CSkinnedObjectsShader()
{
}

void CSkinnedObjectsShader::CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature, UINT nRenderTargets, DXGI_FORMAT* pdxgiRtvFormats, DXGI_FORMAT dxgiDsvFormat)
{
#ifdef _WITH_SCENE_ROOT_SIGNATURE
	m_pd3dGraphicsRootSignature = pd3dGraphicsRootSignature;
#else
	CreateGraphicsRootSignature(pd3dDevice);
#endif

	CShader::CreateShader(
		pd3dDevice,
		m_pd3dGraphicsRootSignature.Get(),
		nRenderTargets,
		pdxgiRtvFormats,
		dxgiDsvFormat
	);
}

D3D12_SHADER_BYTECODE CSkinnedObjectsShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return( CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSSkinnedInstanced", "vs_5_1", ppd3dShaderBlob) );
}

D3D12_SHADER_BYTECODE CSkinnedObjectsShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return( CShader::CompileShaderFromFile(L"Shaders.hlsl", "PSTexturedLightingToMultipleRTs", "ps_5_1", ppd3dShaderBlob) );
}

D3D12_INPUT_LAYOUT_DESC CSkinnedObjectsShader::CreateInputLayout()
{
	UINT nInputElementDescs = 12;
	D3D12_INPUT_ELEMENT_DESC* desc = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	desc[0] = { "POSITION",             0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[1] = { "NORMAL",               0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[2] = { "TEXCOORD",             0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[3] = { "TANGENT",              0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[4] = { "BLENDINDICES",         0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[5] = { "BLENDWEIGHT",          0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };

	desc[6] = { "INSTANCE_WORLD",       0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[7] = { "INSTANCE_WORLD",       1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[8] = { "INSTANCE_WORLD",       2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[9] = { "INSTANCE_WORLD",       3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[10] = { "INSTANCE_MATERIAL_ID",0, DXGI_FORMAT_R32_UINT,           1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[11] = { "INSTANCE_BONE_BASE",  0, DXGI_FORMAT_R32_UINT,           1, 68, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };

	D3D12_INPUT_LAYOUT_DESC layout{};
	layout.pInputElementDescs = desc;
	layout.NumElements = nInputElementDescs;
	return layout;
}

void CSkinnedObjectsShader::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext)
{
	( void ) pd3dCommandList;
	( void ) pContext;
}

void CSkinnedObjectsShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext)
{
	CIlluminatedTexturedShader::Render(pd3dCommandList, pCamera, pContext);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

D3D12_INPUT_LAYOUT_DESC CItemBillboardShader::CreateInputLayout()
{
	UINT nInputElementDescs = 9;
	auto* desc = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	desc[0] = { "POSITION",             0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[1] = { "NORMAL",               0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[2] = { "TEXCOORD",             0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[3] = { "TANGENT",              0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };

	desc[4] = { "INSTANCE_WORLD",       0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[5] = { "INSTANCE_WORLD",       1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[6] = { "INSTANCE_WORLD",       2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[7] = { "INSTANCE_WORLD",       3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[8] = { "INSTANCE_MATERIAL_ID", 0, DXGI_FORMAT_R32_UINT,           1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };

	D3D12_INPUT_LAYOUT_DESC layout{};
	layout.pInputElementDescs = desc;
	layout.NumElements = nInputElementDescs;
	return layout;
}

D3D12_SHADER_BYTECODE CItemBillboardShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"VSItemBillboardInstanced",
		"vs_5_1",
		ppd3dShaderBlob
	);
}

D3D12_SHADER_BYTECODE CItemBillboardShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"PSItemBillboardUnlitAlphaClip",
		"ps_5_1",
		ppd3dShaderBlob
	);
}

D3D12_RASTERIZER_DESC CItemBillboardShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CShader::CreateRasterizerState();

	// 빌보드는 뒤집힌 면 때문에 사라지면 안 되므로 cull off
	rs.CullMode = D3D12_CULL_MODE_NONE;

	return rs;
}

D3D12_INPUT_LAYOUT_DESC CTransparentItemBillboardShader::CreateInputLayout()
{
	// 현재 CItemBillboardShader와 동일한 input layout 사용
	UINT nInputElementDescs = 9;
	auto* desc = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	desc[0] = { "POSITION",             0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[1] = { "NORMAL",               0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[2] = { "TEXCOORD",             0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[3] = { "TANGENT",              0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };

	desc[4] = { "INSTANCE_WORLD",       0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[5] = { "INSTANCE_WORLD",       1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[6] = { "INSTANCE_WORLD",       2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[7] = { "INSTANCE_WORLD",       3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[8] = { "INSTANCE_MATERIAL_ID", 0, DXGI_FORMAT_R32_UINT,           1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };

	D3D12_INPUT_LAYOUT_DESC layout{};
	layout.pInputElementDescs = desc;
	layout.NumElements = nInputElementDescs;
	return layout;
}

D3D12_SHADER_BYTECODE CTransparentItemBillboardShader::CreateVertexShader(
	ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"VSItemBillboardInstanced",
		"vs_5_1",
		ppd3dShaderBlob
	);
}

D3D12_SHADER_BYTECODE CTransparentItemBillboardShader::CreatePixelShader(
	ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"PSItemBillboardUnlitTransparentForward",
		"ps_5_1",
		ppd3dShaderBlob
	);
}

D3D12_RASTERIZER_DESC CTransparentItemBillboardShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CShader::CreateRasterizerState();
	rs.CullMode = D3D12_CULL_MODE_NONE;
	return rs;
}

D3D12_BLEND_DESC CTransparentItemBillboardShader::CreateBlendState()
{
	D3D12_BLEND_DESC bs{};
	bs.AlphaToCoverageEnable = FALSE;
	bs.IndependentBlendEnable = FALSE;

	D3D12_RENDER_TARGET_BLEND_DESC rt{};
	rt.BlendEnable = TRUE;
	rt.LogicOpEnable = FALSE;

	rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	rt.BlendOp = D3D12_BLEND_OP_ADD;

	rt.SrcBlendAlpha = D3D12_BLEND_ONE;
	rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	rt.LogicOp = D3D12_LOGIC_OP_NOOP;
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	bs.RenderTarget[0] = rt;

	return bs;
}

D3D12_DEPTH_STENCIL_DESC CTransparentItemBillboardShader::CreateDepthStencilState()
{
	D3D12_DEPTH_STENCIL_DESC ds = CShader::CreateDepthStencilState();

	ds.DepthEnable = TRUE;
	ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	return ds;
}

D3D12_INPUT_LAYOUT_DESC CMuzzleFlashBillboardShader::CreateInputLayout()
{
	UINT nInputElementDescs = 11;
	auto* desc = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	desc[0] = { "POSITION",        0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[1] = { "NORMAL",          0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[2] = { "TEXCOORD",        0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };
	desc[3] = { "TANGENT",         0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,   0 };

	desc[4] = { "INSTANCE_WORLD",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[5] = { "INSTANCE_WORLD",  1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[6] = { "INSTANCE_WORLD",  2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[7] = { "INSTANCE_WORLD",  3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };

	desc[8] = { "INSTANCE_COLOR",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[9] = { "INSTANCE_PARAMS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 80, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };
	desc[10] = { "INSTANCE_PARAMS", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 96, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 };

	D3D12_INPUT_LAYOUT_DESC layout{};
	layout.pInputElementDescs = desc;
	layout.NumElements = nInputElementDescs;
	return layout;
}

D3D12_SHADER_BYTECODE CMuzzleFlashBillboardShader::CreateVertexShader(
	ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"VSMuzzleFlashBillboardInstanced",
		"vs_5_1",
		ppd3dShaderBlob
	);
}

D3D12_SHADER_BYTECODE CMuzzleFlashBillboardShader::CreatePixelShader(
	ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"PSMuzzleFlashProcedural",
		"ps_5_1",
		ppd3dShaderBlob
	);
}

D3D12_RASTERIZER_DESC CMuzzleFlashBillboardShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CShader::CreateRasterizerState();
	rs.CullMode = D3D12_CULL_MODE_NONE;
	return rs;
}

D3D12_BLEND_DESC CMuzzleFlashBillboardShader::CreateBlendState()
{
	D3D12_BLEND_DESC bs{};
	bs.AlphaToCoverageEnable = FALSE;
	bs.IndependentBlendEnable = FALSE;

	D3D12_RENDER_TARGET_BLEND_DESC rt{};
	rt.BlendEnable = TRUE;
	rt.LogicOpEnable = FALSE;

	// additive muzzle flash
	// dst.rgb += src.rgb * src.a
	rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	rt.DestBlend = D3D12_BLEND_ONE;
	rt.BlendOp = D3D12_BLEND_OP_ADD;

	rt.SrcBlendAlpha = D3D12_BLEND_ONE;
	rt.DestBlendAlpha = D3D12_BLEND_ONE;
	rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	rt.LogicOp = D3D12_LOGIC_OP_NOOP;
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	bs.RenderTarget[0] = rt;

	return bs;
}

D3D12_DEPTH_STENCIL_DESC CMuzzleFlashBillboardShader::CreateDepthStencilState()
{
	D3D12_DEPTH_STENCIL_DESC ds = CShader::CreateDepthStencilState();

	// 벽 뒤에 있으면 가려져야 함
	ds.DepthEnable = TRUE;

	// 이펙트는 depth buffer에 쓰지 않음
	ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	return ds;
}

D3D12_INPUT_LAYOUT_DESC CSwordTrailShader::CreateInputLayout()
{
	UINT nInputElementDescs = 3;
	auto* desc = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];

	desc[0] = {
		"POSITION",
		0,
		DXGI_FORMAT_R32G32B32_FLOAT,
		0,
		0,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	};

	desc[1] = {
		"TEXCOORD",
		0,
		DXGI_FORMAT_R32G32_FLOAT,
		0,
		12,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	};

	desc[2] = {
		"COLOR",
		0,
		DXGI_FORMAT_R32G32B32A32_FLOAT,
		0,
		20,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	};

	D3D12_INPUT_LAYOUT_DESC layout{};
	layout.pInputElementDescs = desc;
	layout.NumElements = nInputElementDescs;
	return layout;
}

D3D12_SHADER_BYTECODE CSwordTrailShader::CreateVertexShader(
	ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"VSSwordTrail",
		"vs_5_1",
		ppd3dShaderBlob
	);
}

D3D12_SHADER_BYTECODE CSwordTrailShader::CreatePixelShader(
	ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"PSSwordTrailProcedural",
		"ps_5_1",
		ppd3dShaderBlob
	);
}

D3D12_RASTERIZER_DESC CSwordTrailShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CShader::CreateRasterizerState();
	rs.CullMode = D3D12_CULL_MODE_NONE;
	return rs;
}

D3D12_BLEND_DESC CSwordTrailShader::CreateBlendState()
{
	D3D12_BLEND_DESC bs{};
	bs.AlphaToCoverageEnable = FALSE;
	bs.IndependentBlendEnable = FALSE;

	D3D12_RENDER_TARGET_BLEND_DESC rt{};
	rt.BlendEnable = TRUE;
	rt.LogicOpEnable = FALSE;

	// 밝게 빛나는 검 궤적. 정렬 문제를 줄이기 위해 additive 사용.
	rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	rt.DestBlend = D3D12_BLEND_ONE;
	rt.BlendOp = D3D12_BLEND_OP_ADD;

	rt.SrcBlendAlpha = D3D12_BLEND_ONE;
	rt.DestBlendAlpha = D3D12_BLEND_ONE;
	rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	rt.LogicOp = D3D12_LOGIC_OP_NOOP;
	rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	bs.RenderTarget[0] = rt;

	return bs;
}

D3D12_DEPTH_STENCIL_DESC CSwordTrailShader::CreateDepthStencilState()
{
	D3D12_DEPTH_STENCIL_DESC ds = CShader::CreateDepthStencilState();

	ds.DepthEnable = TRUE;
	ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	return ds;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//

D3D12_SHADER_BYTECODE CShadowMapStaticShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"VSShadowMapStaticInstanced",
		"vs_5_1",
		ppd3dShaderBlob
	);
}

D3D12_SHADER_BYTECODE CShadowMapStaticShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	if ( ppd3dShaderBlob )
		*ppd3dShaderBlob = nullptr;

	D3D12_SHADER_BYTECODE byteCode{};
	byteCode.pShaderBytecode = nullptr;
	byteCode.BytecodeLength = 0;
	return byteCode;
}

D3D12_SHADER_BYTECODE CShadowMapAlphaClipStaticShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"PSShadowMapAlphaClip",
		"ps_5_1",
		ppd3dShaderBlob
	);
}

D3D12_RASTERIZER_DESC CShadowMapStaticShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CShader::CreateRasterizerState();
	rs.CullMode = D3D12_CULL_MODE_BACK;
	rs.DepthBias = 12000;
	rs.SlopeScaledDepthBias = 0.75f;
	rs.DepthBiasClamp = 0.0f;
	return rs;
}

D3D12_RASTERIZER_DESC CShadowMapAlphaClipStaticShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CShader::CreateRasterizerState();
	rs.CullMode = D3D12_CULL_MODE_NONE;
	rs.DepthBias = 12000;
	rs.SlopeScaledDepthBias = 0.75f;
	rs.DepthBiasClamp = 0.0f;
	return rs;
}

D3D12_SHADER_BYTECODE CShadowMapSkinnedShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"VSShadowMapSkinnedInstanced",
		"vs_5_1",
		ppd3dShaderBlob
	);
}

D3D12_SHADER_BYTECODE CShadowMapSkinnedShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	if ( ppd3dShaderBlob )
		*ppd3dShaderBlob = nullptr;

	D3D12_SHADER_BYTECODE byteCode{};
	byteCode.pShaderBytecode = nullptr;
	byteCode.BytecodeLength = 0;
	return byteCode;
}

D3D12_SHADER_BYTECODE CShadowMapAlphaClipSkinnedShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"PSShadowMapAlphaClip",
		"ps_5_1",
		ppd3dShaderBlob
	);
}

D3D12_RASTERIZER_DESC CShadowMapSkinnedShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CShader::CreateRasterizerState();
	rs.CullMode = D3D12_CULL_MODE_BACK;
	rs.DepthBias = 12000;
	rs.SlopeScaledDepthBias = 0.75f;
	rs.DepthBiasClamp = 0.0f;
	return rs;
}

D3D12_RASTERIZER_DESC CShadowMapAlphaClipSkinnedShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CShader::CreateRasterizerState();
	rs.CullMode = D3D12_CULL_MODE_NONE;
	rs.DepthBias = 12000;
	rs.SlopeScaledDepthBias = 0.75f;
	rs.DepthBiasClamp = 0.0f;
	return rs;
}

D3D12_SHADER_BYTECODE CAlphaClipSkinnedObjectsShader::CreatePixelShader(
	ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"PSTexturedLightingToMultipleRTs_AlphaClip",
		"ps_5_1",
		ppd3dShaderBlob
	);
}

D3D12_RASTERIZER_DESC CAlphaClipSkinnedObjectsShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CShader::CreateRasterizerState();

	// 활/잎/얇은 alpha-cutout 계열은 backface culling 때문에 비어 보일 수 있음.
	rs.CullMode = D3D12_CULL_MODE_NONE;

	return rs;
}

D3D12_SHADER_BYTECODE COcclusionStaticShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"VSStaticOcclusionInstanced",
		"vs_5_1",
		ppd3dShaderBlob
	);
}

D3D12_SHADER_BYTECODE COcclusionStaticShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"PSOcclusionOpaque",
		"ps_5_1",
		ppd3dShaderBlob
	);
}

D3D12_RASTERIZER_DESC COcclusionStaticShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CShader::CreateRasterizerState();
	rs.CullMode = D3D12_CULL_MODE_NONE;
	return rs;
}

D3D12_BLEND_DESC COcclusionStaticShader::CreateBlendState()
{
	D3D12_BLEND_DESC bs = CShader::CreateBlendState();
	bs.RenderTarget[0].RenderTargetWriteMask = 0;
	return bs;
}

D3D12_DEPTH_STENCIL_DESC COcclusionStaticShader::CreateDepthStencilState()
{
	D3D12_DEPTH_STENCIL_DESC ds{};
	::ZeroMemory(&ds, sizeof(D3D12_DEPTH_STENCIL_DESC));

	ds.DepthEnable = TRUE;
	ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ds.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	ds.StencilEnable = FALSE;
	ds.StencilReadMask = 0x00;
	ds.StencilWriteMask = 0x00;

	ds.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	ds.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	ds.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	ds.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	ds.BackFace = ds.FrontFace;

	return ds;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CUIShader::CUIShader()
{
}

CUIShader::~CUIShader()
{
}

D3D12_RASTERIZER_DESC CUIShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CTexturedShader::CreateRasterizerState();
	rs.CullMode = D3D12_CULL_MODE_NONE;      // UI는 양면/쿼드가 많아서 Cull OFF가 안전
	rs.DepthClipEnable = TRUE;
	return rs;
}

D3D12_BLEND_DESC CUIShader::CreateBlendState()
{
	D3D12_BLEND_DESC bs{};
	::ZeroMemory(&bs, sizeof(D3D12_BLEND_DESC));

	bs.AlphaToCoverageEnable = FALSE;
	bs.IndependentBlendEnable = FALSE;

	auto& rt0 = bs.RenderTarget[0];
	rt0.BlendEnable = TRUE;
	rt0.LogicOpEnable = FALSE;

	// Standard alpha blending:
	// out.rgb = src.rgb * src.a + dst.rgb * (1 - src.a)
	rt0.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	rt0.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	rt0.BlendOp = D3D12_BLEND_OP_ADD;

	// alpha channel blending (보통 이대로면 충분)
	rt0.SrcBlendAlpha = D3D12_BLEND_ONE;
	rt0.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	rt0.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	rt0.LogicOp = D3D12_LOGIC_OP_NOOP;
	rt0.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	return bs;
}

D3D12_DEPTH_STENCIL_DESC CUIShader::CreateDepthStencilState()
{
	D3D12_DEPTH_STENCIL_DESC ds{};
	::ZeroMemory(&ds, sizeof(D3D12_DEPTH_STENCIL_DESC));

	ds.DepthEnable = FALSE;                          // UI는 depth test 자체를 끈다
	ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // (의미는 거의 없지만 명시)
	ds.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	ds.StencilEnable = FALSE;
	ds.StencilReadMask = 0x00;
	ds.StencilWriteMask = 0x00;

	ds.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	ds.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	ds.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	ds.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;

	ds.BackFace = ds.FrontFace;

	return ds;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CRectUIShader
void CRectUIShader::CreateShader(
	ID3D12Device* dev,
	ID3D12RootSignature* sceneRootSig,
	UINT nRenderTargets,
	DXGI_FORMAT* rtvFormats,
	DXGI_FORMAT dsvFormat)
{
	// ★ m_pd3dGraphicsRootSignature에 저장하지 않는다.
	//    (CShader::OnPrepareRender가 RootSig를 다시 Set하면,
	//     Scene에서 잡아둔 DescriptorTable이 무효화될 여지가 있어서)
	CShader::CreateShader(
		dev, 
		sceneRootSig,
		nRenderTargets, 
		rtvFormats, 
		dsvFormat);
}

D3D12_RASTERIZER_DESC CRectUIShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CTextureToFullScreenShader::CreateRasterizerState();
	rs.CullMode = D3D12_CULL_MODE_NONE;
	return rs;
}

D3D12_BLEND_DESC CRectUIShader::CreateBlendState()
{
	D3D12_BLEND_DESC bs{};
	::ZeroMemory(&bs, sizeof(bs));

	bs.AlphaToCoverageEnable = FALSE;
	bs.IndependentBlendEnable = FALSE;

	auto& rt0 = bs.RenderTarget[0];
	rt0.BlendEnable = TRUE;
	rt0.LogicOpEnable = FALSE;

	rt0.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	rt0.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	rt0.BlendOp = D3D12_BLEND_OP_ADD;

	rt0.SrcBlendAlpha = D3D12_BLEND_ONE;
	rt0.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	rt0.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	rt0.LogicOp = D3D12_LOGIC_OP_NOOP;
	rt0.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	return bs;
}

void CRectUIShader::UpdateShaderVariables(ID3D12GraphicsCommandList* cmd, void* pContext)
{
	if ( !cmd ) return;
	if ( !m_pd3dcbDrawOptions || !m_pcbMappedDrawOptions ) return;
	if ( !pContext ) return;

	UINT nIndex = m_nDrawOptionWriteIndex;
	if ( nIndex >= m_nMaxDrawOptionEntries )
		nIndex = m_nMaxDrawOptionEntries - 1;
	else
		++m_nDrawOptionWriteIndex;

	const UINT nOffset = m_nDrawOptionsStride * nIndex;

	auto* pDst = reinterpret_cast< PS_CB_DRAW_OPTIONS* >( m_pcbMappedDrawOptions + nOffset );
	const auto* pSrc = reinterpret_cast< const PS_CB_DRAW_OPTIONS* >( pContext );

	*pDst = *pSrc;

	cmd->SetGraphicsRootConstantBufferView(
		ROOT_PARAMETER_DRAW_OPTIONS,
		m_pd3dcbDrawOptions->GetGPUVirtualAddress() + nOffset
	);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CDepthFogShader
void CDepthFogShader::CreateShader(
	ID3D12Device* dev,
	ID3D12RootSignature* sceneRootSig,
	UINT nRenderTargets,
	DXGI_FORMAT* rtvFormats,
	DXGI_FORMAT dsvFormat)
{
	// Scene이 이미 루트시그니처와 SRV 디스크립터 테이블을 세팅하므로
	// 여기서는 Scene RootSig를 그대로 사용한다.
	CShader::CreateShader(
		dev,
		sceneRootSig,
		nRenderTargets,
		rtvFormats,
		dsvFormat);
}

D3D12_SHADER_BYTECODE CDepthFogShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return CShader::CompileShaderFromFile(
		L"Shaders.hlsl",
		"PSDepthFog",
		"ps_5_1",
		ppd3dShaderBlob
	);
}

D3D12_RASTERIZER_DESC CDepthFogShader::CreateRasterizerState()
{
	D3D12_RASTERIZER_DESC rs = CTextureToFullScreenShader::CreateRasterizerState();
	rs.CullMode = D3D12_CULL_MODE_NONE;
	return rs;
}

D3D12_BLEND_DESC CDepthFogShader::CreateBlendState()
{
	D3D12_BLEND_DESC bs{};
	::ZeroMemory(&bs, sizeof(bs));

	bs.AlphaToCoverageEnable = FALSE;
	bs.IndependentBlendEnable = FALSE;

	auto& rt0 = bs.RenderTarget[0];
	rt0.BlendEnable = FALSE;
	rt0.LogicOpEnable = FALSE;
	rt0.SrcBlend = D3D12_BLEND_ONE;
	rt0.DestBlend = D3D12_BLEND_ZERO;
	rt0.BlendOp = D3D12_BLEND_OP_ADD;
	rt0.SrcBlendAlpha = D3D12_BLEND_ONE;
	rt0.DestBlendAlpha = D3D12_BLEND_ZERO;
	rt0.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	rt0.LogicOp = D3D12_LOGIC_OP_NOOP;
	rt0.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	return bs;
}

D3D12_DEPTH_STENCIL_DESC CDepthFogShader::CreateDepthStencilState()
{
	D3D12_DEPTH_STENCIL_DESC ds{};
	::ZeroMemory(&ds, sizeof(ds));

	ds.DepthEnable = FALSE;
	ds.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ds.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	ds.StencilEnable = FALSE;
	ds.StencilReadMask = 0x00;
	ds.StencilWriteMask = 0x00;

	ds.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	ds.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	ds.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	ds.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;

	ds.BackFace = ds.FrontFace;

	return ds;
}

void CDepthFogShader::UpdateShaderVariables(ID3D12GraphicsCommandList* cmd, void* pContext)
{
	if ( !cmd ) return;
	if ( !m_pd3dcbDrawOptions || !m_pcbMappedDrawOptions ) return;
	if ( !pContext ) return;

	UINT nIndex = m_nDrawOptionWriteIndex;
	if ( nIndex >= m_nMaxDrawOptionEntries )
		nIndex = m_nMaxDrawOptionEntries - 1;
	else
		++m_nDrawOptionWriteIndex;

	const UINT nOffset = m_nDrawOptionsStride * nIndex;

	auto* pDst = reinterpret_cast< PS_CB_DRAW_OPTIONS* >( m_pcbMappedDrawOptions + nOffset );
	const auto* pSrc = reinterpret_cast< const PS_CB_DRAW_OPTIONS* >( pContext );

	*pDst = *pSrc;

	cmd->SetGraphicsRootConstantBufferView(
		ROOT_PARAMETER_DRAW_OPTIONS,
		m_pd3dcbDrawOptions->GetGPUVirtualAddress() + nOffset
	);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CPostProcessingShader::CPostProcessingShader()
{
}

CPostProcessingShader::~CPostProcessingShader()
{
}

D3D12_INPUT_LAYOUT_DESC CPostProcessingShader::CreateInputLayout()
{
	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
	d3dInputLayoutDesc.pInputElementDescs = nullptr;
	d3dInputLayoutDesc.NumElements = 0;

	return(d3dInputLayoutDesc);
}

D3D12_DEPTH_STENCIL_DESC CPostProcessingShader::CreateDepthStencilState()
{
	D3D12_DEPTH_STENCIL_DESC d3dDepthStencilDesc;
	::ZeroMemory(&d3dDepthStencilDesc, sizeof(D3D12_DEPTH_STENCIL_DESC));
	d3dDepthStencilDesc.DepthEnable = FALSE;
	d3dDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	d3dDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	d3dDepthStencilDesc.StencilEnable = FALSE;
	d3dDepthStencilDesc.StencilReadMask = 0x00;
	d3dDepthStencilDesc.StencilWriteMask = 0x00;
	d3dDepthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	d3dDepthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;

	return(d3dDepthStencilDesc);
}

void CPostProcessingShader::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
{
	D3D12_DESCRIPTOR_RANGE pd3dDescriptorRanges[1];

	pd3dDescriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	pd3dDescriptorRanges[0].NumDescriptors = 5;
	pd3dDescriptorRanges[0].BaseShaderRegister = 1; //Texture
	pd3dDescriptorRanges[0].RegisterSpace = 0;
	pd3dDescriptorRanges[0].OffsetInDescriptorsFromTableStart = 0;

	D3D12_ROOT_PARAMETER pd3dRootParameters[1];

	pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	pd3dRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
	pd3dRootParameters[0].DescriptorTable.pDescriptorRanges = &pd3dDescriptorRanges[0]; //Texture
	pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_STATIC_SAMPLER_DESC d3dSamplerDesc;
	::ZeroMemory(&d3dSamplerDesc, sizeof(D3D12_STATIC_SAMPLER_DESC));
	d3dSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	d3dSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	d3dSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	d3dSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	d3dSamplerDesc.MipLODBias = 0;
	d3dSamplerDesc.MaxAnisotropy = 1;
	d3dSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	d3dSamplerDesc.MinLOD = 0;
	d3dSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	d3dSamplerDesc.ShaderRegister = 0;
	d3dSamplerDesc.RegisterSpace = 0;
	d3dSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
	::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
	d3dRootSignatureDesc.NumParameters = _countof(pd3dRootParameters);
	d3dRootSignatureDesc.pParameters = pd3dRootParameters;
	d3dRootSignatureDesc.NumStaticSamplers = 1;
	d3dRootSignatureDesc.pStaticSamplers = &d3dSamplerDesc;
	d3dRootSignatureDesc.Flags = d3dRootSignatureFlags;

	ComPtr<ID3DBlob> pd3dSignatureBlob;
	ComPtr<ID3DBlob> pd3dErrorBlob;

	D3D12SerializeRootSignature(
		&d3dRootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&pd3dSignatureBlob,
		&pd3dErrorBlob
	);

	pd3dDevice->CreateRootSignature(
		0,
		pd3dSignatureBlob->GetBufferPointer(),
		pd3dSignatureBlob->GetBufferSize(),
		__uuidof(ID3D12RootSignature),
		(void**)&m_pd3dGraphicsRootSignature
	);

	if (pd3dSignatureBlob)
		pd3dSignatureBlob.Reset();

	if (pd3dErrorBlob)
		pd3dErrorBlob.Reset();
}

D3D12_SHADER_BYTECODE CPostProcessingShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSPostProcessing", "vs_5_1", ppd3dShaderBlob));
}

D3D12_SHADER_BYTECODE CPostProcessingShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "PSPostProcessing", "ps_5_1", ppd3dShaderBlob));
}

void CPostProcessingShader::CreateShader(
	ID3D12Device* pd3dDevice, 
	ID3D12RootSignature* pd3dGraphicsRootSignature, 
	UINT nRenderTargets, 
	DXGI_FORMAT* pdxgiRtvFormats, 
	DXGI_FORMAT dxgiDsvFormat)
{
#ifdef _WITH_SCENE_ROOT_SIGNATURE
	m_pd3dGraphicsRootSignature = pd3dGraphicsRootSignature;
#else
	CreateGraphicsRootSignature(pd3dDevice);
#endif

	CShader::CreateShader(
		pd3dDevice,
		m_pd3dGraphicsRootSignature.Get(),
		nRenderTargets,
		pdxgiRtvFormats,
		dxgiDsvFormat);
}

void CPostProcessingShader::CreateResourcesAndRtvsSrvs(
	ID3D12Device* pd3dDevice, 
	ID3D12GraphicsCommandList* pd3dCommandList, 
	UINT nRenderTargets, 
	DXGI_FORMAT* pdxgiFormats, 
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle)
{
	m_pTexture = make_shared<CTexture>(nRenderTargets, RESOURCE_TEXTURE2D, 0, 1);

	D3D12_CLEAR_VALUE d3dClearValue = { DXGI_FORMAT_R8G8B8A8_UNORM, { 1.0f, 1.0f, 1.0f, 1.0f } };
	for (UINT i = 0; i < nRenderTargets; i++)
	{
		d3dClearValue.Format = pdxgiFormats[i];
		m_pTexture->CreateTexture(
			pd3dDevice,
			FRAME_BUFFER_WIDTH,
			FRAME_BUFFER_HEIGHT,
			pdxgiFormats[i],
			D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COMMON,
			&d3dClearValue,
			RESOURCE_TEXTURE2D,
			i);
	}

	CreateShaderVariables(pd3dDevice, pd3dCommandList);
#ifdef _WITH_SCENE_ROOT_SIGNATURE
	CScene::m_pDescriptorHeap->CreateShaderResourceViewsOther(
		pd3dDevice,
		m_pTexture.get(),
		ROOT_PARAMETER_GLOBAL_SRV);

#else
	CScene::m_pDescriptorHeap->CreateShaderResourceViewsOther(
		pd3dDevice,
		m_pTexture.get(),
		0);

#endif

	D3D12_RENDER_TARGET_VIEW_DESC d3dRenderTargetViewDesc{};
	d3dRenderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	d3dRenderTargetViewDesc.Texture2D.MipSlice = 0;
	d3dRenderTargetViewDesc.Texture2D.PlaneSlice = 0;

	m_pd3dRtvCPUDescriptorHandles = make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(nRenderTargets);

	for (UINT i = 0; i < nRenderTargets; i++)
	{
		d3dRenderTargetViewDesc.Format = pdxgiFormats[i];
		ID3D12Resource* pd3dTextureResource = m_pTexture->GetResource(i);
		pd3dDevice->CreateRenderTargetView(
			pd3dTextureResource, 
			&d3dRenderTargetViewDesc, 
			d3dRtvCPUDescriptorHandle);
		m_pd3dRtvCPUDescriptorHandles[i] = d3dRtvCPUDescriptorHandle;
		d3dRtvCPUDescriptorHandle.ptr += ::gnRtvDescriptorIncrementSize;
	}
}

void CPostProcessingShader::OnPrepareRenderTarget(
	ID3D12GraphicsCommandList* pd3dCommandList,
	int nRenderTargets,
	D3D12_CPU_DESCRIPTOR_HANDLE* pd3dRtvCPUHandles,
	D3D12_CPU_DESCRIPTOR_HANDLE* pd3dDsvCPUHandle
)
{
	int nResources = m_pTexture->GetTextures();
	unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]> pd3dAllRtvCPUHandles = make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(nRenderTargets + nResources);

	for (int i = 0; i < nRenderTargets; i++)
	{
		pd3dAllRtvCPUHandles[i] = pd3dRtvCPUHandles[i];
		pd3dCommandList->ClearRenderTargetView(pd3dRtvCPUHandles[i], Colors::SkyBlue, 0, nullptr);
	}

	for (int i = 0; i < nResources; i++)
	{
		::SynchronizeResourceTransition(
			pd3dCommandList,
			GetTextureResource(i),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);

		D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = GetRtvCPUDescriptorHandle(i);
		pd3dCommandList->ClearRenderTargetView(d3dRtvCPUDescriptorHandle, Colors::White, 0, nullptr);
		pd3dAllRtvCPUHandles[nRenderTargets + i] = d3dRtvCPUDescriptorHandle;
	}
	pd3dCommandList->OMSetRenderTargets(nRenderTargets + nResources, pd3dAllRtvCPUHandles.get(), FALSE, pd3dDsvCPUHandle);
}

void CPostProcessingShader::OnPrepareSceneRenderTargets(
	ID3D12GraphicsCommandList* pd3dCommandList,
	D3D12_CPU_DESCRIPTOR_HANDLE* pd3dDsvCPUHandle
)
{
	if ( !pd3dCommandList ) return;
	if ( !m_pTexture ) return;
	if ( !m_pd3dRtvCPUDescriptorHandles ) return;

	const int nResources = m_pTexture->GetTextures();
	if ( nResources <= 0 ) return;

	for ( int i = 0; i < nResources; ++i )
	{
		::SynchronizeResourceTransition(
			pd3dCommandList,
			GetTextureResource(i),
			D3D12_RESOURCE_STATE_COMMON,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);

		D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = GetRtvCPUDescriptorHandle(i);
		pd3dCommandList->ClearRenderTargetView(
			d3dRtvCPUDescriptorHandle,
			Colors::White,
			0,
			nullptr
		);
	}

	pd3dCommandList->OMSetRenderTargets(
		nResources,
		m_pd3dRtvCPUDescriptorHandles.get(),
		FALSE,
		pd3dDsvCPUHandle
	);
}

void CPostProcessingShader::OnPostRenderTarget(ID3D12GraphicsCommandList* pd3dCommandList)
{
	int nResources = m_pTexture->GetTextures();
	for (int i = 0; i < nResources; i++)
	{
		::SynchronizeResourceTransition(
			pd3dCommandList,
			GetTextureResource(i),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_COMMON
		);
	}
}

void CPostProcessingShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext)
{
	CShader::Render(pd3dCommandList, pCamera, pContext);

	pd3dCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pd3dCommandList->DrawInstanced(6, 1, 0, 0);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
CTextureToFullScreenShader::CTextureToFullScreenShader()
{
}

CTextureToFullScreenShader::~CTextureToFullScreenShader()
{
	ReleaseShaderVariables();
}

void CTextureToFullScreenShader::ReleaseShaderVariables()
{
	if ( m_pd3dcbDrawOptions && m_pcbMappedDrawOptions )
	{
		m_pd3dcbDrawOptions->Unmap(0, nullptr);
		m_pcbMappedDrawOptions = nullptr;
	}

	m_nDrawOptionsStride = 0;
	m_nDrawOptionWriteIndex = 0;

	if ( m_pd3dcbDrawOptions )
		m_pd3dcbDrawOptions.Reset();
}

D3D12_SHADER_BYTECODE CTextureToFullScreenShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "VSScreenRectSamplingTextured", "vs_5_1", ppd3dShaderBlob));
}

D3D12_SHADER_BYTECODE CTextureToFullScreenShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return(CShader::CompileShaderFromFile(L"Shaders.hlsl", "PSScreenRectSamplingTextured", "ps_5_1", ppd3dShaderBlob));
}

void CTextureToFullScreenShader::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	m_nDrawOptionsStride = ( ( sizeof(PS_CB_DRAW_OPTIONS) + 255 ) & ~255 );
	const UINT nBufferBytes = m_nDrawOptionsStride * m_nMaxDrawOptionEntries;

	m_pd3dcbDrawOptions = ::CreateBufferResource(
		pd3dDevice,
		pd3dCommandList,
		nullptr,
		nBufferBytes,
		D3D12_HEAP_TYPE_UPLOAD,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		nullptr);

	m_pd3dcbDrawOptions->Map(0, nullptr, reinterpret_cast< void** >( &m_pcbMappedDrawOptions ));
	m_nDrawOptionWriteIndex = 0;

	CPostProcessingShader::CreateShaderVariables(pd3dDevice, pd3dCommandList);
}

void CTextureToFullScreenShader::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList, void* pContext)
{
	if ( !pd3dCommandList ) return;
	if ( !m_pd3dcbDrawOptions || !m_pcbMappedDrawOptions ) return;
	if ( !pContext ) return;

	UINT nIndex = m_nDrawOptionWriteIndex;
	if ( nIndex >= m_nMaxDrawOptionEntries )
		nIndex = m_nMaxDrawOptionEntries - 1;
	else
		++m_nDrawOptionWriteIndex;

	const UINT nOffset = m_nDrawOptionsStride * nIndex;

	auto* pDrawOptions = reinterpret_cast< PS_CB_DRAW_OPTIONS* >( m_pcbMappedDrawOptions + nOffset );
	::ZeroMemory(pDrawOptions, sizeof(PS_CB_DRAW_OPTIONS));

	pDrawOptions->m_xmn4DrawOptions.x = *( ( int* ) pContext );

	pDrawOptions->m_xmf4UiRect = XMFLOAT4(
		FRAME_BUFFER_WIDTH * 0.5f,
		FRAME_BUFFER_HEIGHT * 0.5f,
		static_cast< float >( FRAME_BUFFER_WIDTH ),
		static_cast< float >( FRAME_BUFFER_HEIGHT )
	);

	pDrawOptions->m_xmf4Viewport = XMFLOAT4(
		static_cast< float >( FRAME_BUFFER_WIDTH ),
		static_cast< float >( FRAME_BUFFER_HEIGHT ),
		1.0f / static_cast< float >( FRAME_BUFFER_WIDTH ),
		1.0f / static_cast< float >( FRAME_BUFFER_HEIGHT )
	);

	const D3D12_GPU_VIRTUAL_ADDRESS d3dGpuVirtualAddress =
		m_pd3dcbDrawOptions->GetGPUVirtualAddress() + nOffset;

	pd3dCommandList->SetGraphicsRootConstantBufferView(
		ROOT_PARAMETER_DRAW_OPTIONS,
		d3dGpuVirtualAddress
	);

	CPostProcessingShader::UpdateShaderVariables(pd3dCommandList, pContext);
}

void CTextureToFullScreenShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera, void* pContext)
{
	CPostProcessingShader::Render(pd3dCommandList, pCamera, pContext);
}
