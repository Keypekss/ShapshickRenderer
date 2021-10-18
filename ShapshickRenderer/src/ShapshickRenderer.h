#pragma once
#include "DXApp.h"

using Microsoft::WRL::ComPtr;

struct SceneConstantBuffer {
	DirectX::XMFLOAT4 position;
	float padding[60]; // constant buffer size must be multiple of 256byte
};

struct Texture
{
	// Unique material name for lookup.
	std::string Name;

	std::wstring Filename;

	ComPtr<ID3D12Resource> Resource = nullptr;
	ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

class ShapShickRenderer : public DXApp
{
public:
	ShapShickRenderer(HINSTANCE hInstance);
	~ShapShickRenderer();

	virtual bool Initialize() override;

private:
	virtual void OnResize()override;
	virtual void Update(const Timer& gt) override;
	virtual void Draw(const Timer& gt) override;

	void BuildPSO();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildBuffers();
	void BuildConstantBuffer();
	void BuildDescriptorHeaps();
	void LoadTexture();
	
	// constant buffer
	ComPtr<ID3D12DescriptorHeap> mCbvHeap;
	ComPtr<ID3D12Resource> mConstantBuffer;
	UINT8* mCbvDataBegin = nullptr;
	SceneConstantBuffer mConstantBufferData{};

	// dx necessary state
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	ComPtr<ID3DBlob> mVertexShaderByteCode = nullptr;
	ComPtr<ID3DBlob> mPixelShaderByteCode = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignature;
	ComPtr<ID3D12PipelineState> mPipelineStateObject;

	// vertex and index buffers
	ComPtr<ID3DBlob> mVertexBufferCPU = nullptr;
	ComPtr<ID3DBlob> mIndexBufferCPU = nullptr;
	ComPtr<ID3D12Resource> mVertexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> mIndexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> mVertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> mIndexBufferUploader = nullptr;
	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

	// texture related
	std::unique_ptr<Texture> mDVDTexture;
	ComPtr<ID3D12DescriptorHeap> mSrvHeap; 
};