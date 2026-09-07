#pragma once

#include <thread>
#include "utility/MyRingBuffer.h"
#include "utility/Message.h"
#include "utility/Header.h"
#include <winsock2.h>
#include <cstdint>

namespace network
{
	class Network
	{
	public:
		Network();
		~Network();
		Network(const Network& other) = delete;
		Network(Network&& other) = delete;

		Network& operator = (const Network& rhs) = delete;
		Network& operator=(Network&& rhs) = delete;

		bool Connect(const char* ip, int32_t port);
		void Disconnect();
		bool Send(utility::Message& msg);
		void Unmarshal();
		utility::Message* PopPacket();
	private:
		void recvThread();

	private:
		SOCKET mSocket;
		std::thread mRecvThread;
		utility::MyRingBuffer* mRecvBuffer;

	private:
		utility::MyRingBuffer* mPacketQueue;
		WSADATA mWsadata;
	};

} // namespace network


extern network::Network g_Network;