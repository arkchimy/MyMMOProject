#pragma once
#include "Actor.h"

namespace actors
{
    class RemotePlayer : public Actor
    {
    public:
        RemotePlayer(const __int64 characterId, const float x, const float y, const __int8 characterType, eDirection direction = eDirection::Down);
        void OnDamaged(__int32 hp, float x, float y, eDirection direction, int fastForwardTicks = 0);

    private:
        virtual void actorUpdate() override;   // 키 입력 없음. 이동은 나중에 MOVE 패킷으로 처리

        void loadTorokoSprite();   // Player::Foo() 복사본

    private:
        __int8 mCharacterType;   // 저장만, 아직 로직에서 사용 안 함
        int32_t mHp;
        int mHitTimer;
    };
}