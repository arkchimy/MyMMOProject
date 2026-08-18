#pragma once
#include "MapConfig.h"
#include "../../_Shared/Protocol.h"
#include <cstdint>

namespace contents
{
	class FieldItem
	{
		friend class FieldServer;
	public:
		FieldItem(int64_t itemUniqueId, eItemId itemId, int32_t count, const map::Position& pos);
		void Update();               // 생존 프레임 카운트
		bool IsExpired() const;      // 30초 경과 여부
	private:
		int64_t mItemUniqueId;
		eItemId mItemId;
		int32_t mCount;
		map::Position mPos;
		map::Sector mSector;
		int32_t mLifeFrame;          // 스폰 이후 경과 틱수
	};
}
