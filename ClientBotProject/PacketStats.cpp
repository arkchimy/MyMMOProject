#include "PacketStats.h"
#include "Common.h"
#include <Windows.h>

namespace stats
{
	constexpr int kTableSize = static_cast<int16_t>(PacketType::MAX);

	static volatile int64_t g_sendCount[kTableSize] = {};
	static volatile int64_t g_recvCount[kTableSize] = {};

	void RecordSend(PacketType type)
	{
		int16_t idx = static_cast<int16_t>(type);
		RT_ASSERT(0 <= idx && idx < kTableSize, "PacketStats : 범위 밖 PacketType");
		InterlockedIncrement64(&g_sendCount[idx]);
	}

	void RecordRecv(PacketType type)
	{
		int16_t idx = static_cast<int16_t>(type);
		RT_ASSERT(0 <= idx && idx < kTableSize, "PacketStats : 범위 밖 PacketType");
		InterlockedIncrement64(&g_recvCount[idx]);
	}

	int64_t GetSendCount(PacketType type)
	{
		return g_sendCount[static_cast<int16_t>(type)];
	}

	int64_t GetRecvCount(PacketType type)
	{
		return g_recvCount[static_cast<int16_t>(type)];
	}

	int64_t GetTotalSendCount()
	{
		int64_t total = 0;
		for (int i = 0; i < kTableSize; ++i)
		{
			total += g_sendCount[i];
		}
		return total;
	}

	int64_t GetTotalRecvCount()
	{
		int64_t total = 0;
		for (int i = 0; i < kTableSize; ++i)
		{
			total += g_recvCount[i];
		}
		return total;
	}
}
