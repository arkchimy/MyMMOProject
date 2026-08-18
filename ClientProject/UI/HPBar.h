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
	class HPBar : public UIBase
	{
	public:
		HPBar();
		~HPBar();

		void SetTarget(actors::Player* target);   // 실제 HP를 읽어올 대상(로컬 플레이어)
		virtual void Update();
		virtual void Render();

		void updateConstantBuffer2();
	private:
		actors::Player* mTarget;
		float mCurrentHp;
		float mMaxHp;
		float rate;

		render::Animation* mHpBar;
		ID3D11Buffer* mVertexBuffer2;
		ID3D11Buffer* mConstantBuffer2;

		render::ImageManager* mUIManager;
	};
}

