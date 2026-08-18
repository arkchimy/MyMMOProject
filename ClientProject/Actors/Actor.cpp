
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <algorithm>

#include "game.h"
#include "Actor.h"
#include "Render/Animation.h"
#include "utility/Common.h"
#include "Map/TileMap.h"

using namespace DirectX;

namespace actors
{
	Actor::Actor()
		:Actor(0)
	{
	}
	Actor::Actor(__int64 characterID)
		: mVertexBuffer(nullptr)
		, mConstantBuffer(nullptr)
		, mIndexCount(0)
		, mX(0.f)
		, mY(0.f)
		, mStartX(0.f)
		, mStartY(0.f)
		, mScale(1.f)
		, mSprite(nullptr)
		, mState(eActorState::Idle)
		, mDirection(eDirection::Down)
		, mSRV(nullptr)
		, mCharacterID(characterID)
		, mbMove(false)
		, mSpeed(2.f)
		, mMoveFrame(0)
	{
		ZeroMemory(mSpriteSet, sizeof(mSpriteSet));
		mIndexCount = Game::GetInstance().GetIndexCnt();
	}
	Actor::~Actor()
	{
		if (mVertexBuffer)
		{
			mVertexBuffer->Release();
			mVertexBuffer = NULL;
		}
		if (mConstantBuffer)
		{
			mConstantBuffer->Release();
			mConstantBuffer = NULL;
		}
		for (int i = 0; i < GAME_CONFIG_STATE_CNT;++i)
		{
			for (int j = 0; j < GAME_CONFIG_DIRECTION_CNT; ++j)
			{
				{
					if (mSpriteSet[i][j] != nullptr)
					{
						delete mSpriteSet[i][j];
					}
				}
			}
		}
	}
	void Actor::moveUpdate()
	{
		++mMoveFrame;
		float dx = kDirectionVectorTable[static_cast<int8_t>(mDirection)].mX;
		float dy = kDirectionVectorTable[static_cast<int8_t>(mDirection)].mY;
		mX = std::clamp(mStartX + dx * mSpeed * mMoveFrame, 0.0f, static_cast<float>(map::MAP_WIDTH * map::MAP_CONFIG_COL * map::MAP_SCALE));//map.scale
		mY = std::clamp(mStartY + dy * mSpeed * mMoveFrame, 0.0f, static_cast<float>(map::MAP_HEIGHT * map::MAP_CONFIG_ROW));

		mState = eActorState::Move;
	}
	void Actor::createVertexBuffer()
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
				RT_ASSERT(FALSE);
			};
		}
	}
	void Actor::createConstantBuffer()
	{

		CBData cbData = { mX, mY, 0, 0 };

		D3D11_BUFFER_DESC bufferDesc = {};
		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.ByteWidth = sizeof(CBData);
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		D3D11_SUBRESOURCE_DATA initData = {};
		initData.pSysMem = &cbData;

		Game::GetInstance().GetDevice().CreateBuffer(&bufferDesc, &initData, &mConstantBuffer);
		mSprite = mSpriteSet[static_cast<uint8_t>(mState)][static_cast<uint8_t>(mDirection)];
		updateConstantBuffer(0.0f, 0.0f);

		Game::GetInstance().GetDeviceContext().IASetIndexBuffer(&Game::GetInstance().GetIndexBuffer(), DXGI_FORMAT_R16_UINT, 0);
		Game::GetInstance().GetDeviceContext().IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	}
	void Actor::updateConstantBuffer(const float cameraX, const float cameraY)
	{
		CBData cbData = {};
		mSprite->GetCurrentUVOffset(cbData.uvOffset[0], cbData.uvOffset[1]);
		mSprite->GetCurrentUVScale(cbData.uvScale[0], cbData.uvScale[1]);

		int width = mSprite->GetSpriteWidth();
		int height = mSprite->GetSpriteHeight();

		float scaleX = mScale * width / 640.0f;   // 1280/2
		float scaleY = mScale * height / 360.0f;  // 720/2

		float camX = cameraX;
		float camY = cameraY;
		float transX = (mX - camX) / 640.0f;
		float transY = (camY - mY) / 360.0f;

		XMMATRIX world = XMMatrixScaling(scaleX, scaleY, 1.0f)
			* XMMatrixTranslation(transX, transY, 0.0f);

		XMMATRIX worldT = XMMatrixTranspose(world);

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		Game::GetInstance().GetDeviceContext().Map(mConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

		memcpy(cbData.matrix, &worldT, sizeof(float) * 16);
		memcpy(mapped.pData, &cbData, sizeof(CBData));

		Game::GetInstance().GetDeviceContext().Unmap(mConstantBuffer, 0);
	}

	void Actor::appendAnimationSprite(const render::eAnimationType& type, const eDirection& direction, const eActorState state
		, const char* filename, const int frameCnt, const int colMax, const int frameX, const int frameY, const int startRow, int32_t speed)
	{
		mSpriteSet[static_cast<__int8>(state)][static_cast<__int8>(direction)] = new render::Animation(type, speed);
		mSpriteSet[static_cast<__int8>(state)][static_cast<__int8>(direction)]->SetAtlas(filename, frameCnt, colMax, frameX, frameY, startRow);
		auto& sprite = mSpriteSet[static_cast<__int8>(state)][static_cast<__int8>(direction)];
		mSRV = sprite->GetCurrentSRV();
	}

	void Actor::changeAnimation(const eDirection direction, const eActorState state, const int fastForwardTicks)
	{
		mSprite->Initialize();
		mSprite = mSpriteSet[static_cast<__int8>(state)][static_cast<__int8>(direction)];
		if (fastForwardTicks > 0)
		{
			mSprite->FastForward(fastForwardTicks);
		}
		auto sampleState = mSprite->GetCurrentSampleState();
		Game::GetInstance().GetDeviceContext().PSSetSamplers(0, 1, &sampleState);
		mSRV = mSprite->GetCurrentSRV();
	}

	void Actor::Update()
	{
		mSprite->Run();
		actorUpdate();
	}

	void Actor::Render(const float cameraX, const float cameraY)
	{
		updateConstantBuffer(cameraX, cameraY);

		auto& deviceContext = Game::GetInstance().GetDeviceContext();
		RT_ASSERT(mSRV != nullptr);
		RT_ASSERT(mConstantBuffer != nullptr);

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		deviceContext.IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
		deviceContext.PSSetShaderResources(0, 1, &mSRV);
		deviceContext.VSSetConstantBuffers(0, 1, &mConstantBuffer);
		deviceContext.DrawIndexed(mIndexCount, 0, 0);
	}

	void Actor::MoveStart(const __int8 direction, const float x, const float y, const float speed)
	{
		RT_ASSERT(mbMove == false);
		mState = eActorState::Move;
		mStartX = x;
		mStartY = y;
		mDirection = static_cast<eDirection>(direction);
		mbMove = true;
		mSpeed = speed;
		mMoveFrame = 0;
		changeAnimation(mDirection, mState);
	}

	void Actor::MoveStop(const __int8 direction, const float x, const float y)
	{
		mState = eActorState::Idle;
		mStartX = x;
		mStartY = y;
		mX = x;
		mY = y;
		mDirection = static_cast<eDirection>(direction);
		mbMove = false;
		mMoveFrame = 0;
		changeAnimation(mDirection, mState);
	}

};