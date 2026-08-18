#include "FieldItem.h"

namespace
{
	constexpr int32_t ITEM_DESPAWN_FRAME = 1500;   // 20ms 틱 * 1500 = 30초
}

namespace contents
{
	FieldItem::FieldItem(int64_t itemUniqueId, eItemId itemId, int32_t count, const map::Position& pos)
		: mItemUniqueId(itemUniqueId)
		, mItemId(itemId)
		, mCount(count)
		, mPos(pos)
		, mSector{ 0, 0 }
		, mLifeFrame(0)
	{
	}

	void FieldItem::Update()
	{
		++mLifeFrame;
	}

	bool FieldItem::IsExpired() const
	{
		return mLifeFrame >= ITEM_DESPAWN_FRAME;
	}
}
