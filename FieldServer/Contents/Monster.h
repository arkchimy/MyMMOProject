#pragma once
#include "MapConfig.h"
#include "../../_Shared/Protocol.h"
#include <cstdint>

namespace contents
{
	enum class eMonsterState : int8_t
	{
		Idle,
		Move,
		Chase,
		Attack,
		Stun,
		Dead,
		Return,//  스폰지역하고 매우 멀어질경우 스폰지역으로 되돌아감.
	};

	class Monster
	{
		friend class FieldServer;
	public:
		Monster(int64_t monsterID, const map::Position& pos, int8_t monsterType);
		int32_t GetAnimFrame() const { return mAnimFrame; }   // 현재 상태 진입 후 경과 틱수 (상태 전환마다 0으로 리셋)
		inline eDirection GetDirection() const { return mDirection; }
		map::Position GetPosition() const { return mPos; }
		void Update();
	private:
		void idleUpdate();
		void patrolUpdate();
		void chaseUpdate(float targetX, float targetY);
		void attackUpdate();
		void stunUpdate();
		void deadUpdate();
		void returnUpdate();

		void changeState(const eMonsterState& state);
		void changeDirection(const eDirection& newDir);
		void resetAnimStart();   // 상태 진입 시 애니메이션 기준점 리셋 (changeState와 재스턴 경로가 공유)

		void moveUpdate();

		void takeDamage(int32_t damage, const class Player* const attacker, int64_t attackerId);
		void giveUpChase();

	private:
		int64_t mMonsterID;
		map::Position mPos;
		map::Position mSpawnPos;
		map::Position mStartMovePos;
		map::Sector mSector;
		map::Sector mDestSector;
		int8_t mMonsterType;

		eDirection mDirection;
		eDirection mBeforeDirection;
		eMonsterState mState;
		eMonsterState mBeforeState;
		int32_t mAnimFrame;   // mMoveFrame → 범용화 (이동거리 계산 + 애니메이션 프레임 동기화 겸용)
		float mSpeed;

		int32_t mHp;
		int32_t mMaxHp;
		int32_t mHitStunFrame;

		int64_t mTargetPlayerID;   // 추적 중인 플레이어의 mSeqID.Value(FieldServer::players 조회 키), -1이면 없음
		map::Position mTargetPos;

		float mKnockbackDist;
	};
}
