#pragma once
#include "UIBase.h"
// HPBar.h
namespace render
{
	class Image;
	class ImageManager;
	class Animation;
}
namespace actors
{
	class Player;
}
namespace ui
{
	class HPBar final : public UIBase
	{
	public:
		HPBar();
		~HPBar();

		HPBar(const HPBar&) = delete;
		HPBar& operator=(const HPBar&) = delete;
		HPBar(HPBar&&) = delete;
		HPBar& operator=(HPBar&&) = delete;

		void SetTarget(actors::Player* target);   // 실제 HP를 읽어올 대상(로컬 플레이어)
		virtual void Update() override;
		virtual void Render() override;

	private:
		void updateConstantBuffer2();
	private:
		actors::Player* mTarget;
		float mCurrentHp;
		float mMaxHp;
		float mRate;

		render::Animation* mHpBar = nullptr;
		ID3D11Buffer* mVertexBuffer2 = nullptr;
		ID3D11Buffer* mConstantBuffer2 = nullptr;

		render::ImageManager* mUIManager;
	};
}

