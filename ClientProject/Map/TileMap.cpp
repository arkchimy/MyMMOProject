//TileMap.cpp
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include "TileMap.h"
#include "../Game.h"
#include "utility/Common.h"

using namespace DirectX;

namespace map
{
	TileMap::TileMap()
		:mScale(2.f)
	{
		for (int i = 0; i < MAP_CONFIG_ROW; ++i)
		{
			for (int j = 0; j < MAP_CONFIG_COL;++j)
			{
				mTile[i][j] = rand() % MAP_CONFIG_TILESET;
			}
		}
		mTileImg[0].ReadFromFile("Asset/tiles/00.png");
		mTileImg[1].ReadFromFile("Asset/tiles/01.png");
		mTileImg[2].ReadFromFile("Asset/tiles/02.png");
		mTileImg[3].ReadFromFile("Asset/tiles/03.png");
		mTileImg[4].ReadFromFile("Asset/tiles/04.png");
		mTileImg[5].ReadFromFile("Asset/tiles/05.png");

		createVertexBuffer();
		createConstantBuffer();
	}

	TileMap::~TileMap()
	{
		if (mVertexBuffer != nullptr)
		{
			mVertexBuffer->Release();
		}
		if (mConstantBuffer != nullptr)
		{
			mConstantBuffer->Release();
		}
	}

	void TileMap::Render(const float cameraX, const float cameraY)
	{
		auto& instance = Game::GetInstance();
		auto& deviceContext = instance.GetDeviceContext();

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		deviceContext.IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);

		for (int i = 0; i < MAP_CONFIG_ROW; ++i)
		{
			for (int j = 0; j < MAP_CONFIG_COL;++j)
			{
				auto srv = mTileImg[mTile[i][j]].GetSRV();
				RT_ASSERT(mConstantBuffer != nullptr);
				updateConstantBuffer(i, j, cameraX, cameraY);

				deviceContext.PSSetShaderResources(0, 1, &srv);
				deviceContext.VSSetConstantBuffers(0, 1, &mConstantBuffer);
				auto sampleState = mTileImg[mTile[i][j]].GetSampleState();
				deviceContext.PSSetSamplers(0, 1, &sampleState);
				deviceContext.DrawIndexed(instance.GetIndexCnt(), 0, 0);
			}
		}
	}

	void TileMap::createVertexBuffer()
	{
		// Create a vertex buffer
		{
			const std::vector<Vertex> vertices =
			{
				{{-0.5f, -0.5f, 0.0f, 1.0f} ,{ 0.f, 1.f } },
				{{0.5f, -0.5f, 0.0f, 1.0f} ,{ 1.f, 1.f } },
				{{0.5f, 0.5f, 0.0f, 1.0f}, { 1.f, 0.f } },
				{{-0.5f, 0.5f, 0.0f, 1.0f}, { 0.f, 0.f }}
			};

			D3D11_BUFFER_DESC bufferDesc;
			ZeroMemory(&bufferDesc, sizeof(bufferDesc));
			bufferDesc.Usage = D3D11_USAGE_DYNAMIC;                        // write access access by CPU and GPU
			bufferDesc.ByteWidth = UINT(sizeof(Vertex) * vertices.size()); // size is the VERTEX struct * 3
			bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;               // use as a vertex buffer
			bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;            // allow CPU to write in buffer
			bufferDesc.StructureByteStride = sizeof(Vertex);

			D3D11_SUBRESOURCE_DATA vertexBufferData = {
				0,
			};
			vertexBufferData.pSysMem = vertices.data();
			vertexBufferData.SysMemPitch = 0;
			vertexBufferData.SysMemSlicePitch = 0;

			const HRESULT hr = Game::GetInstance().GetDevice().CreateBuffer(&bufferDesc, &vertexBufferData, &mVertexBuffer);
			if (FAILED(hr))
			{
				//std::cout << "CreateBuffer() failed. " << std::hex << hr << std::endl;
				RT_ASSERT(FALSE);
			};
		}
	}
	void TileMap::createConstantBuffer()
	{
		CBData cbData = { 0, 0, 0, 0 };

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.ByteWidth = sizeof(CBData);
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = &cbData;

		Game::GetInstance().GetDevice().CreateBuffer(&bufferDesc, &initData, &mConstantBuffer);
	}
	void TileMap::updateConstantBuffer(int row, int col, const float cameraX, const float cameraY)
	{
		CBData cbData = {};
		cbData.uvOffset[0] = 0.f;
		cbData.uvOffset[1] = 0.f;
		cbData.uvScale[0] = 1.f;
		cbData.uvScale[1] = 1.f;

		float width = (int)MAP_WIDTH * mScale;
		float height = (int)MAP_HEIGHT * mScale;

		float scaleX = width / 640.0f;   // 1280/2
		float scaleY = height / 360.0f;  // 720/2

		float camX = cameraX;
		float camY = cameraY;
		float transX;
		float transY;

		if (row % 2 == 0)
		{
			transX = ((float)col * width + width * 0.5f - camX ) / 640.0f;

		}
		else
		{
			transX = ((float)col * width - camX) / 640.0f;
		}
		transY = (camY - (row * height * 0.5f + height * 0.5f)) / 360.0f;

		XMMATRIX world = XMMatrixScaling(scaleX, scaleY, 1.0f)
			* XMMatrixTranslation(transX, transY, 0.0f);

		XMMATRIX worldT = XMMatrixTranspose(world);

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		Game::GetInstance().GetDeviceContext().Map(mConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

		memcpy(cbData.matrix, &worldT, sizeof(float) * 16);
		memcpy(mapped.pData, &cbData, sizeof(CBData));

		Game::GetInstance().GetDeviceContext().Unmap(mConstantBuffer, 0);
	}
};