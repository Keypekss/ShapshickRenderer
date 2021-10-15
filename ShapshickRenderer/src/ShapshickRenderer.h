#pragma once
#include "DXApp.h"

using Microsoft::WRL::ComPtr;

struct SceneConstantBuffer {
	DirectX::XMFLOAT4 offset;
	float padding[60]; // constant buffer size must be multiple of 256byte
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

	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildTriangle();
	void BuildConstantBuffer();
	void BuildPSO();

	ComPtr<ID3D12DescriptorHeap> mCbvHeap;
	ComPtr<ID3D12Resource> mConstantBuffer;
	UINT8* mCbvDataBegin = nullptr;
	SceneConstantBuffer mConstantBufferData{};

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	ComPtr<ID3DBlob> mVertexShaderByteCode = nullptr;
	ComPtr<ID3DBlob> mPixelShaderByteCode = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignature;
	ComPtr<ID3D12PipelineState> mPipelineStateObject;

	// buffers
	ComPtr<ID3DBlob> mVertexBufferCPU = nullptr;
	ComPtr<ID3DBlob> mIndexBufferCPU = nullptr;
	ComPtr<ID3D12Resource> mVertexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> mIndexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> mVertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> mIndexBufferUploader = nullptr;

	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;
};