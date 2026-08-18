#include "Monster.h"
#include "../../_Shared/AttackConfig.h"
#include <algorithm>
#include <cstdlib>
#include "../Common.h"
#include "Player.h"
namespace
{
	constexpr float PATROL_RADIUS = 150.f;
	constexpr float CHASE_LEASH_RADIUS = 2400.f;   // 스폰 위치 기준 이 거리 넘으면 추적 포기
	constexpr int32_t MONSTER_MAX_HP = 100;
	constexpr int32_t MONSTER_RESPAWN_FRAME = 500;   // 20ms 틱 * 500 = 10초
}

namespace contents
{
	Monster::Monster(int64_t monsterID, const map::Position& pos, int8_t monsterType)
		: mMonsterID(monsterID)
		, mPos(pos)
		, mSpawnPos(pos)
		, mStartMovePos(pos)
		, mSector{ 0, 0 }
		, mDestSector{ 0, 0 }
		, mMonsterType(monsterType)
		, mDirection(eDirection::Down)
		, mBeforeDirection(eDirection::Down)
		, mState(eMonsterState::Idle)
		, mBeforeState(eMonsterState::Idle)
		, mAnimFrame(0)
		, mSpeed(3.f)
		, mHp(MONSTER_MAX_HP)
		, mMaxHp(MONSTER_MAX_HP)
		, mHitStunFrame(0)
		, mTargetPlayerID(-1)
		, mTargetPos(pos)
		, mKnockbackDist(30.f)

	{
	}

	void Monster::Update()
	{
		++mAnimFrame;

		switch (mState)
		{
		case eMonsterState::Idle:
			idleUpdate();
			break;
		case eMonsterState::Move:
			patrolUpdate();
			break;
		case eMonsterState::Chase:
			chaseUpdate(mTargetPos.x, mTargetPos.y);
			break;
		case eMonsterState::Attack:
			attackUpdate();
			break;
		case eMonsterState::Stun:
			stunUpdate();
			break;
		case eMonsterState::Dead:
			deadUpdate();
			break;
		case eMonsterState::Return:
			returnUpdate();
			break;
		default:
			MyAssert(false, "정의되지않은 상태");
		}
	}

	void Monster::idleUpdate()
	{
		// 순찰 범위 초과 시 스폰 위치로 방향을 잡음
		if (PATROL_RADIUS < (mPos - mSpawnPos))
		{
			bool leftDir = mSpawnPos.x < mPos.x ? true : false;
			bool upDir = mSpawnPos.y < mPos.y ? true : false;

			if (mSpawnPos.x == mPos.x)
			{
				mDirection = upDir ? eDirection::Up : eDirection::Down;
			}
			else if (mSpawnPos.y == mPos.y)
			{
				mDirection = leftDir ? eDirection::Left : eDirection::Right;
			}
			else if (leftDir)
			{
				mDirection = upDir ? eDirection::UpLeft : eDirection::DownLeft;
			}
			else
			{
				mDirection = upDir ? eDirection::UpRight : eDirection::DownRight;
			}
			changeState(eMonsterState::Move);
		}
		// 1% 확률로 랜덤 방향 이동 시작
		else if (rand() % 100 == 0 && 10 <= mAnimFrame)
		{
			mDirection = static_cast<eDirection>(rand() % static_cast<int>(eDirection::Max));
			changeState(eMonsterState::Move);
		}

	}

	void Monster::patrolUpdate()
	{
		// 순찰 범위 초과 시 강제 정지
		// 1% 확률로 정지

		// 너무 멀어지면 되돌아 오기.
		if (PATROL_RADIUS <= (mPos - mSpawnPos))
		{
			changeState(eMonsterState::Return);
		}
		// 정지 확률
		else if (rand() % 100 == 0 && 10 <= mAnimFrame)
		{
			changeState(eMonsterState::Idle);
			return;
		}
		// 움직임 로직
		else
		{
			moveUpdate();
		}
	}

	void Monster::giveUpChase()
	{
		if (PATROL_RADIUS < (mPos - mSpawnPos))
		{
			changeState(eMonsterState::Return);
		}
		else
		{
			changeState(eMonsterState::Idle);
		}
	}

	void Monster::chaseUpdate(float targetX, float targetY)
	{
		if (CHASE_LEASH_RADIUS < (mPos - mSpawnPos))
		{
			giveUpChase();
			return;
		}

		eDirection newDirection = getDirectionTo(mPos.x, mPos.y, targetX, targetY);

		// 사거리 안에 들어오면 더 접근하지 않고 공격으로 전이 (타겟 방향으로 고정)
		if ((mPos - map::Position{ targetX, targetY }) <= MONSTER_ATTACK_RANGE)
		{
			changeDirection(newDirection);
			changeState(eMonsterState::Attack);
			return;
		}

		// 방향이 바뀌었거나(타겟이 움직여서), 이번 이동 구간이 처음 시작하는 틱이면
		// 이동 기준점(mStartMovePos/mAnimFrame)을 새로 잡음
		if (newDirection != mDirection && 10 <= mAnimFrame)
		{
			changeDirection(newDirection);
		}
		moveUpdate();
	}

	void Monster::attackUpdate()
	{
		// 판정 자체는 FieldServer가 mAnimFrame==MONSTER_ATTACK_WINDUP_FRAME 시점에 수행.
		// 여기서는 총 지속시간이 끝났는지만 관리.
		if (mAnimFrame >= MONSTER_ATTACK_TOTAL_FRAME)
		{
			changeState(eMonsterState::Chase);
		}
	}

	void Monster::stunUpdate()
	{
		if (mHitStunFrame > 0)
		{
			--mHitStunFrame;
			return;
		}
		// 스턴이 끝나면 다시 추적 시작 (타겟은 takeDamage에서 이미 세팅해둠)
		changeState(eMonsterState::Chase);
	}

	void Monster::deadUpdate()
	{
		if (mAnimFrame >= MONSTER_RESPAWN_FRAME)
		{
			changeState(eMonsterState::Idle);
			mStartMovePos = mSpawnPos;
			mPos = mSpawnPos;
			mHp = mMaxHp;
		}
	}

	void Monster::returnUpdate()
	{
		if ((mPos - mSpawnPos) <= 10.f)
		{
			changeState(eMonsterState::Idle);
		}
		else if (15 <= mAnimFrame)
		{
			auto newDir = getDirectionTo(mPos.x, mPos.y, mSpawnPos.x, mSpawnPos.y);
			changeDirection(newDir);
		}
		moveUpdate();
	}

	void Monster::resetAnimStart()
	{
		mAnimFrame = 0;
		mStartMovePos = mPos;
	}

	void Monster::changeState(const eMonsterState& state)
	{
		MyAssert(state != mState, "상태 전이 실패");
		mState = state;
		resetAnimStart();
	}

	void Monster::changeDirection(const eDirection& newDir)
	{
		mDirection = newDir;
		mAnimFrame = 0;
		mStartMovePos = mPos;
	}

	void Monster::moveUpdate()
	{
		float dx = kDirectionVectorTable[static_cast<int8_t>(mDirection)].mX;
		float dy = kDirectionVectorTable[static_cast<int8_t>(mDirection)].mY;
		mPos.x = std::clamp(mStartMovePos.x + dx * mSpeed * mAnimFrame, 0.0f, static_cast<float>(SECTOR_WORLD_W * SECTOR_COL_CNT));
		mPos.y = std::clamp(mStartMovePos.y + dy * mSpeed * mAnimFrame, 0.0f, static_cast<float>(SECTOR_WORLD_H * SECTOR_ROW_CNT));
	}

	void Monster::takeDamage(int32_t damage, const Player* const attacker, int64_t attackerId)
	{
		if (mState == eMonsterState::Dead)
		{
			return;
		}

		mDirection = getDirectionTo(mPos.x, mPos.y, attacker->GetPosition().x, attacker->GetPosition().y);
		const DirectionVector& facing = kDirectionVectorTable[static_cast<int8_t>(attacker->GetDirection())];

		mPos.x = std::clamp(mPos.x + facing.mX * mKnockbackDist, 0.0f, static_cast<float>(SECTOR_WORLD_W * SECTOR_COL_CNT));
		mPos.y = std::clamp(mPos.y + facing.mY * mKnockbackDist, 0.0f, static_cast<float>(SECTOR_WORLD_H * SECTOR_ROW_CNT));

		mHp = std::max(mHp - damage, 0);
		if (mHp == 0)
		{
			changeState(eMonsterState::Dead);
		}
		else
		{
			mHitStunFrame = MONSTER_HIT_STUN_FRAME;
			mTargetPlayerID = attackerId;

			if (mState == eMonsterState::Stun)
			{
				resetAnimStart();               // 이미 스턴 중 재피격 — 상태 전이 없이 애니메이션만 재시작
			}
			else
			{
				changeState(eMonsterState::Stun);
			}
		}
	}
}
