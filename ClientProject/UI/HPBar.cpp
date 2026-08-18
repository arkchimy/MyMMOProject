//// HPBar.cpp
#include "HPBar.h"
#include "../Render/ImageManager.h"
#include "../Render/Image.h"
#include "../Render/Animation.h"
#include "../utility/Common.h"
#include "../Actors/Player.h"


#include <DirectXMath.h>
#include "../Game.h"

using namespace DirectX;

static const char* fileName[] =
{
	"Asset/ui/HP/Borders/Border_Black.png",
	"Asset/ui/HP/Style_3.png",
};

namespace ui
{
	HPBar::HPBar()
		: mTarget(nullptr)
		, mCurrentHp(100.f)
		, mMaxHp(100.f)
		, rate(1.f)
		, mUIManager(nullptr)
	{
		mUIManager = render::ImageManager::GetInstance();
		const render::Image* border = nullptr;
		border = mUIManager->Load(fileName[0]);

		mSprite = new render::Animation(render::eAnimationType::EndStop);
		mSprite->SetAtlas(fileName[0], 1, 1, border->GetWidth(), border->GetHeight(), 0);

		mX = -640.f + border->GetWidth() / 2.f;
		mY = -360.f + border->GetHeight() / 2.f;

		mUIManager->Release(fileName[0]);
		createConstantBuffer(&mConstantBuffer);
		createVertexBuffer(&mVertexBuffer);

		//hpBar
		{
			const render::Image* border = nullptr;
			border = mUIManager->Load(fileName[1]);

			mHpBar = new render::Animation(render::eAnimationType::EndStop);
			mHpBar->SetAtlas(fileName[1], 1, 1, border->GetWidth(), border->GetHeight(), 0);

			mUIManager->Release(fileName[1]);
			createConstantBuffer(&mConstantBuffer2);
			createVertexBuffer(&mVertexBuffer2);
			updateConstantBuffer2();
		}
	}
	HPBar::~HPBar()
	{
		delete mSprite;
		delete mHpBar;
	}
	void HPBar::SetTarget(actors::Player* target)
	{
		mTarget = target;
	}
	void HPBar::Update()
	{
		if (mTarget != nullptr)
		{
			mCurrentHp = static_cast<float>(mTarget->GetHp());
			mMaxHp = static_cast<float>(mTarget->GetMaxHp());
		}

		if (mMaxHp <= 0.f)
		{
			mSprite->Run();
			return;
		}
		rate = mCurrentHp / mMaxHp;

		mSprite->Run();
		mHpBar->Run();
	}
	void HPBar::Render()
	{
		UIBase::Render();
		updateConstantBuffer2();
		
		auto& deviceContext = Game::GetInstance().GetDeviceContext();
		RT_ASSERT(mHpBar->GetCurrentSRV() != nullptr);
		RT_ASSERT(mConstantBuffer2 != nullptr);
		auto srv = mHpBar->GetCurrentSRV();

		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		deviceContext.IASetVertexBuffers(0, 1, &mVertexBuffer2, &stride, &offset);

		deviceContext.PSSetShaderResources(0, 1, &srv);
		deviceContext.VSSetConstantBuffers(0, 1, &mConstantBuffer2);
		deviceContext.DrawIndexed(6, 0, 0);

	}


	void HPBar::updateConstantBuffer2()
	{
		CBData cbData = {};
		mHpBar->GetCurrentUVOffset(cbData.uvOffset[0], cbData.uvOffset[1]);
		mHpBar->GetCurrentUVScale(cbData.uvScale[0], cbData.uvScale[1]);

		int width = mHpBar->GetSpriteWidth();
		int height = mHpBar->GetSpriteHeight();

		float scaleX = rate * mScale * width / 640.0f;   // 1280/2
		float scaleY = mScale * height / 360.0f;  // 720/2

		float transX = (mX - (1 - rate) * width * 0.5f) / 640.0f;
		float transY = (-mY) / 360.0f;

		XMMATRIX world = XMMatrixScaling(scaleX, scaleY, 1.0f)
			* XMMatrixTranslation(transX, transY, 0.0f);

		XMMATRIX worldT = XMMatrixTranspose(world);

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		Game::GetInstance().GetDeviceContext().Map(mConstantBuffer2, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

		memcpy(cbData.matrix, &worldT, sizeof(float) * 16);
		memcpy(mapped.pData, &cbData, sizeof(CBData));

		Game::GetInstance().GetDeviceContext().Unmap(mConstantBuffer2, 0);
	}
};