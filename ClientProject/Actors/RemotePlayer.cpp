#include "RemotePlayer.h"
#include "../Render/Animation.h"
#include "../../_Shared/AttackConfig.h"
#include <algorithm>

//Attack
//Down
//Hit
//Guard
//Idle
//Move
namespace toroko
{
	const char* etoroko_filename = "Asset/pet/toroka.png";

	enum etoroko
	{
		Attack_Len = 12, //EndStop
		Down_Len = 10,  // EndStop

		Hit_Len = 2,    // EndStop
		Guard_Len = 1, // EndStop
		idle_Len = 12,   // Loop

		Move_Len = 9,  // 

		frameX = 128,
		frameY = 128,

		colMax = 19,
	};
};


namespace actors
{
	RemotePlayer::RemotePlayer(const __int64 characterId, const float x, const float y, const __int8 characterType, eDirection direction)
		: Actor(characterId)
		, mCharacterType(characterType)
		, mHp(PLAYER_MAX_HP)
		, mHitTimer(0)
	{
		mX = x;
		mY = y;
		mScale = 1.5f;
		mDirection = direction;
		loadTorokoSprite();
		createVertexBuffer();
		createConstantBuffer();
		changeAnimation(mDirection, mState);
	}
	void RemotePlayer::actorUpdate()
	{
		if (mHitTimer > 0)
		{
			--mHitTimer;
			if (mHitTimer == 0)
			{
				changeAnimation(mDirection, mbMove ? eActorState::Move : eActorState::Idle);
			}
			return;
		}

		if (mbMove)
		{
			moveUpdate();
		}
	}
	void RemotePlayer::OnDamaged(__int32 hp, float x, float y, eDirection direction, int fastForwardTicks)
	{
		mHp = hp;
		mX = x;
		mY = y;
		mStartX = x;
		mStartY = y;
		mMoveFrame = 0;
		mDirection = direction;
		mbMove = false;

		if (mHp <= 0)
		{
			mHitTimer = 0;
			changeAnimation(mDirection, eActorState::Down, fastForwardTicks);
		}
		else
		{
			mHitTimer = std::max(0, PLAYER_HIT_STUN_FRAME - fastForwardTicks);
			changeAnimation(mDirection, eActorState::Stun, fastForwardTicks);
		}
	}
	void RemotePlayer::loadTorokoSprite()
	{
		int offset = 0;
		for (int i = 0; i < 8; ++i)
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
				, toroko::etoroko_filename, toroko::Attack_Len, toroko::colMax, toroko::frameX, toroko::frameY, offset);
			offset += toroko::Attack_Len;
			//Down
			appendAnimationSprite(render::eAnimationType::EndStop, dir, eActorState::Down
				, toroko::etoroko_filename, toroko::Down_Len, toroko::colMax, toroko::frameX, toroko::frameY, offset);
			offset += toroko::Down_Len;
			//Hit
			appendAnimationSprite(render::eAnimationType::EndStop, dir, eActorState::Stun
				, toroko::etoroko_filename, toroko::Hit_Len, toroko::colMax, toroko::frameX, toroko::frameY, offset);
			offset += toroko::Hit_Len;
			//Guard
			appendAnimationSprite(render::eAnimationType::EndStop, dir, eActorState::Guard
				, toroko::etoroko_filename, toroko::Guard_Len, toroko::colMax, toroko::frameX, toroko::frameY, offset);
			offset += toroko::Guard_Len;
			//Idle
			appendAnimationSprite(render::eAnimationType::Loop, dir, eActorState::Idle
				, toroko::etoroko_filename, toroko::idle_Len, toroko::colMax, toroko::frameX, toroko::frameY, offset);
			offset += toroko::idle_Len;
			//Move
			appendAnimationSprite(render::eAnimationType::Loop, dir, eActorState::Move
				, toroko::etoroko_filename, toroko::Move_Len, toroko::colMax, toroko::frameX, toroko::frameY, offset);
			offset += toroko::Move_Len;
		}
	}
}
