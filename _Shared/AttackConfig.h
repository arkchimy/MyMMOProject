#pragma once
#include "Protocol.h"
#include <cmath>

namespace Config
{
	struct Position
	{
		const float operator-(const Position& other)
		{
			//거리 반환
			return std::sqrt((x - other.x) * (x - other.x) + (y - other.y) * (y - other.y));
		}
		bool operator != (const Position& other)
		{
			return x != other.x || y != other.y;
		}
		float x;
		float y;
	};
}

enum : int32_t
{
	PLAYER_HIT_STUN_FRAME = 30,        // 플레이어의 스턴시간
	PLAYER_ATTACK_TOTAL_FRAME = 30,    // Player_idle로 돌아오는시간

	MONSTER_HIT_STUN_FRAME = 50,       // 몬스터의 스턴시간
	MONSTER_ATTACK_TOTAL_FRAME = 80,   // monster_idle로 돌아오는시간

	ALLOW_DELAY_FRAME = 25,			   // 오차 허락 프레임 한계치

	PLAYER_ATTACK_AWINDUP_FRAME = 8,   // Player의 공격 판정 프레임
	PLAYER_ATTACK_DAMAGE = 35,

	MONSTER_ATTACK_WINDUP_FRAME = 20,  // 이 틱에 판정 실행

	MONSTER_ATTACK_DAMAGE = 10,
	PLAYER_MAX_HP = 100,
};

// float는 enum에 담을 수 없어 그대로 constexpr 유지
constexpr float PLAYER_ATTACK_KNOCKBACK_DIST = 100.f;
constexpr float MONSTER_ATTACK_KNOCKBACK_DIST = 50.f;
constexpr float MONSTER_ATTACK_RANGE = 50.f;          // 이 거리 이내면 Chase 대신 Attack 진입
constexpr float LOOT_RANGE = 50.f;

inline bool getSkillConfig(int8_t skillId, float& outLen, int16_t& outMaxTargetCnt)
{
	switch (skillId)
	{
	case 0: // 기본 공격
		outLen = 160;
		outMaxTargetCnt = 1;
		return true;
	default:
		return false;
	}
}

inline bool isInAttackRange(float attackerX, float attackerY, float targetX, float targetY, float rangeLen)
{
	Config::Position attackerPos(attackerX, attackerY);
	Config::Position targetPos(targetX, targetY);
	float dis = attackerPos - targetPos;

	return dis <= rangeLen;
}

inline eDirection getDirectionTo(float fromX, float fromY, float toX, float toY)
{
	float dx = toX - fromX;
	float dy = toY - fromY;
	float len = std::sqrt(dx * dx + dy * dy);
	if (len < 0.0001f)
	{
		return eDirection::Down;
	}
	dx /= len;
	dy /= len;

	eDirection best = eDirection::Down;
	float bestDot = -2.f;
	for (int i = 0; i < static_cast<int>(eDirection::Max); ++i)
	{
		const DirectionVector& v = kDirectionVectorTable[i];
		float dot = dx * v.mX + dy * v.mY;
		if (dot > bestDot)
		{
			bestDot = dot;
			best = static_cast<eDirection>(i);
		}
	}
	return best;
}
