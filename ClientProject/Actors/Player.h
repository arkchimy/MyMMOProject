#pragma once
#include "Actor.h"
#include <cstdint>

namespace actors
{
	class Player final : public Actor
	{
	public:
		Player(int64_t characterID,float x, float y);
		void Foo();
		bool TryAttack();
		void OnDamaged(int32_t hp, float x, float y, eDirection direction);
		bool IsAlive() const { return mHp > 0; }
		int32_t GetHp() const { return mHp; }
		int32_t GetMaxHp() const { return mMaxHp; }
	private:
		virtual void actorUpdate() override;

		void sendMoveStartPacket();
		void sendMoveStopPacket();

		void inputFunction();
	private:
		int32_t mAttackFrame;
		int32_t mHp;
		int32_t mMaxHp;
		int32_t mHitTimer;
	};

} // namespace actor