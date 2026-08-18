#pragma once
#include "../_lib/AcceptEx_IOCP_NetworkLib/AcceptEx_IOCP_NetworkLib.h"
#include "../../_Shared/Protocol.h"
#include "MapConfig.h"
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace contents
{
	enum class ePlayerState : int8_t
	{
		Idle,
		Move,
		Attack,
		Dead,
		Stun,
	};

	class Player
	{
		friend class FieldServer;
	public:
		Player(const SOCKADDR_IN& addr, const network::SeqAndIdx& seqID);
		~Player();
		Player(const Player& other) = delete;
		Player(Player&& other) = delete;

		Player& operator = (const Player& rhs) = delete;
		Player& operator = (Player&& rhs) = delete;
	public:
		void InitPlayerInfo(const int64_t accountNo, const int64_t characterId, const int8_t fieldID);
		void EnQueueMsg(utility::Message* msg);
		utility::Message* DeQueueMsgOrNull();
		void SetPosition(float x, float y);
		int32_t GetAnimFrame() const { return mAnimFrame; }
		bool IsDead() const { return mState == ePlayerState::Dead; }

		void takeDamage(int32_t damage, const class Monster* const attacker);
		ePlayerState GetSpawnState() const { return mState; }
		eDirection GetDirection() const { return mDirection; }
		map::Position GetPosition() const { return mPos; }
	private:
		void update();
		void changeState(ePlayerState state);
		void resetAnimStart();
		void moveUpdate();
	private:
		utility::MyRingBuffer* mRecvQ;
		const SOCKADDR_IN        mAddr;

		int64_t mAccountNo;
		int64_t mCharacterID;
		map::Position mPos;
		map::Position mStartMovePos;
		const network::SeqAndIdx mSeqID;

		float mSpeed;
		DWORD mLastTime;
		__int32 mAnimFrame;   // mMoveFrame → 범용화 (이동/공격은 상호배타적이라 공유 가능)

		map::Sector mCurrentSector;
		map::Sector mDestSector;

		eDirection mDirection;

		int8_t mCurrentFieldID; // 현재 속해있는 Field
		int8_t mNextFieldID;    // deffered로 옮길 Field
		ePlayerState mState;
		int8_t mCharacterType;
		char mNickname[20];
		bool bConnect;
		uint64_t mSyncCnt;

		int32_t mAttackPower;
		DWORD mLastAttackTime;
		std::vector<int64_t> mPendingAttackTargets;   // handleAttackReq에서 받은 후보 → windup 시점에 재검증
		int8_t mPendingAttackSkillId;   // windup 시점 재검증용 (rangeLen/rangeWidth 재조회)
		int32_t mHitStunFrame;   // Stun 지속시간 카운트다운 (mAnimFrame과 별개, Monster와 동일 패턴)

		int32_t mHp;
		int32_t mMaxHp;

		std::unordered_map<int8_t, int32_t> mInventory;   // itemId → 개수, 임시 인메모리

		// 최상위 비트(1ULL << 63)만 사용: disconnect 시 위치저장 요청을 보내고
		// 아직 dbThread 응답을 못 받은 상태를 표시. 이 비트가 켜져있는 동안엔
		// notifyDisconnect/players.erase를 미룬다 (재접속 시 낡은 위치를 덮어쓰는
		// 순서 꼬임 방지). 응답 오면 이 비트를 끄고 그 자리에서 바로 처리.
		uint64_t mDBRequestCnt;
		int64_t mSendMsgCnt;
	};
}