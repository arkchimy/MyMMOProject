#include <iostream>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "Game.h"
#include "Actors/Actor.h"
#include "Map/TileMap.h"
#include "Scene/SceneManager.h"
#include "Scene/LoginScene.h"
#include "utility/Common.h"

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Game::Game()
	:mSceneManager(scene::SceneManager::GetInstance())

{
	constexpr int width = 1280, height = 720;

	WNDCLASSEX mWc = {
		sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L,
		GetModuleHandle(NULL), NULL, NULL, NULL, NULL,
		L"MyStoneAge", NULL };
	RegisterClassEx(&mWc);

	RECT wr = { 0, 0, width, height };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

	mHwnd = CreateWindow(
		mWc.lpszClassName, L"MyStoneAge", WS_OVERLAPPEDWINDOW,
		100, 100, wr.right - wr.left, wr.bottom - wr.top,
		NULL, NULL, mWc.hInstance, NULL);
	mWcInstance = GetModuleHandle(NULL);

	initialize(mHwnd, width, height);
}

Game::~Game()
{
	Clean();
	DestroyWindow(mHwnd);
	UnregisterClass(L"MyStoneAge", mWcInstance);
}

void Game::Start()
{
	mSceneManager->InitScene(new scene::LoginScene());
}

void Game::initShaders()
{
	{
		ID3DBlob* vertexBlob = nullptr;
		ID3DBlob* pixelBlob = nullptr;
		ID3DBlob* errorBlob = nullptr;

		if (FAILED(D3DCompileFromFile(L"VS.hlsl", 0, 0, "main", "vs_5_0", 0, 0, &vertexBlob, &errorBlob)))
		{
			if (errorBlob)
			{
				std::cout << "Vertex shader compile error\n"
					<< (char*)errorBlob->GetBufferPointer() << std::endl;
			}
		}

		if (FAILED(D3DCompileFromFile(L"PS.hlsl", 0, 0, "main", "ps_5_0", 0, 0, &pixelBlob, &errorBlob)))
		{
			if (errorBlob)
			{
				std::cout << "Pixel shader compile error\n"
					<< (char*)errorBlob->GetBufferPointer() << std::endl;
			}
		}

		mDevice->CreateVertexShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), NULL, &mVertexShader);
		mDevice->CreatePixelShader(pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(), NULL, &mPixelShader);

		// Create the input layout object
		D3D11_INPUT_ELEMENT_DESC ied[] =
		{
			{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};

		mDevice->CreateInputLayout(ied, 2, vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), &mLayout);
		mDeviceContext->IASetInputLayout(mLayout);
	}
}

void Game::initialize(HWND& window, int width, int height)
{

	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.BufferDesc.Width = width;                       // set the back buffer width
	swapChainDesc.BufferDesc.Height = height;                     // set the back buffer height
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // use 32-bit color
	swapChainDesc.BufferCount = 2;                                // one back buffer
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;  // how swap chain is to be used
	swapChainDesc.OutputWindow = window;                          // the window to be used
	swapChainDesc.SampleDesc.Count = 1;                           // how many multisamples
	swapChainDesc.Windowed = TRUE;                                // windowed/full-screen mode
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // allow full-screen switching
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	// createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;

	const D3D_FEATURE_LEVEL featureLevelArray[1] = { D3D_FEATURE_LEVEL_11_0 };
	if (FAILED(D3D11CreateDeviceAndSwapChain(
		NULL,                        // 어떤 GPU 쓸지 (NULL = 기본 GPU)
		D3D_DRIVER_TYPE_HARDWARE,    // 실제 GPU 사용 (SOFTWARE면 CPU로 돌림)
		NULL,                        // 소프트웨어 드라이버 핸들 (HARDWARE면 NULL)
		createDeviceFlags,           // 디버그 플래그 등 (현재 0)
		featureLevelArray,           // DirectX 버전 (D3D_FEATURE_LEVEL_11_0)
		1,                           // featureLevelArray 개수
		D3D11_SDK_VERSION,           // SDK 버전 (항상 이 값)
		&swapChainDesc,              // SwapChain 설정 구조체
		&mSwapChain,                 // [out] SwapChain 포인터
		&mDevice,                    // [out] Device 포인터
		NULL,                        // [out] 실제 선택된 feature level (필요없으면 NULL)
		&mDeviceContext              // [out] DeviceContext 포인터
	)))
	{
		std::cout << "D3D11CreateDeviceAndSwapChain() error" << std::endl;
	}

	// CreateRenderTarget
	ID3D11Texture2D* pBackBuffer;
	mSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	if (pBackBuffer)
	{
		mDevice->CreateRenderTargetView(pBackBuffer, NULL, &mRenderTargetView);
		pBackBuffer->Release();
	}
	else
	{
		std::cout << "CreateRenderTargetView() error" << std::endl;
		exit(-1);
	}

	// Set the viewport
	mViewport = new D3D11_VIEWPORT();
	ZeroMemory(mViewport, sizeof(D3D11_VIEWPORT));
	mViewport->TopLeftX = 0;
	mViewport->TopLeftY = 0;
	mViewport->Width = float(width);
	mViewport->Height = float(height);
	mViewport->MinDepth = 0.0f;
	mViewport->MaxDepth = 1.0f; // Note: important for depth buffering
	mDeviceContext->RSSetViewports(1, mViewport);

	initShaders();
	createIndexBuffer();
	createBlendState();
}

void Game::createIndexBuffer()
{

	// Create an index buffer
	{
		const std::vector<uint16_t> indices =
		{
			3,
			1,
			0,
			2,
			1,
			3,
		};

		mIndexCount = UINT(indices.size());

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC; // write access access by CPU and GPU
		bufferDesc.ByteWidth = UINT(sizeof(uint16_t) * indices.size());
		bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;     // use as a vertex buffer
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // allow CPU to write in buffer
		bufferDesc.StructureByteStride = sizeof(uint16_t);

		D3D11_SUBRESOURCE_DATA indexBufferData = { 0 };
		indexBufferData.pSysMem = indices.data();
		indexBufferData.SysMemPitch = 0;
		indexBufferData.SysMemSlicePitch = 0;

		mDevice->CreateBuffer(&bufferDesc, &indexBufferData, &mIndexBuffer);
	}
}

void Game::createBlendState()
{
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	mDevice->CreateBlendState(&blendDesc, &mBlendState);

}

void Game::Update()
{
	mSceneManager->Update();
}

void Game::Render()
{
	{
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		mDeviceContext->RSSetViewports(1, mViewport);
		mDeviceContext->OMSetRenderTargets(1, &mRenderTargetView, nullptr);
		mDeviceContext->ClearRenderTargetView(mRenderTargetView, clearColor);

		float blendFactor[4] = { 0, 0, 0, 0 };
		mDeviceContext->OMSetBlendState(mBlendState, blendFactor, 0xFFFFFFFF);

		mDeviceContext->VSSetShader(mVertexShader, 0, 0);
		mDeviceContext->PSSetShader(mPixelShader, 0, 0);
	}
	mSceneManager->Render();
	mSwapChain->Present(1, 0);
}

void Game::Clean()
{
	if (mLayout)
	{
		mLayout->Release();
		mLayout = NULL;
	}
	if (mVertexShader)
	{
		mVertexShader->Release();
		mVertexShader = NULL;
	}
	if (mPixelShader)
	{
		mPixelShader->Release();
		mPixelShader = NULL;
	}

	if (mIndexBuffer)
	{
		mIndexBuffer->Release();
		mIndexBuffer = NULL;
	}
	if (mBlendState)
	{
		mBlendState->Release();
		mBlendState = NULL;
	}

	if (mRenderTargetView)
	{
		mRenderTargetView->Release();
		mRenderTargetView = NULL;
	}
	if (mSwapChain)
	{
		mSwapChain->Release();
		mSwapChain = NULL;
	}
	if (mDeviceContext)
	{
		mDeviceContext->Release();
		mDeviceContext = NULL;
	}
	if (mDevice)
	{
		mDevice->Release();
		mDevice = NULL;
	}
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU)
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}