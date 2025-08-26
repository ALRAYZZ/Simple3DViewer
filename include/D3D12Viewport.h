// Manages D3D12 initialization and rendering within a Qt widget.

#pragma once

#include <QWidget>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <QVector3D>
#include <vector>
#include "Model.h"
#include <QMatrix4x4>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;

class D3D12Viewport : public QWidget
{
	Q_OBJECT

public:
	D3D12Viewport(QWidget* parent = nullptr);
	~D3D12Viewport();

	// Override paintEngine to return nullptr for Direct3D rendering
	QPaintEngine* paintEngine() const override { return nullptr; }
	void loadModel(const Model* model);

protected:
	void initializeD3D12();
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;

private:
	// D3D12 core components that need to be managed to render within the widget
	ComPtr<ID3D12Device> device; // Graphics brain, represents the GPU
	ComPtr<IDXGISwapChain3> swapChain; // Manages the buffers for rendering and presenting to the screen
	ComPtr<ID3D12CommandQueue> commandQueue; // Sends commands to the GPU
	ComPtr<ID3D12CommandAllocator> commandAllocator; // Allocates memory for command lists
	ComPtr<ID3D12GraphicsCommandList> commandList; // Issues drawing commands
	ComPtr<ID3D12DescriptorHeap> rtvHeap; // Manages render target views (interprets the bynary data from renderTargets)
	ComPtr<ID3D12Resource> renderTargets[2]; // The actual buffers we render to (hold the bynary data)

	// Synchronization objects
	HANDLE fenceEvent; // Event handle for GPU synchronization
	ComPtr<ID3D12Fence> fence; // Synchronization primitive
	UINT64 fenceValue; // Current fence value for synchronization
	UINT frameIndex; // Current frame index in the swap chain

	ComPtr<ID3D12RootSignature> rootSignature; // Defines how shaders access resources
	ComPtr<ID3D12PipelineState> pipelineState; // Encapsulates the GPU state for rendering
	ComPtr<ID3D12Resource> vertexBuffer; // Buffer holding vertex data
	ComPtr<ID3D12Resource> indexBuffer; // Buffer holding index data
	ComPtr<ID3D12Resource> constantBuffer; // Buffer for passing constants to shaders
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView; // View describing the vertex buffer
	D3D12_INDEX_BUFFER_VIEW indexBufferView; // View describing the index buffer
	unsigned int indexCount; // Number of indices to draw
	DirectX::XMFLOAT4X4 mvpMatrix;
};