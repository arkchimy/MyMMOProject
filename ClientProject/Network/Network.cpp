#include "NetConfig.h"
#include "Network.h"

network::Network g_Network;

namespace network
{
	Network::Network()
		: mSocket(INVALID_SOCKET)
		, mRecvBuffer(nullptr)
		, mPacketQueue(nullptr)
		, mWsadata{ 0 }
	{
		RT_ASSERT(WSAStartup(MAKEWORD(2, 2), &mWsadata) == 0);

		mRecvBuffer = new utility::MyRingBuffer();
		mPacketQueue = new utility::MyRingBuffer();
	}

	Network::~Network()
	{

		Disconnect();
		delete mRecvBuffer;
		delete mPacketQueue;
		WSACleanup();
	}

	bool Network::Connect(const char* ip, int32_t port)
	{
		if (mSocket != INVALID_SOCKET)
		{
			// 이미 연결됨
			return false;
		}

		mSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (mSocket == INVALID_SOCKET)
		{
			return false;
		}

		SOCKADDR_IN addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		inet_pton(AF_INET, ip, &addr.sin_addr);

		if (connect(mSocket, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
		{
			closesocket(mSocket);
			mSocket = INVALID_SOCKET;
			return false;
		}

		mRecvThread = std::thread(&Network::recvThread, this);
		SetThreadDescription(mRecvThread.native_handle(), L"RecvThread");
		return true;
	}
	void Network::Disconnect()
	{
		if (mSocket == INVALID_SOCKET)
		{
			return;
		}

		closesocket(mSocket);
		mSocket = INVALID_SOCKET;

		if (mRecvThread.joinable())
		{
			mRecvThread.join();
		}
		mRecvBuffer->ClearBuffer();
		utility::Message* msg;
		while ((msg = PopPacket()) != nullptr)
		{
			delete msg;
		}
		mPacketQueue->ClearBuffer();

	}
	bool Network::Send(utility::Message& msg) 
	{
		if (mSocket == INVALID_SOCKET)
		{
			return false;
		}

		int32_t sendSize = static_cast<int32_t>(msg.GetUseSize());
		int32_t result = send(mSocket, msg.GetFrontPtr(), sendSize, 0);

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
			char* f = mRecvBuffer->GetFrontPtr();
			char* r = mRecvBuffer->GetRearPtr();
			char* writePtr = r;
			int32_t directFreeSize = mRecvBuffer->GetDirectFreeSize(f,r);
			if (directFreeSize == 0)
			{
				// 수신버퍼가 가득찼다.
				__debugbreak();
				break;
			}
			int32_t result = recv(mSocket, writePtr, directFreeSize, 0);
			if (result <= 0)
			{
				break;
			}

			mRecvBuffer->MoveRear(result);
			Unmarshal();
		}
	}
	void network::Network::Unmarshal()
	{

		while (true)
		{
			char* f = mRecvBuffer->GetFrontPtr();
			char* r = mRecvBuffer->GetRearPtr();

			char* pf = mPacketQueue->GetFrontPtr();
			char* pr = mPacketQueue->GetRearPtr();

			int32_t useSize = mRecvBuffer->GetUseSize(f, r);
			if (useSize < (int32_t)sizeof(Header))
			{
				break;
			}

			Header header;
			mRecvBuffer->Peek(&header, sizeof(Header));

			int32_t totalSize = sizeof(Header) + header.Len;
			if (useSize < totalSize)
			{
				break;
			}

			if (mPacketQueue->GetFreeSize(pf,pr) < sizeof(size_t))
			{
				break; // 패킷큐 꽉 참 → 다음 recv 때 재시도
			}

			mRecvBuffer->MoveFront(sizeof(Header));

			char tempBuf[utility::eBufferSize::BufferSize];
			mRecvBuffer->Dequeue(tempBuf, header.Len);

			utility::Message* msg = new utility::Message();
			msg->PutData(tempBuf, header.Len);

			mPacketQueue->Enqueue(&msg, sizeof(utility::Message*));
		}
	}
	utility::Message* network::Network::PopPacket()
	{
		char* pf = mPacketQueue->GetFrontPtr();
		char* pr = mPacketQueue->GetRearPtr();
		if (mPacketQueue->GetUseSize(pf,pr) < (int32_t)sizeof(utility::Message*))
		{
			return nullptr;
		}

		utility::Message* msg = nullptr;
		mPacketQueue->Dequeue(&msg, sizeof(utility::Message*));

		return msg;
	}

};