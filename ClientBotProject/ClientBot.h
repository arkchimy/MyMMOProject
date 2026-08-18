#pragma once
#ifdef WIN32_LEAN_AND_MEAN
#else 
#define WIN32_LEAN_AND_MEAN
#endif 

#include <memory>
#include <thread>
#include <cstdint>
#include <iosfwd>
#include "Packet.h"
class RAIIwsadata;
class Session;

class ClientBot final
{
public:
	ClientBot();
	~ClientBot();

	ClientBot(const ClientBot&) = delete;
	ClientBot(const ClientBot&&) = delete;

	ClientBot& operator= (const ClientBot&) = delete;
	ClientBot& operator= (const ClientBot&&) = delete;

private:
	void selectThread(int32_t sessionCnt);
	// 소켓 생성 후 연결
	void connectSessions(std::shared_ptr<Session[]>& sessions, int32_t sessionCnt);
	void connectSession(std::shared_ptr<Session[]>& sessions, int32_t idx);
	// 소켓을 select에 등록 후 처리.
	bool selectLogic(std::shared_ptr<Session[]>& sessions, int32_t sessionCnt);
	void startInput(int32_t& userCnt);
	void monitorThread() const;

	void recvPacketProc(Session& session);						//수신 버퍼에서 완성 패킷 조립
	void packetProc(Session& session, const char* payload, int len);	//완성 패킷 type별 처리
	void handleFieldAuth(Session& session, const FieldAuthResPayload& res);
	void handleMonsterMoveStop(Session& session,const MoveStopPayload& packet);
	void handleMonsterDamaged(Session& session, const MonsterDamaged& packet);

	void handleCharecterDamaged(Session& session, const CharacterDamaged& packet);
	void handleItemSpawn(Session& session, const ItemSpawn& packet);
	//void handleLootReq();
	void handleLootRes(Session& session, const LootRes& packet);

	friend std::ostream& operator>>(std::ostream& out, const ClientBot& bot);

private:
	std::thread threads[20];
	int32_t mThreadCnt;
	std::thread mMonitorThread;
	bool mMonitorOn;
	RAIIwsadata* wsa;
	char mIPAddress[16];
	short mPort;

	volatile int64_t mAuthWaitCount;	//현재 AUTH 응답 대기 중인 세션 수
	volatile int64_t mDisconnectCount;		//CONNECTED/AUTHED 상태에서 서버가 끊은 누적 카운트
	volatile int64_t mReconnectDeadCount;	//봇 자신이 Dead 상태 감지 후 스스로 재접속한 누적 카운트
	volatile int64_t mConnectedCount;		//현재 연결 중(CONNECTED+AUTHED)인 세션 수
	volatile int64_t mTotalConnectCount;	//connect() 호출 누적 횟수
	volatile int64_t mTotalRootItemCount;	//connect() 호출 누적 횟수
};

