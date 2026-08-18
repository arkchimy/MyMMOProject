#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN             // 거의 사용되지 않는 내용을 Windows 헤더에서 제외합니다.
#endif // DEBUG

#ifndef NOMINMAX
#define NOMINMAX                        // Windows.h의 min/max 매크로가 std::min/std::max를 오염시키는 것 방지
#endif // !1



#include <shared_mutex>
#include <stack>
#include <thread>
#include <memory>

#include "Session.h"
#include "utility/Message.h"


namespace network
{
	constexpr short RELEASE_IOCOUNT = static_cast<short>(1 << 14);
	class NetworkLib
	{
	public:
		NetworkLib();
		virtual ~NetworkLib() {}

	protected:
		void start();
		void stop();
		void disconnectAllSession();

		virtual void onAccept(SOCKADDR_IN& addr, const SeqAndIdx& sessionID) = 0;
		virtual void onRecv(utility::Message* msg) = 0;
		virtual void onSend(utility::Message* msg) = 0;
		virtual void onRelease(const SeqAndIdx& sessionID) = 0;

		Session* const FindSessionOrNull(const SeqAndIdx& seqId);
		void sendPost(const SeqAndIdx& sessionID, utility::Message& msg);
	public:
		int64_t GetAcceptCount() const { return mAcceptCnt; }
		int64_t GetSendCount() const { return mSendCnt; }
		int64_t GetRecvCount() const { return mRecvCnt; }
		int64_t GetDisConnectCount() const { return mDisConnect; }
		int64_t GetSessionCount() const { return mSessionCnt; }
	protected:
		// 실제론 연결되지않은 Socket AcceptEx를 반환하되 disConnect카운팅 안하고싶을떄 ture넣으면 카운팅 안 됨.
		void disconnectSession(const SeqAndIdx& sessionID, bool isNetworklib = false);

	private:
		void workerThread();

		void registerAcceptEx();
		void completeAcceptEx(Session& session);

		void registerRecv(Session& session);
		void completeRecv(Session& session, DWORD transferred);

		void registerSend(Session& session);
		void completeSend(Session& session);

		void completeRelease(Session& session);

		void checkAndHandleIoError(Session& session, const int lastError);

		bool stackSessionIdx_Pop(ull& out);
		void stackSessionIdx_Push(const ull& input);

		bool sessionLock(const SeqAndIdx& sessionID);
		void sessionUnLock(const SeqAndIdx& sessionID);

	private:
		std::thread mWorkerThreads[CONFIG_WORKER_THREAD_CNT];
		SOCKET mListenSock;
		uint64_t mbOn;
		HANDLE mHcp; // iocpHandle
		std::unique_ptr<Session> mSessions[CONFIG_SESSION_MAX];

		std::stack<seqAddrType> mStackSessionIdx;
		std::shared_mutex mStackMutex;

		seqAddrType mSeqID = -1;

		int64_t mAcceptCnt;
		int64_t mSendCnt;
		int64_t mRecvCnt;
		int64_t mDisConnect;
		int64_t mSessionCnt;         // 실제 accept 성공한 세션 수 (GetSessionCount()가 리턴하는 값)
		int64_t mActiveSlotCnt; // pending accept 포함, 정리 안 끝난 슬롯 수 (stop()의 종료 대기 전용)
	};

} // namespace network
