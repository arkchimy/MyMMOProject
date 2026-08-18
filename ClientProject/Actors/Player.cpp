#include "Network/Network.h"
#include "Game.h"
#include "Player.h"
#include "../Render/Animation.h"
#include "utility/Common.h"
#include "AttackConfig.h"
#include <timeapi.h>
const char* etoroko_filename = "Asset/pet/toroka.png";

//Attack
//Down
//Hit
//Guard
//Idle
//Move
namespace toroko
{
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
	void Player::Foo()
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
				, etoroko_filename, toroko::Attack_Len, toroko::colMax, toroko::frameX, toroko::frameY, offset,2);
			offset += toroko::Attack_Len;
			//Down
			appendAnimationSprite(render::eAnimationType::EndStop, dir, eActorState::Down
				, etoroko_filename, toroko::Down_Len, toroko::colMax, toroko::frameX, toroko::frameY, offset);
			offset += toroko::Down_Len;
			//Hit
			appendAnimationSprite(render::eAnimationType::EndStop, dir, eActorState::Stun
				, etoroko_filename, toroko::Hit_Len, toroko::colMax, toroko::frameX, toroko::frameY, offset);
			offset += toroko::Hit_Len;
			//Guard
			appendAnimationSprite(render::eAnimationType::EndStop, dir, eActorState::Guard
				, etoroko_filename, toroko::Guard_Len, toroko::colMax, toroko::frameX, toroko::frameY, offset);
			offset += toroko::Guard_Len;
			//Idle
			appendAnimationSprite(render::eAnimationType::Loop, dir, eActorState::Idle
				, etoroko_filename, toroko::idle_Len, toroko::colMax, toroko::frameX, toroko::frameY, offset);
			offset += toroko::idle_Len;
			//Move
			appendAnimationSprite(render::eAnimationType::Loop, dir, eActorState::Move
				, etoroko_filename, toroko::Move_Len, toroko::colMax, toroko::frameX, toroko::frameY, offset);
			offset += toroko::Move_Len;
		}
	}

	void Player::sendMoveStartPacket()
	{
		PacketType::MOVE_START;
		Header header{0};
		header.Len = sizeof(__int16) + sizeof(mCharacterID) + sizeof(mX) + sizeof(mY) + sizeof(mDirection) + sizeof(mSpeed);
		header.RandKey = 0;

		utility::Message msg;
		msg.InitMessage(0, 0);
		msg.PutData(&header, sizeof(header));
		msg << static_cast<__int16>(PacketType::MOVE_START);
		msg << mCharacterID << mX << mY << static_cast<__int8>(mDirection) << mSpeed;
		// TODO : 연결이 끊겼을때  대처방안 생각하기.
		RT_ASSERT(g_Network.Send(msg) == true);
	}

	void Player::sendMoveStopPacket()
	{
		PacketType::MOVE_STOP;
		Header header{0};
		header.Len = sizeof(__int16) + sizeof(mCharacterID) + sizeof(mX) + sizeof(mY) + sizeof(mDirection);
		header.RandKey = 0;

		utility::Message msg;
		msg.InitMessage(0, 0);
		msg.PutData(&header, sizeof(header));
		msg << static_cast<__int16>(PacketType::MOVE_STOP);
		msg << mCharacterID << mX << mY << static_cast<__int8>(mDirection);
		// TODO : 연결이 끊겼을때  대처방안 생각하기.
		RT_ASSERT(g_Network.Send(msg) == true);
	}

	void Player::inputFunction()
	{

		bool bMove = false;
		eDirection before_Dir = mDirection;
		eActorState before_state = mState;

		if (GetForegroundWindow() == Game::GetInstance().GetWindows())
		{
			if (GetAsyncKeyState('A') & 0x8000)
			{
				mDirection = eDirection::Left;
				bMove = true;
			}
			else if (GetAsyncKeyState('D') & 0x8000)
			{
				mDirection = eDirection::Right;
				bMove = true;
			}

			if (GetAsyncKeyState('W') & 0x8000)
			{
				if (mDirection == eDirection::Right)
				{
					mDirection = eDirection::UpRight;
				}
				else if (mDirection == eDirection::Left)
				{
					mDirection = eDirection::UpLeft;
				}
				else
				{
					mDirection = eDirection::Up;
				}
				bMove = true;
			}
			else if (GetAsyncKeyState('S') & 0x8000)
			{
				if (mDirection == eDirection::Right)
				{
					mDirection = eDirection::DownRight;
				}
				else if (mDirection == eDirection::Left)
				{
					mDirection = eDirection::DownLeft;
				}
				else
				{
					mDirection = eDirection::Down;
				}
				bMove = true;
			}
		}
		if (bMove)
		{
			mState = eActorState::Move;
		}
		else
		{
			mState = eActorState::Idle;
		}
		if (mState != before_state)
		{
			mMoveFrame = 0;
			mStartX = mX;
			mStartY = mY;
			// 이제 움직이는 것이라면,
			if (mState == eActorState::Move)
			{
				sendMoveStartPacket();
			}
			// 이제 멈추는 것이라면,
			else if (mState == eActorState::Idle)
			{
				sendMoveStopPacket();
			}
			changeAnimation(mDirection, mState);
		}
		else if (mDirection != before_Dir && bMove)
		{
			// 멈추지않고 방향만 바꾸기.
			mMoveFrame = 0;
			mStartX = mX;
			mStartY = mY;
			sendMoveStopPacket();
			sendMoveStartPacket();
			changeAnimation(mDirection, mState);
		}
		if (bMove)
		{
			moveUpdate();
		}
		else
		{
			mState = eActorState::Idle;
			mbMove = false;
			mMoveFrame = 0;
			mStartX = mX;
			mStartY = mY;
		}
	}

	void Player::OnDamaged(__int32 hp, float x, float y, eDirection direction)
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
			mState = eActorState::Down;
			changeAnimation(mDirection, eActorState::Down);
		}
		else
		{
			mHitTimer = PLAYER_HIT_STUN_FRAME;
			mState = eActorState::Stun;
			changeAnimation(mDirection, eActorState::Stun);
		}
	}

	Player::Player(__int64 characterID, float x, float y)
		:Actor(characterID)
		, mAttackFrame(0)
		, mHp(PLAYER_MAX_HP)
		, mMaxHp(PLAYER_MAX_HP)
		, mHitTimer(0)
	{
		mScale = 1.5f;
		mX = x;
		mY = y;
		mStartX = x;
		mStartY = y;
		mSpeed = 4.5f;
		Foo();

		createVertexBuffer();
		createConstantBuffer();
	}
	bool Player::TryAttack()
	{
		if (mHp <= 0 || mHitTimer > 0)
		{
			return false;
		}

		if (mState == eActorState::Attack)
		{
			return false;
		}

		if (mbMove)
		{
			sendMoveStopPacket();
			mbMove = false;
			mMoveFrame = 0;
			mStartX = mX;
			mStartY = mY;
		}
		mAttackFrame = 0;
		Attack();
		return true;
	}
	void Player::actorUpdate()
	{
		switch (mState)
		{
		case eActorState::Idle:
		case eActorState::Move:
			inputFunction();
			break;
		case eActorState::Attack:
			++mAttackFrame;
			if (PLAYER_ATTACK_TOTAL_FRAME == mAttackFrame)
			{
				mState = eActorState::Idle;
				changeAnimation(mDirection, mState);
			}
			break;
		case eActorState::Stun:
			--mHitTimer;
			if (mHitTimer == 0)
			{
				mState = eActorState::Idle;
				changeAnimation(mDirection, mState);
			}
			return;
		case eActorState::Down:
			break;
		case eActorState::Guard:
			break;
		}


	}

}; // namespace actor