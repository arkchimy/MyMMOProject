#pragma once
#ifdef WIN32_LEAN_AND_MEAN
#else
	#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <WS2tcpip.h>
#include <cstdint>
#include "../_Shared/Protocol.h"
#include "Map.h"

constexpr const int64_t changeDirFrame = 25;
constexpr const char* debugStr[] = {
	"Idle",
	"Move",
	"Chase",
	"Attack",
	"Stun",
	"Root",
	"Dead",
};

class ClientBot;

enum class ePlayerState : int8_t
{
	Idle,
	Move,
	Chase, // 근처에 MonsterMoveStop 대상을 쫒는 상태
	Attack, // 해당 Monster를 공격함.
	Stun,   // Player가 Hit상태
	Root, // 근처의 Item을 찾으며 줍기 시도.
	Dead,	// Player의 사망.
};

constexpr int SESSION_SEND_BUFFER_SIZE = 1024;
constexpr int SESSION_RECV_BUFFER_SIZE = 40960;	//SPAWN_BATCH 최대치(서버 Message MaxSize 3000)보다 크게


enum class eSessionState
{
	DISCONNECTED,	// 미사용 or 끊김 (어느 셋에도 등록 안 함)
	REQ_CONNECT,		// connect 걸어놓고 결과 대기 중 (writeSet + exceptSet 감시)
	CONNECTED,		// 연결 완료, 인증 응답 대기 (readSet 감시)
	AUTHED,			// FIELD_AUTH_RES 수신, 필드 입장 완료 (readSet 감시)
};

class Session
{
	friend class ClientBot;
public:
	Session();

	void Init();	//소켓 생성
	void EnqueueSend(const void* data, int size);	//송신 버퍼 뒤에 이어붙임 (실제 send는 select 루프 담당)
	void PostSend();	//writeSet 신호 시 호출 : 보내고 남은 만큼 앞으로 당김
	bool Recv();		//readSet 신호 시 호출 : 수신 버퍼에 쌓음. false = 연결 끊김
	
	void PostFieldAuthReq();	//connect 성공 직후 1회 : 입장 인증 요청 송신
	void ConnectComplete();
	
	int64_t GetAccountNo() const { return mAccountNo; }
	int32_t GetDeadFrame() const { return mDeadFrame; }
	void Update();
	void ChaseTarget(const int64_t targetID, const map::Position& targetPos);
	void ChaseItem(const int64_t targetID, const map::Position& targetPos);
	void TakeDamage(const int32_t hp, const float x, const float y);

	void MonsterChaseInit();
	void ItemChaseInit();
	void Disconnect(int32_t currentTime);

private:
	void idleUpdate();
	void moveUpdate();
	void chaseUpdate();
	void attackUpdate();
	void stunUpdate();
	void rootUpdate();
	void deadUpdate();
	void resetAnimStart();
private:
	void changeState(const ePlayerState& mode);
	void selectAction();

	void PostMoveStart(eDirection dir);
	void PostMoveStop();
	void PostAttackReq();
	void PostLootReq();

	void PostHeartBeat();
	void traceMove(const char* str, int8_t on);
private:
	SOCKET mSock;
	eSessionState mSessionState;
	int64_t mAccountNo;	//생성 시 1회 발급, 재접속해도 유지 (Init에서 초기화하지 않음)

	//AUTH_RES로 받는 입장 정보 (이후 MOVE 패킷에 사용)
	int64_t mCharacterId;
	map::Position mPos;
	map::Position mStartMovePos;
	float mSpeed;

	//이동 판정용 상태 (mX, mY는 마지막으로 서버에 보고한 위치 = 데드레커닝 앵커로 계속 갱신됨)
	eDirection mDirection;

	DWORD mNextDecisionTime;	//다음 판정 시각 (timeGetTime() 기준)
	DWORD mConnectTime;
	int64_t mAnimFrame;

	char mSendBuffer[SESSION_SEND_BUFFER_SIZE];
	int mSendSize;	//전송 대기 중인 바이트 수
	char mRecvBuffer[SESSION_RECV_BUFFER_SIZE];
	int mRecvSize;	//조립 대기 중인 바이트 수

	DWORD mLastTime;
	ePlayerState mState;

	int64_t mTargetID;
	map::Position mTargetPos;

	int64_t mTargetItemID;
	map::Position mTargetItemPos;

	int32_t mHitStunFrame;

	bool mLootRequested;	// LOOT_REQ 보내고 LOOT_RES 대기 중인지
	int8_t bMove = false;
	int32_t mDeadFrame;
	int32_t mDisconnectTime;
	uint32_t mMonsterKillTime;

	// 위치저장 검증용: Disconnect()는 mPos를 안 건드리므로, 재접속해서
	// FIELD_AUTH_RES를 덮어쓰기 직전까지 mPos엔 disconnect 시점 위치가 그대로 남아있음.
	// 이 플래그로 "첫 접속(비교 무의미)"과 "재접속(비교 대상 있음)"만 구분.
	bool mHasDisconnectPos = false;
public:
	inline static LONG64 g_accountNo = 0;
};


namespace map
{
	enum MapConfig
	{
		MAP_CONFIG_ROW = 100,
		MAP_CONFIG_COL = 50,
		MAP_CONFIG_TILESET = 6,
		MAP_WIDTH = 64,
		MAP_HEIGHT = 47,
		MAP_SCALE = 2,
	};
}