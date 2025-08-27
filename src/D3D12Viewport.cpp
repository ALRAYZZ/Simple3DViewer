// Implementation DirectX 12 viewport handling

#include "D3D12Viewport.h"
#include <QWindow>
#include <stdexcept>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QWheelEvent>
#include <QDebug>

using namespace DirectX;

D3D12Viewport::D3D12Viewport(QWidget* parent) : QWidget(parent), frameIndex(0), fenceValue(0)
{
	setAttribute(Qt::WA_PaintOnScreen, true);
	setAttribute(Qt::WA_NativeWindow, true);
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setAttribute(Qt::WA_NoSystemBackground, true);
	setFocusPolicy(Qt::StrongFocus);

	leftButtonPressed = false;
	isWireframe = false;

	winId();

	indexCount = 0;
	XMStoreFloat4x4(&mvpMatrix, XMMatrixIdentity());

	try
	{
		initializeD3D12();
	}
	catch (const std::exception& ex)
	{
		QMessageBox::critical(nullptr, "D3D12 Initialization Error", QString("Failed to initialize DirectX 12: %1").arg(ex.what()));
		throw;
	}
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
	if (vertexBuffer) vertexBuffer.Reset();
	if (indexBuffer) indexBuffer.Reset();
	if (constantBuffer) constantBuffer.Reset();
	if (rootSignature) rootSignature.Reset();
	if (pipelineState) pipelineState.Reset();
}



// Helper function to get shader path
std::wstring GetShaderPath(const std::wstring& shaderName)
{
	// Try current directory first
	QFileInfo localFile(QString::fromStdWString(shaderName));
	if (localFile.exists())
	{
		return shaderName;
	}

	// Try resources subdirectory
	QFileInfo resourceFile(QString("resource/") + QString::fromStdWString(shaderName));
	if (resourceFile.exists())
	{
		return L"resource/" + shaderName;
	}

	// Try absolute path fro mexecutable directory
	QDir exeDir(QCoreApplication::applicationDirPath());
	QFileInfo exeDirFile(exeDir.filePath(QString::fromStdWString(shaderName)));
	if (exeDirFile.exists())
	{
		return exeDirFile.absoluteFilePath().toStdWString();
	}

	// Try resources in executable directory
	QFileInfo exeResourcesFile(exeDir.filePath("resources/" + QString::fromStdWString(shaderName)));
	if (exeResourcesFile.exists())
	{
		return exeResourcesFile.absoluteFilePath().toStdWString();
	}
	return shaderName; // Fallback to original name
}
// Set up core D3D12 components to draw frames to a Qt widget
void D3D12Viewport::initializeD3D12()
{
	std::wstring vertexShaderPath = GetShaderPath(L"vertex.hlsl");
	std::wstring pixelShaderPath = GetShaderPath(L"pixel.hlsl");

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
	if (FAILED(factory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &tempSwapChain)))
	{
		throw std::runtime_error("Failed to create swap chain");
	}
	tempSwapChain.As(&swapChain);

	// Create RTV descriptor heap. Dedicated block of memory to tell GPU how to use back buffers.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = 2;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	if (FAILED(device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap))))
	{
		throw std::runtime_error("Failed to create RTV descriptor heap");
	}

	// Create render targets
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	for (UINT i = 0; i < 2; i++)
	{
		if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]))))
		{
			throw std::runtime_error("Failed to get swap chain buffer");
		}
		device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += rtvDescriptorSize;
	}

	// Create depth stencil descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	if (FAILED(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap))))
	{
		throw std::runtime_error("Failed to create depth stencil descriptor heap");
	}

	// Create depth stencil buffer
	D3D12_RESOURCE_DESC depthDesc = {};
	depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthDesc.Alignment = 0;
	depthDesc.Width = width();
	depthDesc.Height = height();
	depthDesc.DepthOrArraySize = 1;
	depthDesc.MipLevels = 1;
	depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.SampleDesc.Quality = 0;
	depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES deapthHeapProps = {};
	deapthHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	deapthHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	deapthHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	deapthHeapProps.CreationNodeMask = 1;
	deapthHeapProps.VisibleNodeMask = 1;

	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.DepthStencil.Stencil = 0;

	if (FAILED(device->CreateCommittedResource(
		&deapthHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&depthDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS(&depthBuffer)
	)))
	{
		throw std::runtime_error("Failed to create depth buffer");
	}

	// Create depth stencil view
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	device->CreateDepthStencilView(depthBuffer.Get(), &dsvDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());


	// Create command allocator and command list
	if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator))))
	{
		throw std::runtime_error("Failed to create command allocator");
	}
	if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList))))
	{
		throw std::runtime_error("Failed to create command list");
	}
	commandList->Close();

	// Create synchronization objects
	fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence))))
	{
		throw std::runtime_error("Failed to create fence");
	}

	// Create constant buffer for MVP matrix - properly initialize the resource desc
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC cbDesc = {};
	cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	cbDesc.Alignment = 0;

	struct ConstantBufferData
	{
		XMFLOAT4X4 mvpMatrix;
		XMFLOAT4X4 modelMatrix;
		XMFLOAT4X4 normalMatrix;
		XMFLOAT4X4 padding;
	};
	static_assert((sizeof(ConstantBufferData) % 256) == 0, "Constant buffer size must be 256-byte aligned");
	cbDesc.Width = sizeof(ConstantBufferData);

	cbDesc.Height = 1;
	cbDesc.DepthOrArraySize = 1;
	cbDesc.MipLevels = 1;
	cbDesc.Format = DXGI_FORMAT_UNKNOWN;
	cbDesc.SampleDesc.Count = 1;
	cbDesc.SampleDesc.Quality = 0;
	cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	cbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	if (FAILED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constantBuffer))))
	{
		throw std::runtime_error("Failed to create constant buffer");
	}

	// Map and initialize constant buffer
	void* cbData;
	D3D12_RANGE range = { 0, 0 };
	if (FAILED(constantBuffer->Map(0, &range, &cbData)))
	{
		throw std::runtime_error("Failed to map constant buffer");
	}
	XMStoreFloat4x4(&mvpMatrix, XMMatrixIdentity());
	memcpy(cbData, &mvpMatrix, sizeof(XMFLOAT4X4));
	constantBuffer->Unmap(0, nullptr);

	// Manual root parameter initialization (could be loaded from a file instead)
	D3D12_ROOT_PARAMETER rootParameters[1];
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].Descriptor.RegisterSpace = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.NumParameters = 1;
	rsDesc.pParameters = rootParameters;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> rsBlob, errorBlob;
	if (FAILED(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &errorBlob)))
	{
		throw std::runtime_error("Failed to serialize root signature");
	}
	if (FAILED(device->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature))))
	{
		throw std::runtime_error("Failed to create root signature");
	}

	ComPtr<ID3DBlob> vsBlob, psBlob;
	
	// Try to compile vertex shader with better error reporting
	HRESULT vsResult = D3DCompileFromFile(vertexShaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vsBlob, &errorBlob);
	if (FAILED(vsResult))
	{
		std::string errorMsg = "Failed to compile vertex shader";
		if (errorBlob)
		{
			errorMsg += ":\n";
			errorMsg += static_cast<char*>(errorBlob->GetBufferPointer());
		}
		errorMsg += "\nHRESULT: 0x" + std::to_string(vsResult);
		throw std::runtime_error(errorMsg);
	}

	HRESULT psResult = D3DCompileFromFile(pixelShaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &psBlob, &errorBlob);
	if (FAILED(psResult))
	{
		std::string errorMsg = "Failed to compile pixel shader";
		if (errorBlob)
		{
			errorMsg += ":\n";
			errorMsg += static_cast<char*>(errorBlob->GetBufferPointer());
		}
		errorMsg += "\nHRESULT: 0x" + std::to_string(psResult);
		throw std::runtime_error(errorMsg);
	}

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// Manual blend state initialization
	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = FALSE;
	blendDesc.RenderTarget[0].BlendEnable = FALSE;
	blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// Pipeline state creation
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputLayout, 2 };
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.RasterizerState = {
		D3D12_FILL_MODE_WIREFRAME,
		D3D12_CULL_MODE_NONE,
		FALSE, // FrontCounterClockwise
		D3D12_DEFAULT_DEPTH_BIAS,
		D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
		D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
		TRUE, // DepthClipEnable
		FALSE, // MultisampleEnable
		FALSE, // AntialiasedLineEnable
		0, // ForcedSampleCount
		D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	};
	psoDesc.BlendState = blendDesc;
	// Enable depth testing
	psoDesc.DepthStencilState.DepthEnable = TRUE;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	psoDesc.DepthStencilState.StencilEnable = FALSE;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;
	if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState))))
	{
		throw std::runtime_error("Failed to create graphics pipeline state");
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDescSolid = psoDesc;
	psoDescSolid.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // Change to solid fill mode
	psoDescSolid.RasterizerState.CullMode = D3D12_CULL_MODE_BACK; // Enable back-face culling
	if (FAILED(device->CreateGraphicsPipelineState(&psoDescSolid, IID_PPV_ARGS(&pipelineStateSolid))))
	{
		throw std::runtime_error("Failed to create solid graphics pipeline state");
	}
}
// Load model data into GPU buffers
void D3D12Viewport::loadModel(const Model* model)
{
	if (!model)
	{
		qCritical() << "Null model passed to loadModel";
		return;
	}

	try
	{
		qDebug() << "Starting model load...";

		const auto& positions = model->getVertices();
		const auto& indices = model->getIndices();
		const auto& normals = model->getNormals();

		if (positions.empty() || indices.empty())
		{
			qCritical() << "Model has no vertices or indices";
			indexCount = 0;
			return;
		}

		if (positions.size() > UINT_MAX || indices.size() > UINT_MAX)
		{
			qCritical() << "Model too large for 32-bit buffers";
			return;
		}

		// Combined vertex data (position + normal)
		std::vector<Vertex> vertices;
		vertices.reserve(positions.size());

		for (size_t i = 0; i < positions.size(); ++i)
		{
			Vertex vertex;
			vertex.position = positions[i];
			vertex.normal = (i < normals.size()) ? normals[i] : QVector3D(0.0f, 1.0f, 0.0f);
			vertices.push_back(vertex);
		}


		qDebug() << "Model stats - Vertices:" << vertices.size() << "Indices:" << indices.size();
		indexCount = static_cast<UINT>(indices.size());

		// Create vertex buffer
		D3D12_HEAP_PROPERTIES heapProps = {};
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC vbDesc = {};
		vbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		vbDesc.Alignment = 0;
		vbDesc.Width = vertices.size() * sizeof(Vertex);
		vbDesc.Height = 1;
		vbDesc.DepthOrArraySize = 1;
		vbDesc.MipLevels = 1;
		vbDesc.Format = DXGI_FORMAT_UNKNOWN;
		vbDesc.SampleDesc.Count = 1;
		vbDesc.SampleDesc.Quality = 0;
		vbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		vbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		if (FAILED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer))))
		{
			throw std::runtime_error("Failed to create vertex buffer");
		}
		void* vbData;
		vertexBuffer->Map(0, nullptr, &vbData);
		memcpy(vbData, vertices.data(), vertices.size() * sizeof(Vertex));
		vertexBuffer->Unmap(0, nullptr);
		vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
		vertexBufferView.SizeInBytes = static_cast<UINT>(vertices.size() * sizeof(Vertex));
		vertexBufferView.StrideInBytes = sizeof(Vertex);

		// Create index buffer
		D3D12_RESOURCE_DESC ibDesc = {};
		ibDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		ibDesc.Alignment = 0;
		ibDesc.Width = indices.size() * sizeof(unsigned int);
		ibDesc.Height = 1;
		ibDesc.DepthOrArraySize = 1;
		ibDesc.MipLevels = 1;
		ibDesc.Format = DXGI_FORMAT_UNKNOWN;
		ibDesc.SampleDesc.Count = 1;
		ibDesc.SampleDesc.Quality = 0;
		ibDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		ibDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		if (FAILED(device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &ibDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indexBuffer))))
		{
			throw std::runtime_error("Failed to create index buffer");
		}
		void* ibData;
		indexBuffer->Map(0, nullptr, &ibData);
		memcpy(ibData, indices.data(), indices.size() * sizeof(unsigned int));
		indexBuffer->Unmap(0, nullptr);
		indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
		indexBufferView.SizeInBytes = static_cast<UINT>(indices.size() * sizeof(unsigned int));
		indexBufferView.Format = DXGI_FORMAT_R32_UINT;

		update();
	}
	catch (const std::exception& ex)
	{
		qCritical() << "Exception in loadModel:" << ex.what();
		throw;
	}
	catch (...)
	{
		qCritical() << "Unknown exception in loadModel";
		throw;
	}
}
// Renders frame to the current back buffer and presents it
void D3D12Viewport::paintEvent(QPaintEvent*)
{
	try
	{
		qDebug() << "Rendering frame, indexCount:" << indexCount;

		// Get matrices separately
		auto mvpMatrix = camera.getMVPMatrix((float)width() / (float)height());
		auto modelMatrix = XMMatrixIdentity(); 
		auto normalMatrix = XMMatrixInverse(nullptr, XMMatrixTranspose(modelMatrix));

		// Update constant buffer with all matrices
		struct ConstantBufferData
		{
			XMFLOAT4X4 mvpMatrix;
			XMFLOAT4X4 modelMatrix;
			XMFLOAT4X4 normalMatrix;
		} cbData;

		XMStoreFloat4x4(&cbData.mvpMatrix, XMLoadFloat4x4(&mvpMatrix));
		XMStoreFloat4x4(&cbData.modelMatrix, modelMatrix);
		XMStoreFloat4x4(&cbData.normalMatrix, normalMatrix);

		void* mappedData;
		D3D12_RANGE range = { 0, 0 };
		constantBuffer->Map(0, &range, &mappedData);
		// FIX: Copy cbData instead of mvpMatrix
		memcpy(mappedData, &cbData, sizeof(ConstantBufferData));
		constantBuffer->Unmap(0, nullptr);

		commandAllocator->Reset();
		commandList->Reset(commandAllocator.Get(), isWireframe ? pipelineState.Get() : pipelineStateSolid.Get());

		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = renderTargets[frameIndex].Get();
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
		commandList->ResourceBarrier(1, &barrier);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandle.ptr += frameIndex * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();

		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

		const float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

		// Set viewport
		D3D12_VIEWPORT viewport = {};
		viewport.TopLeftX = 0;
		viewport.TopLeftY = 0;
		viewport.Width = static_cast<float>(width());
		viewport.Height = static_cast<float>(height());
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		commandList->RSSetViewports(1, &viewport);

		// Set scissor rect
		D3D12_RECT scissorRect = { 0, 0, width(), height() };
		commandList->RSSetScissorRects(1, &scissorRect);

		if (indexCount > 0)
		{
			commandList->SetGraphicsRootSignature(rootSignature.Get());
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			commandList->IASetIndexBuffer(&indexBufferView);
			commandList->SetGraphicsRootConstantBufferView(0, constantBuffer->GetGPUVirtualAddress());
			commandList->DrawIndexedInstanced(indexCount, 1, 0, 0, 0);
		}

		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		commandList->ResourceBarrier(1, &barrier);

		commandList->Close();
		ID3D12CommandList* cmdLists[] = { commandList.Get() };
		commandQueue->ExecuteCommandLists(1, cmdLists);

		swapChain->Present(1, 0);
		frameIndex = swapChain->GetCurrentBackBufferIndex();

		commandQueue->Signal(fence.Get(), ++fenceValue);
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);

		qDebug() << "Frame rendered successfully.";
	}
	catch (const std::exception& ex)
	{
		qCritical() << "Exception in paintEvent:" << ex.what();
	}
	catch (...)
	{
		qCritical() << "Unknown exception in paintEvent";
	}
}
// Rebuild rendering pipeline when window changes size, since buffers depend on the window size
void D3D12Viewport::resizeEvent(QResizeEvent *event)
{
	if (swapChain)
	{
		// Wait for GPU to finish
		commandQueue->Signal(fence.Get(), ++fenceValue);
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);

		// Release old resources before resizing
		for (UINT i = 0; i < 2; ++i)
		{
			renderTargets[i].Reset();
		}
		depthBuffer.Reset();
		swapChain->ResizeBuffers(2, width(), height(), DXGI_FORMAT_R8G8B8A8_UNORM, 0);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
		UINT rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		for (UINT i = 0; i < 2; i++)
		{
			swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i]));
			device->CreateRenderTargetView(renderTargets[i].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += rtvDescriptorSize;
		}
		// Create depth stencil descriptor heap
		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		if (FAILED(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap))))
		{
			throw std::runtime_error("Failed to create depth stencil descriptor heap");
		}

		// Create depth stencil buffer
		D3D12_RESOURCE_DESC depthDesc = {};
		depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		depthDesc.Alignment = 0;
		depthDesc.Width = width();
		depthDesc.Height = height();
		depthDesc.DepthOrArraySize = 1;
		depthDesc.MipLevels = 1;
		depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.SampleDesc.Quality = 0;
		depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_HEAP_PROPERTIES depthHeapProps = {};
		depthHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		depthHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		depthHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		depthHeapProps.CreationNodeMask = 1;
		depthHeapProps.VisibleNodeMask = 1;

		D3D12_CLEAR_VALUE depthClearValue = {};
		depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
		depthClearValue.DepthStencil.Depth = 1.0f;
		depthClearValue.DepthStencil.Stencil = 0;

		if (FAILED(device->CreateCommittedResource(
			&depthHeapProps,
			D3D12_HEAP_FLAG_NONE,
			&depthDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
			&depthClearValue,
			IID_PPV_ARGS(&depthBuffer))))
		{
			throw std::runtime_error("Failed to create depth buffer");
		}

		// Create depth stencil view
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
		device->CreateDepthStencilView(depthBuffer.Get(), &dsvDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());

	}
	QWidget::resizeEvent(event);
}


// Mouse event handlers
void D3D12Viewport::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
	{
		leftButtonPressed = true;
		lastMousePos = event->pos();
	}
}
void D3D12Viewport::mouseMoveEvent(QMouseEvent* event)
{
	if (leftButtonPressed)
	{
		float dx = event->pos().x() - lastMousePos.x();
		float dy = event->pos().y() - lastMousePos.y();
		camera.orbit(dx, dy);
		lastMousePos = event->pos();
		update();
	}
}
void D3D12Viewport::wheelEvent(QWheelEvent* event)
{
	camera.zoom(event->angleDelta().y() / 120.0f); // Scroll sensitivity
	update();
}

// Toggle between wireframe and solid rendering modes
void D3D12Viewport::toggleWireframe()
{
	isWireframe = !isWireframe;
	update();
}
