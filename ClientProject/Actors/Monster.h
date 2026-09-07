#pragma once
#include <cstdint>
#include "Actor.h"

namespace actors
{
	class Monster final : public Actor
	{
	public:
		Monster(const int64_t monsterId, const float x, const float y, const int8_t monsterType, eDirection direction, int32_t hp, int32_t animFrame);

		void OnDamaged(int32_t hp, float x, float y, eDirection direction);
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
