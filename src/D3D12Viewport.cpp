// Implementation DirectX 12 viewport handling

#include "D3D12Viewport.h"
#include <QWindow>
#include <stdexcept>

D3D12Viewport::D3D12Viewport(QWidget* parent) : QWidget(parent), frameIndex(0), fenceValue(0)
{
	setAttribute(Qt::WA_PaintOnScreen, true);
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	initializeD3D12();
}

D3D12Viewport::~D3D12Viewport()
{
	// Wait for GPU to finish
	if (commandQueue && fence)
	{
		commandQueue->Signal(fence.Get(), fenceValue);
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
		CloseHandle(fenceEvent);
	}
}

// Set up core D3D12 components to draw frames to a Qt widget
void D3D12Viewport::initializeD3D12()
{
	// Create device (GPU)
	if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
	{
		throw std::runtime_error("Failed to create D3D12 device");
	}

	// Create command queue
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	if (FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue))))
	{
		throw std::runtime_error("Failed to create command queue");
	}

	// Create swap chain. Create frames before replacing the displayed one to avoid flickering.
	ComPtr<IDXGIFactory4> factory;
	if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))))
	{
		throw std::runtime_error("Failed to create DXGI factory");
	}

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width();
	swapChainDesc.Height = height();
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;


	ComPtr<IDXGISwapChain1> tempSwapChain;
	HWND hwnd = reinterpret_cast<HWND>(winId()); // cast WinId() to HWND
	factory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &tempSwapChain);
	tempSwapChain.As(&swapChain);

	// Create RTV descriptor heap. Dedicated block of memory to tell GPU how to use back buffers.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = 2;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));

	// Create render targets
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (UINT i = 0; i < 2; i++)
	{
		swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
		device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += rtvDescriptorSize;
	}

	// Create command allocator and command list
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
	commandList->Close();

	// Create synchronization objects
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
}

// Renders frame to the current back buffer and presents it
void D3D12Viewport::paintEvent(QPaintEvent*)
{
	commandAllocator->Reset();
	commandList->Reset(commandAllocator.Get(), nullptr);

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = renderTargets[frameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	commandList->ResourceBarrier(1, &barrier);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += frameIndex * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	const float clearColor[] = { 0.0f, 0.2f, 0.2f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	commandList->ResourceBarrier(1, &barrier);

	commandList->Close();
	ID3D12CommandList *cmdLists[] = { commandList.Get() };
	commandQueue->ExecuteCommandLists(1, cmdLists);

	swapChain->Present(1, 0);
	frameIndex = swapChain->GetCurrentBackBufferIndex();

	commandQueue->Signal(fence.Get(), ++fenceValue);
	fence->SetEventOnCompletion(fenceValue, fenceEvent);	
	WaitForSingleObject(fenceEvent, INFINITE);
}


// Rebuild rendering pipeline when window changes size, since buffers depend on the window size
void D3D12Viewport::resizeEvent(QResizeEvent *event)
{
	if (swapChain)
	{
		for (UINT i = 0; i < 2; ++i)
		{
			renderTargets[i].Reset();
		}
		swapChain->ResizeBuffers(2, width(), height(), DXGI_FORMAT_R8G8B8A8_UNORM, 0);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		for (UINT i = 0; i < 2; i++)
		{
			swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
			device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += rtvDescriptorSize;
		}
	}
	QWidget::resizeEvent(event);
}
