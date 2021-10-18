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

	BuildBuffers();
	BuildConstantBuffer();
	LoadTexture();
	BuildDescriptorHeaps();
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

float translationSpeedX = 0.0002f;
float translationSpeedY = 0.0001f;
void ShapShickRenderer::Update(const Timer& gt)
{
	const float upperBound = 0.75f;
	const float lowerBound = -0.75f;
	const float rightBound = 0.75f;
	const float leftBound = -0.75f;

// 	std::wstring text = L"\nposX: " + std::to_wstring(mConstantBufferData.position.x);
// 	OutputDebugString(text.c_str());

	mConstantBufferData.position.x += translationSpeedX;
	mConstantBufferData.position.y += translationSpeedY;

	// if texture is touching the left or right borders, change direction of movement
	if (mConstantBufferData.position.x >= rightBound || mConstantBufferData.position.x <= leftBound) 
		translationSpeedX = -translationSpeedX;	
	
	// if texture is touching the upper or lower borders, change direction of movement
	if (mConstantBufferData.position.y >= upperBound || mConstantBufferData.position.y <= lowerBound)
		translationSpeedY = -translationSpeedY;

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

		mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Black, 0, nullptr);

		mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

		mCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
		mCommandList->IASetIndexBuffer(&mIndexBufferView);		

		mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// set cbv heap
		ID3D12DescriptorHeap* cbvDescriptorHeaps[] = { mCbvHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(cbvDescriptorHeaps), cbvDescriptorHeaps);
		mCommandList->SetGraphicsRootDescriptorTable(0, mCbvHeap->GetGPUDescriptorHandleForHeapStart());

		// set srv heap
		ID3D12DescriptorHeap* srvDescriptorHeaps[] = { mSrvHeap.Get() };
		mCommandList->SetDescriptorHeaps(_countof(srvDescriptorHeaps), srvDescriptorHeaps);

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvHeap->GetGPUDescriptorHandleForHeapStart());
		mCommandList->SetGraphicsRootDescriptorTable(1, tex);

		mCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

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
	CD3DX12_ROOT_PARAMETER1 rootParameters[2] = {};
	CD3DX12_DESCRIPTOR_RANGE1 cbvTable = {}; 
	CD3DX12_DESCRIPTOR_RANGE1 srvTable = {}; 

	cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

	rootParameters[0].InitAsDescriptorTable(1, &cbvTable, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParameters[1].InitAsDescriptorTable(1, &srvTable, D3D12_SHADER_VISIBILITY_PIXEL);	

	// sampler for texture
	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER); // addressW

	// create the root signature
	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 1, &pointClamp, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
		{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,	0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void ShapShickRenderer::BuildBuffers()
{
	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT2 texCoord;
	};

	Vertex vertices[] = {
		// positions				// texCoords
		{ { -0.25f, -0.25f , 0.0f}, { 0.0f, 1.0f } },
		{ {  0.25f, -0.25f , 0.0f}, { 1.0f, 1.0f } },
		{ { -0.25f,  0.25f , 0.0f}, { 0.0f, 0.0f } },
		{ {  0.25f,  0.25f , 0.0f}, { 1.0f, 0.0f } }
	};

	// set indices
	uint16_t indices[] = {
		0, 2, 1,
		1, 2, 3
	};

	// sizes of buffers in terms of bytes
	const UINT vbByteSize = sizeof(vertices);
	const UINT ibByteSize = sizeof(indices);

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mVertexBufferCPU));
	CopyMemory(mVertexBufferCPU->GetBufferPointer(), &vertices, vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mIndexBufferCPU));
	CopyMemory(mIndexBufferCPU->GetBufferPointer(), &indices, ibByteSize);

	// send buffers to the gpu
	mVertexBufferGPU = DXUtil::CreateDefaultBuffer(mDevice.Get(),
		mCommandList.Get(), &vertices, vbByteSize, mVertexBufferUploader);

	mIndexBufferGPU = DXUtil::CreateDefaultBuffer(mDevice.Get(),
		mCommandList.Get(), &indices, ibByteSize, mIndexBufferUploader);

	// set vertex buffer view
	mVertexBufferView.BufferLocation = mVertexBufferGPU->GetGPUVirtualAddress();
	mVertexBufferView.StrideInBytes = sizeof(Vertex);
	mVertexBufferView.SizeInBytes = vbByteSize;

	// set index buffer view
	mIndexBufferView.BufferLocation = mIndexBufferGPU->GetGPUVirtualAddress();
	mIndexBufferView.Format = DXGI_FORMAT_R16_UINT;
	mIndexBufferView.SizeInBytes = ibByteSize;
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

void ShapShickRenderer::BuildDescriptorHeaps()
{
	// create srv heap
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(mDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));

	// fill the heap with descriptors
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvHeap->GetCPUDescriptorHandleForHeapStart());

	auto dvdTexture = mDVDTexture->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = dvdTexture->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = dvdTexture->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	mDevice->CreateShaderResourceView(dvdTexture.Get(), &srvDesc, hDescriptor);
}

void ShapShickRenderer::LoadTexture()
{
	mDVDTexture = std::make_unique<Texture>();
	mDVDTexture->Filename = L"Textures/dvd.dds";
	mDVDTexture->Name = "dvdTexture";

	ResourceUploadBatch resourceUpload(mDevice.Get());
	resourceUpload.Begin();
	
	ThrowIfFailed(CreateDDSTextureFromFile(mDevice.Get(), resourceUpload, (mDVDTexture->Filename).c_str(), mDVDTexture->Resource.ReleaseAndGetAddressOf()));

	// upload resources to the gpu
	auto uploadResourcesFinished = resourceUpload.End(mCommandQueue.Get());

	uploadResourcesFinished.wait();

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
