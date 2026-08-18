#pragma once
#include "Actor.h"
#include <cstdint>

namespace actors
{
	class Player : public Actor
	{
	public:
		Player(__int64 characterID,float x, float y);
		virtual void actorUpdate() override;
		void Foo();
		bool TryAttack();
		void OnDamaged(__int32 hp, float x, float y, eDirection direction);
		bool IsAlive() const { return mHp > 0; }
		int32_t GetHp() const { return mHp; }
		int32_t GetMaxHp() const { return mMaxHp; }
	private:
		void sendMoveStartPacket();
		void sendMoveStopPacket();

		void inputFunction();
	private:
		int mAttackFrame;
		int32_t mHp;
		int32_t mMaxHp;
		int mHitTimer;
	};

} // namespace actor