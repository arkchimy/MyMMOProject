#include "UIBase.h"
// UIBase.cpp

#include <DirectXMath.h>
#include "utility/Common.h"
#include "../Render/Animation.h"
#include "../Game.h"

using namespace DirectX;

namespace ui
{
	UIBase::UIBase()
		:mX(0)
		, mY(0)
		, mScale(1.f)
		, mSprite(nullptr)
		, mVertexBuffer(nullptr)
		, mConstantBuffer(nullptr)
	{

	}
	void UIBase::Render()
	{
		//?곸닔踰꾪띁 ?ｊ린
		// Samapler ?명똿
		// SRV ?명똿
		// DRAW
		auto& deviceContext = Game::GetInstance().GetDeviceContext();
		RT_ASSERT(mSprite->GetCurrentSRV() != nullptr);
		RT_ASSERT(mConstantBuffer != nullptr);
		auto srv = mSprite->GetCurrentSRV();

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		deviceContext.IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);

		deviceContext.PSSetShaderResources(0, 1, &srv);
		deviceContext.VSSetConstantBuffers(0, 1, &mConstantBuffer);
		deviceContext.DrawIndexed(6, 0, 0);
	}

	void UIBase::createConstantBuffer(ID3D11Buffer** constantBuffer)
	{
		CBData cbData = { mX, mY, 0, 0 };

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.ByteWidth = sizeof(CBData);
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = &cbData;

		Game::GetInstance().GetDevice().CreateBuffer(&bufferDesc, &initData, constantBuffer);

		Game::GetInstance().GetDeviceContext().IASetIndexBuffer(&Game::GetInstance().GetIndexBuffer(), DXGI_FORMAT_R16_UINT, 0);
		Game::GetInstance().GetDeviceContext().IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		updateConstantBuffer();
	}
	void UIBase::createVertexBuffer(ID3D11Buffer** vertexBuffer)
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

			const HRESULT hr = Game::GetInstance().GetDevice().CreateBuffer(&bufferDesc, &vertexBufferData, vertexBuffer);
			if (FAILED(hr))
			{
				//std::cout << "CreateBuffer() failed. " << std::hex << hr << std::endl;
				RT_ASSERT(FALSE);
			};
		}
	}
	void UIBase::updateConstantBuffer()
	{
		CBData cbData = {};
		mSprite->GetCurrentUVOffset(cbData.uvOffset[0], cbData.uvOffset[1]);
		mSprite->GetCurrentUVScale(cbData.uvScale[0], cbData.uvScale[1]);

		int width = mSprite->GetSpriteWidth();
		int height = mSprite->GetSpriteHeight();

		float scaleX = mScale * width / 640.0f;   // 1280/2
		float scaleY = mScale * height / 360.0f;  // 720/2

		float transX = (mX ) / 640.0f;
		float transY = (- mY) / 360.0f;

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