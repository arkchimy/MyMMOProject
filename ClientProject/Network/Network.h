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
		Network(const Network&& other) = delete;

		Network& operator = (const Network& rhs) = delete;
		Network& operator = (const Network&& rhs) = delete;

		bool Connect(const char* ip, int port);
		void Disconnect();
		bool Send(utility::Message& msg);
		void Unmarshal();
		utility::Message* PopPacket();
	private:
		void recvThread();

	private:
		SOCKET m_socket;
		std::thread m_recvThread;
		utility::MyRingBuffer* m_recvBuffer;

	private:
		utility::MyRingBuffer* m_packetQueue;
		WSADATA wsadata;
	};

} // namespace network


extern network::Network g_Network;