#pragma once
#include "Actor.h"

namespace actors
{
	class FieldItemActor : public Actor
	{
	public:
		FieldItemActor(int64_t itemUniqueId, float x, float y, eItemId itemId);
	protected:
		virtual void actorUpdate() override;
	private:
		void loadItemSprite(eItemId itemId);
	private:
		float mBaseY;    // bobbing 기준점 (서버 좌표, 흔들림에 영향 안 받음)
		int32_t mBobTick;
	};
}
