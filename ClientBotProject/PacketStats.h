#pragma once
#include <cstdint>
#include "../_Shared/Protocol.h"

//패킷 타입별 송수신 카운트를 프로세스 전역으로 집계 (Session/ClientBot 양쪽에서 호출)
namespace stats
{
	void RecordSend(PacketType type);
	void RecordRecv(PacketType type);
	int64_t GetSendCount(PacketType type);
	int64_t GetRecvCount(PacketType type);
	int64_t GetTotalSendCount();
	int64_t GetTotalRecvCount();
}
