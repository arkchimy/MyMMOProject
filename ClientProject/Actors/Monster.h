#pragma once
#include "Actor.h"

namespace actors
{
	class Monster : public Actor
	{
	public:
		Monster(const __int64 monsterId, const float x, const float y, const __int8 monsterType, eDirection direction, __int32 hp, __int32 animFrame);

		void OnDamaged(__int32 hp, float x, float y, eDirection direction);
		bool IsAlive() const { return mHp > 0; }

	private:
		virtual void actorUpdate() override;

		void loadMonsterSprite();

	private:
		int8_t mMonsterType;   // 저장만, 아직 로직에서 사용 안 함
		int32_t mHp;
		int32_t mHitTimer;
	};
}
