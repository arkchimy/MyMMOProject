#pragma once
#include <cstdint>
#include "../_Shared/Protocol.h"

//패킷 타입별 송수신 카운트 집계 (클라이언트향 패킷만 대상)
namespace stats
{
	void RecordSend(PacketType type);
	void RecordRecv(PacketType type);
	int64_t GetSendCount(PacketType type);
	int64_t GetRecvCount(PacketType type);
}
