#include "NetConfig.h"
#include "Network.h"

network::Network g_Network;

namespace network
{
	Network::Network()
		: m_socket(INVALID_SOCKET)
		, m_recvBuffer(nullptr)
		, m_packetQueue(nullptr)
		, wsadata{ 0 }
	{
		RT_ASSERT(WSAStartup(MAKEWORD(2, 2), &wsadata) == 0);

		m_recvBuffer = new utility::MyRingBuffer();
		m_packetQueue = new utility::MyRingBuffer();
	}

	Network::~Network()
	{

		Disconnect();
		delete m_recvBuffer;
		delete m_packetQueue;
		WSACleanup();
	}

	bool Network::Connect(const char* ip, int port)
	{
		if (m_socket != INVALID_SOCKET)
		{
			// 이미 연결됨
			return false;
		}

		m_socket = socket(AF_INET, SOCK_STREAM, 0);
		if (m_socket == INVALID_SOCKET)
		{
			return false;
		}

		SOCKADDR_IN addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		inet_pton(AF_INET, ip, &addr.sin_addr);

		if (connect(m_socket, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
		{
			closesocket(m_socket);
			m_socket = INVALID_SOCKET;
			return false;
		}

		m_recvThread = std::thread(&Network::recvThread, this);
		SetThreadDescription(m_recvThread.native_handle(), L"RecvThread");
		return true;
	}
	void Network::Disconnect()
	{
		if (m_socket == INVALID_SOCKET)
		{
			return;
		}

		closesocket(m_socket);
		m_socket = INVALID_SOCKET;

		if (m_recvThread.joinable())
		{
			m_recvThread.join();
		}
		m_recvBuffer->ClearBuffer();
		utility::Message* msg;
		while ((msg = PopPacket()) != nullptr)
		{
			delete msg;
		}
		m_packetQueue->ClearBuffer();

	}
	bool Network::Send(utility::Message& msg) 
	{
		if (m_socket == INVALID_SOCKET)
		{
			return false;
		}

		int sendSize = static_cast<int>(msg.GetUseSize());
		int result = send(m_socket, msg.GetFrontPtr(), sendSize, 0);

		if (result == SOCKET_ERROR)
		{
			Disconnect();
			return false;
		}

		return true;
	}
	void Network::recvThread()
	{
		while (true)
		{
			char* f = m_recvBuffer->GetFrontPtr();
			char* r = m_recvBuffer->GetRearPtr();
			char* writePtr = r;
			int32_t directFreeSize = m_recvBuffer->GetDirectFreeSize(f,r);
			if (directFreeSize == 0)
			{
				// 수신버퍼가 가득찼다.
				__debugbreak();
				break;
			}
			int result = recv(m_socket, writePtr, directFreeSize, 0);
			if (result <= 0)
			{
				break;
			}

			m_recvBuffer->MoveRear(result);
			Unmarshal();
		}
	}
	void network::Network::Unmarshal()
	{

		while (true)
		{
			char* f = m_recvBuffer->GetFrontPtr();
			char* r = m_recvBuffer->GetRearPtr();

			char* pf = m_packetQueue->GetFrontPtr();
			char* pr = m_packetQueue->GetRearPtr();

			int32_t useSize = m_recvBuffer->GetUseSize(f, r);
			if (useSize < (int)sizeof(Header))
			{
				break;
			}

			Header header;
			m_recvBuffer->Peek(&header, sizeof(Header));

			int totalSize = sizeof(Header) + header.Len;
			if (useSize < totalSize)
			{
				break;
			}

			if (m_packetQueue->GetFreeSize(pf,pr) < sizeof(size_t))
			{
				break; // 패킷큐 꽉 참 → 다음 recv 때 재시도
			}

			m_recvBuffer->MoveFront(sizeof(Header));

			char tempBuf[utility::eBufferSize::BufferSize];
			m_recvBuffer->Dequeue(tempBuf, header.Len);

			utility::Message* msg = new utility::Message();
			msg->PutData(tempBuf, header.Len);

			m_packetQueue->Enqueue(&msg, sizeof(utility::Message*));
		}
	}
	utility::Message* network::Network::PopPacket()
	{
		char* pf = m_packetQueue->GetFrontPtr();
		char* pr = m_packetQueue->GetRearPtr();
		if (m_packetQueue->GetUseSize(pf,pr) < (int)sizeof(utility::Message*))
		{
			return nullptr;
		}

		utility::Message* msg = nullptr;
		m_packetQueue->Dequeue(&msg, sizeof(utility::Message*));

		return msg;
	}

};