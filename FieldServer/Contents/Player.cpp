#include "Player.h"
#include "MapConfig.h"
#include "../../_Shared/AttackConfig.h"
#include "../Common.h"
#include <algorithm>
#include "Monster.h"
namespace contents
{
	Player::Player(const SOCKADDR_IN& addr, const network::SeqAndIdx& seqID)
		: mRecvQ(new utility::MyRingBuffer())
		, mAddr(addr)
		, mSeqID(seqID)
		, mAccountNo(0)
		, mCharacterID(0)
		, mCurrentFieldID(-1)
		, mNextFieldID(mCurrentFieldID)
		, mCurrentSector(0, 0)
		, mDestSector(0, 0)
		, mLastTime(0)
		, mPos(0.f, 0.f)
		, mStartMovePos(0.f, 0.f)
		, mCharacterType(0)
		, bConnect(true)
		, mSpeed(4.5f)
		, mState(ePlayerState::Idle)
		, mAnimFrame(0)
		, mDirection(eDirection::Down)
		, mSyncCnt(0)
		, mAttackPower(PLAYER_ATTACK_DAMAGE)
		, mLastAttackTime(0)
		, mPendingAttackSkillId(0)
		, mHitStunFrame(0)
		, mHp(PLAYER_MAX_HP)
		, mMaxHp(PLAYER_MAX_HP)
		, mDBRequestCnt(0)
		, mSendMsgCnt(0)
	{
		memset(mNickname, 0, sizeof(mNickname));
	}

	Player::~Player()
	{
		while (utility::Message* msg = DeQueueMsgOrNull())
		{
			// 해결 방안 : 특정 패킷을 보내자마자 연결을 끊은 상대에게 책임을 전가함.
			// 시나리오 : 1. 클라가 메세지를 보내자마자 끊음.
			//			  2. 서버는 Player의 큐를 전부 비우고 DBCount가 0일 때 위치를 db에 저장 중임.
			//			  3. 이때 아직 player는 살아있고, iocp workerThread가 playerQ에 Enq함.
			//			  4. 위치저장의 db가 완료되엇고 player를 delete함.
			
			// 실제로 이런 현상이 있는지 체크용도. 끝나면 주석 처리.
			MY_ASSERT(false, "DB카운트가 0이여서 delete되어도 연결 끊기 직전에 메세지를 보내면 큐에 남아있음.");
			MY_DELETE msg;
		}
		delete mRecvQ;
	}

	void Player::InitPlayerInfo(const int64_t accountNo, const int64_t characterId, const int8_t MoveIdx)
	{
		mAccountNo = (accountNo);
		mCharacterID = (characterId);
		mNextFieldID = (MoveIdx);
	}

	void Player::EnQueueMsg(utility::Message* msg)
	{
		RT_ASSERT(msg != nullptr);
		mRecvQ->Enqueue(&msg, sizeof(size_t));
	}
	utility::Message* Player::DeQueueMsgOrNull()
	{
		if (!bConnect)
		{
			return nullptr;
		}
		char* f = mRecvQ->GetFrontPtr();
		char* r = mRecvQ->GetRearPtr();
		if (mRecvQ->GetUseSize(f,r) < sizeof(size_t))
		{
			return nullptr;
		}
		utility::Message* retval;
		mRecvQ->Dequeue(&retval, sizeof(size_t));
		return retval;
	}

	void Player::SetPosition(float x, float y)
	{
		mPos.x = x;
		mPos.y = y;
	}

	void Player::resetAnimStart()
	{
		mAnimFrame = 0;
		mStartMovePos = mPos;
	}

	void Player::changeState(ePlayerState state)
	{
		MyAssert(state != mState, "상태 전이 실패");
		mState = state;
		resetAnimStart();
	}

	void Player::moveUpdate()
	{
		float dx = kDirectionVectorTable[static_cast<int8_t>(mDirection)].mX;
		float dy = kDirectionVectorTable[static_cast<int8_t>(mDirection)].mY;
		mPos.x = std::clamp(mStartMovePos.x + dx * mSpeed * mAnimFrame, 0.0f, static_cast<float>(SECTOR_WORLD_W * SECTOR_COL_CNT));
		mPos.y = std::clamp(mStartMovePos.y + dy * mSpeed * mAnimFrame, 0.0f, static_cast<float>(SECTOR_WORLD_H * SECTOR_ROW_CNT));
	}

	void Player::update()
	{
		if (!bConnect)
		{
			// 확인했음.
			//RT_ASSERT(false, "연결이 끊겼는데 update를 도는 경우가 있음. DB의 지연처리때문으로 예상");
			return;
		}
		++mAnimFrame;
		if (mState == ePlayerState::Dead)
		{
			return;
		}

		

		switch (mState)
		{
		case ePlayerState::Move:
			moveUpdate();
			break;
		case ePlayerState::Attack:
			if (PLAYER_ATTACK_TOTAL_FRAME <= mAnimFrame)
			{
				changeState(ePlayerState::Idle);
			}
			break;
		case ePlayerState::Stun:
			if (mHitStunFrame > 0)
			{
				--mHitStunFrame;
			}
			else
			{
				changeState(ePlayerState::Idle);
			}
			break;
		default:
			break;
		}
	}


	void Player::takeDamage(int32_t damage, const Monster* const attacker)
	{
		if (mState == ePlayerState::Dead)
		{
			return;
		}

		mDirection = getDirectionTo(mPos.x, mPos.y, attacker->GetPosition().x, attacker->GetPosition().y);
		const DirectionVector& facing = kDirectionVectorTable[static_cast<int8_t>(attacker->GetDirection())];

		mPos.x = std::clamp(mPos.x + facing.mX * PLAYER_ATTACK_KNOCKBACK_DIST, 0.0f, static_cast<float>(SECTOR_WORLD_W * SECTOR_COL_CNT));
		mPos.y = std::clamp(mPos.y + facing.mY * PLAYER_ATTACK_KNOCKBACK_DIST, 0.0f, static_cast<float>(SECTOR_WORLD_H * SECTOR_ROW_CNT));

		mHp = std::max(mHp - damage, 0);
		if (mHp == 0)
		{
			changeState(ePlayerState::Dead);
		}
		else
		{
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

};