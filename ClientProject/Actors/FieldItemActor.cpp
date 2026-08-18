#include "FieldItemActor.h"
#include "Render/Animation.h"
#include <cmath>

namespace
{
	struct ItemSpriteInfo
	{
		const char* filename;
		int width;
		int height;
	};

	// eItemId 순서(Gold, Potion)와 동일하게 맞춤
	const ItemSpriteInfo kItemSpriteTable[] =
	{
		{ "Asset/DropItem/Gold.png",     24, 20 },
		{ "Asset/DropItem/Potion_1.png", 24, 31 },
	};

	constexpr float BOB_AMPLITUDE = 3.f;
	constexpr float BOB_SPEED = 0.05f;
}

namespace actors
{
	FieldItemActor::FieldItemActor(int64_t itemUniqueId, float x, float y, eItemId itemId)
		: Actor(itemUniqueId)
		, mBaseY(y)
		, mBobTick(0)
	{
		mX = x;
		mY = y;
		mScale = 1.f;
		loadItemSprite(itemId);
		createVertexBuffer();
		createConstantBuffer();
		changeAnimation(mDirection, mState);
	}

	void FieldItemActor::actorUpdate()
	{
		++mBobTick;
		mY = mBaseY + sinf(mBobTick * BOB_SPEED) * BOB_AMPLITUDE;
	}

	void FieldItemActor::loadItemSprite(eItemId itemId)
	{
		const ItemSpriteInfo& info = kItemSpriteTable[static_cast<size_t>(itemId)];
		appendAnimationSprite(render::eAnimationType::Loop, eDirection::Down, eActorState::Idle,
			info.filename, 1, 1, info.width, info.height, 0);
	}
}
