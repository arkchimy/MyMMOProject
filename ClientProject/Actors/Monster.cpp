#include "Monster.h"
#include "../Render/Animation.h"
#include "../../_Shared/AttackConfig.h"

namespace karfe
{
	const char* ekarfe_filename = "Asset/pet/Karfe.png";
	enum ekarfe
	{
		Move_Len = 9,  // 

		Attack_Len = 8, //EndStop
		Down_Len = 8,  // EndStop

		Hit_Len = 2,    // EndStop
		Guard_Len = 1, // EndStop
		idle_Len = 6,   // Loop

		frameX = 128,
		frameY = 128,
		colMax = 16,
	};
}

namespace actors
{
	Monster::Monster(const int64_t monsterId, const float x, const float y, const int8_t monsterType, eDirection direction, int32_t hp, int32_t animFrame)
		: Actor(monsterId)
		, mMonsterType(monsterType)
		, mHp(hp)
		, mHitTimer(0)
	{
		mX = x;
		mY = y;
		mScale = 1.5f;
		mSpeed = 3.0f;
		mDirection = direction;
		loadMonsterSprite();
		createVertexBuffer();
		createConstantBuffer();
		changeAnimation(mDirection, mHp > 0 ? eActorState::Idle : eActorState::Down, animFrame);
	}
	void Monster::actorUpdate()
	{
		if (mHitTimer > 0)
		{
			--mHitTimer;
			if (mHitTimer == 0)
			{
				changeAnimation(mDirection, mBMove ? eActorState::Move : eActorState::Idle);
			}
			return;
		}

		if (mBMove)
		{
			moveUpdate();
		}
	}
	void Monster::OnDamaged(int32_t hp, float x, float y, eDirection direction)
	{
		mHp = hp;
		mX = x;
		mY = y;
		mStartX = x;
		mStartY = y;
		mMoveFrame = 0;
		mDirection = direction;

		mBMove = false;
		if (mHp <= 0)
		{
			mHitTimer = 0;
			changeAnimation(mDirection, eActorState::Down);
		}
		else
		{
			mHitTimer = MONSTER_HIT_STUN_FRAME;
			changeAnimation(mDirection, eActorState::Stun);
		}
	}
	void Monster::loadMonsterSprite()
	{
		int32_t offset = 0;
		for (int32_t i = 0; i < 8; ++i)
		{
			eDirection dir;
			switch (i)
			{
			case 0:
				dir = eDirection::Down;
				break;
			case 1:
				dir = eDirection::DownLeft;
				break;
			case 2:
				dir = eDirection::Left;
				break;
			case 3:
				dir = eDirection::UpLeft;
				break;
			case 4:
				dir = eDirection::Up;
				break;
			case 5:
				dir = eDirection::UpRight;
				break;
			case 6:
				dir = eDirection::Right;
				break;
			case 7:
				dir = eDirection::DownRight;
				break;
			}
			//attack
			appendAnimationSprite(render::eAnimationType::EndStop, dir, eActorState::Attack
				, karfe::ekarfe_filename, karfe::Attack_Len, karfe::colMax, karfe::frameX, karfe::frameY, offset);
			offset += karfe::Attack_Len;
			//Down
			appendAnimationSprite(render::eAnimationType::EndStop, dir, eActorState::Down
				, karfe::ekarfe_filename, karfe::Down_Len, karfe::colMax, karfe::frameX, karfe::frameY, offset);
			offset += karfe::Down_Len;
			//Hit
			appendAnimationSprite(render::eAnimationType::EndStop, dir, eActorState::Stun
				, karfe::ekarfe_filename, karfe::Hit_Len, karfe::colMax, karfe::frameX, karfe::frameY, offset);
			offset += karfe::Hit_Len;
			//Guard
			appendAnimationSprite(render::eAnimationType::EndStop, dir, eActorState::Guard
				, karfe::ekarfe_filename, karfe::Guard_Len, karfe::colMax, karfe::frameX, karfe::frameY, offset);
			offset += karfe::Guard_Len;
			//Idle
			appendAnimationSprite(render::eAnimationType::Loop, dir, eActorState::Idle
				, karfe::ekarfe_filename, karfe::idle_Len, karfe::colMax, karfe::frameX, karfe::frameY, offset);
			offset += karfe::idle_Len;
			//Move
			appendAnimationSprite(render::eAnimationType::Loop, dir, eActorState::Move
				, karfe::ekarfe_filename, karfe::Move_Len, karfe::colMax, karfe::frameX, karfe::frameY, offset);
			offset += karfe::Move_Len;
		}
	}
}
