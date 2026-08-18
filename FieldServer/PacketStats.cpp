#include "PacketStats.h"
#include "Common.h"
#include <Windows.h>

namespace stats
{
	constexpr int kTableSize = 128;

	static volatile int64_t g_sendCount[kTableSize] = {};
	static volatile int64_t g_recvCount[kTableSize] = {};

	void RecordSend(PacketType type)
	{
		int16_t idx = static_cast<int16_t>(type);
		RT_ASSERT(0 <= idx && idx < kTableSize);
		InterlockedIncrement64(&g_sendCount[idx]);
	}

	void RecordRecv(PacketType type)
	{
		int16_t idx = static_cast<int16_t>(type);
		RT_ASSERT(0 <= idx && idx < kTableSize);
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
}
