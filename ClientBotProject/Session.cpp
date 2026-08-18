#include "Session.h"
#include "Common.h"
#include "Packet.h"
#include "PacketStats.h"
#include <WS2tcpip.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <timeapi.h>
#include <algorithm>
#include "../_Shared/AttackConfig.h"

//봇 전체에서 유니크한 accountNo 발급 (selectThread 여러 개가 동시에 세션을 생성)


static constexpr float kBotSpeed = 4.5f;	//서버 Player::mSpeed 기본값과 반드시 일치
Session::Session()
	:mSock(INVALID_SOCKET)
	, mSessionState(eSessionState::DISCONNECTED)
	, mAccountNo(InterlockedIncrement64(&g_accountNo))
	, mCharacterId(0)
	, mLastTime(0)
	, mPos(0.f, 0.f)
	, mStartMovePos(0.f, 0.f)
	, mSpeed(4.5f)
	, mDirection(eDirection::Down)
	, mNextDecisionTime(0)
	, mSendSize(0)
	, mRecvSize(0)
	, mConnectTime(0)
	, mAnimFrame(0)
	, mState(ePlayerState::Idle)
	, mTargetID(-1)
	, mTargetPos(0.f, 0.f)
	, mTargetItemID(-1)
	, mTargetItemPos(0.f, 0.f)
	, mHitStunFrame(0)
	, mLootRequested(false)
	, mDeadFrame(0)
	, mDisconnectTime(0)
	, mMonsterKillTime(0)
{
	ZeroMemory(mSendBuffer, SESSION_SEND_BUFFER_SIZE);
	ZeroMemory(mRecvBuffer, SESSION_RECV_BUFFER_SIZE);
}

void Session::Init()
{
	//중복 Init 방지
	RT_ASSERT(mSock == INVALID_SOCKET, "Init 중복 호출 : mSock이 이미 존재");

	mSock = socket(AF_INET, SOCK_STREAM, 0);
	int flag = 1;
	int result = setsockopt(mSock, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));
	mSendSize = 0;
	mRecvSize = 0;
	mDeadFrame = 0;
}

void Session::PostSend()
{
	int ret = send(mSock, mSendBuffer, mSendSize, 0);
	if (ret <= 0)
	{
		if (WSAGetLastError() == WSAEWOULDBLOCK)
		{
			__debugbreak();
		}
		//select 신호 직후라도 드물게 WOULDBLOCK 가능 : 다음 바퀴에 재시도
		//RT_ASSERT(WSAGetLastError() == WSAEWOULDBLOCK, "send 에러");
		return;
	}

	//보낸 만큼 앞으로 당기기 (겹치는 구간 복사라 memmove)
	RT_ASSERT(mSendSize >= ret, "역행함.");
	size_t len = static_cast<size_t>(mSendSize - ret);
	memmove(mSendBuffer, mSendBuffer + ret, len);
	mSendSize -= ret;
}

bool Session::Recv()
{
	//남은 공간이 0이면 조립 로직이 버퍼를 안 비웠다는 뜻 (설계 오류)
	//가득 찬 채 recv(len=0)를 부르면 리턴 0이라 끊김으로 오판하게 됨
	RT_ASSERT(0 < SESSION_RECV_BUFFER_SIZE - mRecvSize, "recvBuffer 가득 참");

	int ret = recv(mSock, mRecvBuffer + mRecvSize, SESSION_RECV_BUFFER_SIZE - mRecvSize, 0);
	if (ret == 0)
	{
		//RT_ASSERT(WSAGetLastError() == WSAEWOULDBLOCK, "서버가 끊음");
		return false;	//정상 종료 (상대가 FIN)
	}
	if (ret == SOCKET_ERROR)
	{
		//RT_ASSERT(WSAGetLastError() == WSAEWOULDBLOCK, "서버가 끊음");
		if (WSAGetLastError() == WSAEWOULDBLOCK)
		{
			return true;	//신호 후에도 드물게 비어있을 수 있음 : 다음 바퀴에
		}
		return false;	//WSAECONNRESET 등 강제 종료
	}

	mRecvSize += ret;
	return true;
}

void Session::EnqueueSend(const void* data, int size)
{
	mLastTime = timeGetTime();
	//안 들어가면 봇 설계 오류 : 송신량이 소켓 처리량을 초과했다는 뜻
	RT_ASSERT(mSendSize + size <= SESSION_SEND_BUFFER_SIZE, "sendBuffer 오버플로우");

	memcpy(mSendBuffer + mSendSize, data, size);
	mSendSize += size;
}

void Session::PostFieldAuthReq()
{
	FieldAuthReqPacket pkt{};
	pkt.header.Len = sizeof(pkt.type) + sizeof(pkt.id) + sizeof(pkt.pw);
	pkt.header.RandKey = 0;
	pkt.type = static_cast<int16_t>(PacketType::FIELD_AUTH_REQ);

	// mAccountNo에서 id 파생 — 재접속해도 mAccountNo가 그대로라 같은 id로 같은 계정에 로그인됨
	char idStr[20];	// pkt.id와 크기 일치 — mAccountNo 자릿수가 커져도 snprintf가 여기서 안전하게 truncate
	snprintf(idStr, sizeof(idStr), "bot%lld", static_cast<long long>(mAccountNo));
	memcpy(pkt.id, idStr, strlen(idStr));	// 나머지는 pkt{} 초기화로 이미 0

	static constexpr char kBotPw[] = "botpass123";
	memcpy(pkt.pw, kBotPw, sizeof(kBotPw) - 1);	// sizeof-1: 널 종단 문자 제외

	EnqueueSend(&pkt, sizeof(pkt));
	stats::RecordSend(PacketType::FIELD_AUTH_REQ);
}

void Session::ConnectComplete()
{
	mConnectTime = timeGetTime();
}

void Session::idleUpdate()
{
	if (mMonsterKillTime != 0)
	{
		if (mMonsterKillTime + 10000 <= timeGetTime())
		{
			mTargetID = -1;
			mMonsterKillTime = 0;
		}
	}

	if (mTargetItemID != -1)
	{
		changeState(ePlayerState::Root);
		return;
	}
	else if (mTargetID != -1)
	{
		changeState(ePlayerState::Chase);
		return;
	}

	int randNum = rand() % 100;
	//Idle : 5% 이동 시작, 95% 유지
	if (randNum <= 5)
	{

		eDirection dir = static_cast<eDirection>(rand() % static_cast<int>(eDirection::Max));
		PostMoveStart(dir);
		changeState(ePlayerState::Move);
		traceMove("idleUpdate1", true);
		return;
	}


}

void Session::moveUpdate()
{
	int randNum = rand() % 100;
	if (randNum == 0)
	{
		PostMoveStop();
		changeState(ePlayerState::Idle);
		traceMove("moveUpdate1", false);
	}
	else if (randNum == 1 && changeDirFrame <= mAnimFrame)
	{
		eDirection dir = static_cast<eDirection>(rand() % static_cast<int>(eDirection::Max));
		PostMoveStop();
		traceMove("moveUpdate2", false);
		PostMoveStart(dir);
		traceMove("moveUpdate3", true);
	}
}

void Session::chaseUpdate()
{
	// Chase 전환시는 무조건 MoveStop 상태에서.
	eDirection dirToTarget = getDirectionTo(mPos.x, mPos.y, mTargetPos.x, mTargetPos.y);

	float rangeLen = 0.f;
	int16_t maxTargetCnt = 0;
	getSkillConfig(0, rangeLen, maxTargetCnt);   // skillId 0 = 기본 공격

	if (isInAttackRange(mPos.x, mPos.y, mTargetPos.x, mTargetPos.y, rangeLen))
	{
		if (bMove)
		{
			PostMoveStop();
			traceMove("chaseUpdate1", false);

		}
		if(mMonsterKillTime == 0)
		{
			mMonsterKillTime = timeGetTime();
		}
		mDirection = dirToTarget;
		PostAttackReq();
		changeState(ePlayerState::Attack);
		return;
	}
	if (changeDirFrame <= mAnimFrame)
	{
		mAnimFrame = 0;
		if (bMove)
		{
			PostMoveStop();
			traceMove("chaseUpdate2", false);
		}
		PostMoveStart(dirToTarget);
		traceMove("chaseUpdate3", true);
	}
}

void Session::attackUpdate()
{
	if (mAnimFrame == PLAYER_ATTACK_TOTAL_FRAME)
	{
		changeState(ePlayerState::Idle);
	}
}

void Session::rootUpdate()
{
	if (mTargetItemID == -1)
	{
		if (bMove)
		{
			PostMoveStop();
			traceMove("rootUpdate3", false);
		}
		changeState(ePlayerState::Idle);
		return;
	}
	if (mLootRequested)
	{
		return;   // LOOT_RES 대기 중 — handleLootRes가 mTargetItemID 지우고 Idle로 돌려보냄
	}
	eDirection dirToTarget;
	if (LOOT_RANGE < (mPos - mTargetItemPos))
	{
		dirToTarget = getDirectionTo(mPos.x, mPos.y, mTargetItemPos.x, mTargetItemPos.y);
		if (changeDirFrame <= mAnimFrame)
		{
			if (bMove)
			{
				PostMoveStop();
				traceMove("rootUpdate1", false);
			}

			PostMoveStart(dirToTarget);
			traceMove("rootUpdate2", true);
		}
		return;
	}
	if (bMove)
	{
		PostMoveStop();   // 요청 대기 중 서버와 위치 어긋나지 않도록 정지 통보
		traceMove("rootUpdate3", false);
	}
	PostLootReq();

	mLootRequested = true;
}

void Session::deadUpdate()
{
	++mDeadFrame;
}

void Session::stunUpdate()
{
	if (mAnimFrame == PLAYER_HIT_STUN_FRAME)
	{
		changeState(ePlayerState::Idle);
	}
}

void Session::resetAnimStart()
{
	mAnimFrame = 0;
	mStartMovePos = mPos;
}

void Session::changeState(const ePlayerState& mode)
{
	//std::cout << "AccountNo : " << mAccountNo << "\t" << "before :" << std::setw(changeDirFrame) << debugStr[static_cast<int8_t>(mState)] << "\tafter : " << std::setw(10) << debugStr[static_cast<int8_t>(mode)]<<"\n";
	if (mode == mState)
	{
		__debugbreak();
	}
	mState = mode;
	resetAnimStart();
}

void Session::selectAction()
{
	DWORD currentTime = timeGetTime();
	// 마지막 송신이 20초 넘게 경과되었다면.
	if (mLastTime + 20000 <= currentTime)
	{
		PostHeartBeat();
	}

}

void Session::Update()
{

	if (bMove)
	{
		float dx = kDirectionVectorTable[static_cast<int8_t>(mDirection)].mX;
		float dy = kDirectionVectorTable[static_cast<int8_t>(mDirection)].mY;
		mPos.x = std::clamp(mStartMovePos.x + dx * mSpeed * mAnimFrame, 0.0f, static_cast<float>(SECTOR_WORLD_W * SECTOR_COL_CNT));
		mPos.y = std::clamp(mStartMovePos.y + dy * mSpeed * mAnimFrame, 0.0f, static_cast<float>(SECTOR_WORLD_H * SECTOR_ROW_CNT));
	}

	switch (mState)
	{
	case ePlayerState::Idle:
		idleUpdate();
		break;
	case ePlayerState::Move:
		moveUpdate();
		break;
	case ePlayerState::Chase:
		chaseUpdate();
		break;
	case ePlayerState::Attack:
		attackUpdate();
		break;
	case ePlayerState::Stun:
		stunUpdate();
		break;
	case ePlayerState::Root:
		rootUpdate();
		break;
	case ePlayerState::Dead:
		deadUpdate();
		break;
	}
	++mAnimFrame;
	selectAction();
}

void Session::ChaseTarget(const int64_t targetID, const map::Position& targetPos)
{
	if (mTargetID == targetID)
	{
		mTargetPos = targetPos;
	}
	else if (mTargetID == -1 )
	{
		mTargetID = targetID;
		mTargetPos = targetPos;
	}

}

void Session::ChaseItem(const int64_t targetID, const map::Position& targetPos)
{

	mTargetItemID = targetID;
	mTargetItemPos = targetPos;
	mLootRequested = false;
}

void Session::TakeDamage(const int32_t hp, const float x, const float y)
{
	if (mState == ePlayerState::Dead)
	{
		return;
	}

	mMonsterKillTime = timeGetTime();
	mPos.x = x;
	mPos.y = y;
	mStartMovePos = mPos;

	mTargetPos.x = x;
	mTargetPos.y = y;

	if (bMove)
	{
		traceMove("TakeDamage", false);
	}

	if (hp <= 0)
	{
		changeState(ePlayerState::Dead);
		return;
	}
	else
	{
		mAnimFrame = 0;
		mHitStunFrame = PLAYER_HIT_STUN_FRAME;
		if (mState == ePlayerState::Stun)
		{
			resetAnimStart();   // 이미 스턴 중 재피격 — 상태 전이 없이 애니메이션만 재시작
		}
		else
		{
			changeState(ePlayerState::Stun);
		}
	}
}

void Session::MonsterChaseInit()
{
	RT_ASSERT(mTargetID != -1, "쫒는 상대가없는데 init이 옴");
	mTargetID = -1;
}

void Session::ItemChaseInit()
{
	RT_ASSERT(mTargetItemID != -1, "쫒는 상대가없는데 init이 옴");
	mTargetItemID = -1;
}

void Session::Disconnect(int32_t currentTime)
{
	closesocket(mSock);
	mSock = INVALID_SOCKET;
	mSessionState = eSessionState::DISCONNECTED;
	mDisconnectTime = currentTime;
}




void Session::PostMoveStart(eDirection dir)
{
	MoveStartPacket pkt{};
	pkt.header.Len = sizeof(pkt.type) + sizeof(pkt.characterId) + sizeof(pkt.x) + sizeof(pkt.y) + sizeof(pkt.direction) + sizeof(pkt.speed);
	pkt.header.RandKey = 0;
	pkt.type = static_cast<int16_t>(PacketType::MOVE_START);
	pkt.characterId = mCharacterId;
	pkt.x = mPos.x;
	pkt.y = mPos.y;
	pkt.direction = static_cast<int8_t>(dir);
	pkt.speed = kBotSpeed;

	EnqueueSend(&pkt, sizeof(pkt));
	stats::RecordSend(PacketType::MOVE_START);

	mStartMovePos = mPos;
	mDirection = dir;
	mAnimFrame = 0;
}

void Session::PostMoveStop()
{
	MoveStopPacket pkt{};
	pkt.header.Len = sizeof(pkt.type) + sizeof(pkt.characterId) + sizeof(pkt.x) + sizeof(pkt.y) + sizeof(pkt.direction);
	pkt.header.RandKey = 0;
	pkt.type = static_cast<int16_t>(PacketType::MOVE_STOP);
	pkt.characterId = mCharacterId;
	pkt.x = mPos.x;
	pkt.y = mPos.y;
	pkt.direction = static_cast<int8_t>(mDirection);

	EnqueueSend(&pkt, sizeof(pkt));
	stats::RecordSend(PacketType::MOVE_STOP);

	mStartMovePos = mPos;
	mAnimFrame = 0;
}

void Session::PostAttackReq()
{
	PlayerAttackReq pkt{};
	pkt.header.Len = sizeof(pkt.type) + sizeof(pkt.skillId) + sizeof(pkt.targetCnt) + sizeof(pkt.monsterId);
	pkt.header.RandKey = 0;
	pkt.type = static_cast<int16_t>(PacketType::PLAYER_ATTACK_REQ);
	pkt.skillId = 0;
	pkt.targetCnt = 1;
	pkt.monsterId = mTargetID;

	EnqueueSend(&pkt, sizeof(pkt));
	stats::RecordSend(PacketType::PLAYER_ATTACK_REQ);
}

void Session::PostLootReq()
{
	LootReq pkt{};
	pkt.header.Len = sizeof(pkt.type) + sizeof(pkt.itemUniqueId);
	pkt.header.RandKey = 0;
	pkt.type = static_cast<int16_t>(PacketType::LOOT_REQ);
	pkt.itemUniqueId = mTargetItemID;

	EnqueueSend(&pkt, sizeof(pkt));
	stats::RecordSend(PacketType::LOOT_REQ);
}

void Session::PostHeartBeat()
{
	//TODO : 하트비트 프로토콜 구현하기.

}

void Session::traceMove(const char* str, int8_t on)
{
	/*switch (on)
	{
	case 0:
		std::cout <<"AccountNo : " << mAccountNo << "\t" << str << "\t : Off" << "\n";
		break;
	case 1:
		std::cout << "AccountNo : " << mAccountNo << "\t" << str << "\t : On" << "\n";
	}*/

	bMove = on;
}


