#include "ShapshickRenderer.h"
#include <DirectXColors.h>

using namespace DirectX;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		ShapShickRenderer theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

ShapShickRenderer::ShapShickRenderer(HINSTANCE hInstance)
	: DXApp(hInstance)
{
}

ShapShickRenderer::~ShapShickRenderer()
{
}

bool ShapShickRenderer::Initialize()
{
	if (!DXApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	BuildTriangle();
	BuildConstantBuffer();
	BuildRootSignature();
	BuildShadersAndInputLayout();	
	BuildPSO();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	return true;
}

void ShapShickRenderer::OnResize()
{
	DXApp::OnResize();
}

void ShapShickRenderer::Update(const Timer& gt)
{
	const float translationSpeed = 0.0002f;
	const float offsetBounds = 1.25f;

	mConstantBufferData.offset.x += translationSpeed;

	if (mConstantBufferData.offset.x >= offsetBounds)
		mConstantBufferData.offset.x = -offsetBounds;

	memcpy(mCbvDataBegin, &mConstantBufferData, sizeof(mConstantBufferData));
}

void ShapShickRenderer::Draw(const Timer& gt)
{

	ThrowIfFailed(mDirectCmdListAlloc->Reset());
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPipelineStateObject.Get()));

	// populate the command list
	{
		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

		mCommandList->RSSetViewports(1, &mScreenViewport);
		mCommandList->RSSetScissorRects(1, &mScissorRect);

		mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);

		mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);

		mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ID3D12DescriptorHeap* ppHeaps[] = { mCbvHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
		mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());


		mCommandList->DrawInstanced(3, 1, 0, 0);

		mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	}

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Wait until frame commands are complete.  This waiting is inefficient and is done for simplicity. 
	FlushCommandQueue();
}

void ShapShickRenderer::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER1 rootParameters[1] = {};
	CD3DX12_DESCRIPTOR_RANGE1 cbvTable = {};

	rootParameters[0].InitAsDescriptorTable(1, &cbvTable, D3D12_SHADER_VISIBILITY_VERTEX);
	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);	

	// create a empty root signature
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedSignature = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeVersionedRootSignature(&rootSignatureDesc, serializedSignature.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr) 
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	
	ThrowIfFailed(hr);

	ThrowIfFailed(mDevice->CreateRootSignature(0, serializedSignature->GetBufferPointer(), serializedSignature->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
}

void ShapShickRenderer::BuildShadersAndInputLayout()
{
	mVertexShaderByteCode	= DXUtil::CompileShader(L"Shaders/Shader.hlsl", nullptr, "VS", "vs_5_0");
	mPixelShaderByteCode	= DXUtil::CompileShader(L"Shaders/Shader.hlsl", nullptr, "PS", "ps_5_0");

	mInputLayout = {
		{ "POSITION",	0, DXGI_FORMAT_R32G32_FLOAT,		0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",		0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void ShapShickRenderer::BuildTriangle()
{
	struct Vertex {
		XMFLOAT2 position;
		XMFLOAT4 color;
	};

	std::array<Vertex, 3> vertices = {
				// positions				// colors
		Vertex({XMFLOAT2(0.0f,    0.25f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f)}),
		Vertex({XMFLOAT2(0.25f,  -0.25f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f)}),
		Vertex({XMFLOAT2(-0.25f, -0.25f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f)})
	};

	const unsigned int vBufferSize = sizeof(vertices);

	ThrowIfFailed(D3DCreateBlob(vBufferSize, &mVertexBufferCPU));
	CopyMemory(mVertexBufferCPU->GetBufferPointer(), vertices.data(), vBufferSize);

	// send vertex buffer to the GPU
	mVertexBufferGPU = DXUtil::CreateDefaultBuffer(mDevice.Get(), mCommandList.Get(), vertices.data(), vBufferSize, mVertexBufferUploader);

	// set vertex buffer view
	mVertexBufferView.BufferLocation = mVertexBufferGPU->GetGPUVirtualAddress();
	mVertexBufferView.StrideInBytes = sizeof(Vertex);
	mVertexBufferView.SizeInBytes = vBufferSize;
}

void ShapShickRenderer::BuildConstantBuffer()
{
	// describe constant buffer heap
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;	
	ThrowIfFailed(mDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap)));

	const UINT	constantBufferSize = sizeof(SceneConstantBuffer);
	
	// create constant buffer
	ThrowIfFailed(mDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mConstantBuffer)));

	// describe and create constant buffer view
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = mConstantBuffer->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = constantBufferSize;
	mDevice->CreateConstantBufferView(&cbvDesc, mCbvHeap->GetCPUDescriptorHandleForHeapStart());

	// map and initialize constant buffer. don't unmap until the app closes.
	CD3DX12_RANGE readRange(0, 0); // we don't intend to read from this resource on the CPU.
	ThrowIfFailed(mConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mCbvDataBegin)));
	memcpy(mCbvDataBegin, &mConstantBufferData, sizeof(mConstantBufferData));
}

void ShapShickRenderer::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.VS = CD3DX12_SHADER_BYTECODE(mVertexShaderByteCode.Get());
	psoDesc.PS = CD3DX12_SHADER_BYTECODE(mPixelShaderByteCode.Get());
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.DSVFormat = mDepthStencilFormat;
	psoDesc.SampleDesc = { 1, 0 };

	ThrowIfFailed(mDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPipelineStateObject)));
}
