#pragma once
#include "../_Shared/Protocol.h"
#include <cstdint>
#include <stdlib.h>

namespace contents
{
	struct DropEntry
	{
		eItemId itemId;
		int32_t count;
		int32_t dropRate;   // 가중치 (다른 항목들과의 상대값, %일 필요 없음)
	};

	// 몬스터 타입이 지금은 1종류뿐이라 공용 테이블 하나로 시작.
	// 나중에 몬스터 타입별로 나뉘면 monsterType → 테이블 매핑으로 확장.
	inline const DropEntry kDropTable[] =
	{
		{ eItemId::Gold,   1, 50 },
		{ eItemId::Gold,   5, 20 },
		{ eItemId::Gold,  10, 10 },
		{ eItemId::Potion, 1, 20 },
	};
	inline bool rollDrop(eItemId& outItemId, int32_t& outCount)
	{
		int32_t randVal = rand();
		while (32700 <= randVal)
		{
			randVal = rand(); // 32700 ~ 32767 이 나오면 다시 돌리기.
		}
		int32_t roll = randVal % 100;
		int32_t acc = 0;
		for (const DropEntry& entry : kDropTable)
		{
			acc += entry.dropRate;
			if (roll < acc)
			{
				outItemId = entry.itemId;
				outCount = entry.count;
				return true;
			}
		}
		return false;
	}
}
