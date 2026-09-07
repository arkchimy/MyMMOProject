#pragma once
#include <cstdint>
#include "Actor.h"

namespace actors
{
    class RemotePlayer final : public Actor
    {
    public:
        RemotePlayer(const int64_t characterId, const float x, const float y, const int8_t characterType, eDirection direction = eDirection::Down);
        void OnDamaged(int32_t hp, float x, float y, eDirection direction, int32_t fastForwardTicks = 0);

    private:
        virtual void actorUpdate() override;   // 키 입력 없음. 이동은 나중에 MOVE 패킷으로 처리

        void loadTorokoSprite();   // Player::Foo() 복사본

    private:
        int8_t mCharacterType;   // 저장만, 아직 로직에서 사용 안 함
        int32_t mHp;
        int32_t mHitTimer;
    };
}