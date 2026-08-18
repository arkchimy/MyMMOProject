#pragma once
#include "../../_Shared/Protocol.h"
namespace render
{
	class Image;
	class Animation;
	enum class eAnimationType : __int8;
}
namespace actors
{
	enum class eActorState : __int8
	{
		Idle,
		Move,
		Attack,
		Stun,
		Down,
		Guard,
	};

	enum game_Config
	{
		GAME_CONFIG_STATE_CNT = 6, // eActorState
		GAME_CONFIG_DIRECTION_CNT = 8, // eDirection

	};
	class Actor
	{
	public:
		Actor();
		Actor(int64_t characterID);
		virtual ~Actor();

	public:
		void Update();
		void Render(const float cameraX, const float cameraY);
		void Attack(const int fastForwardTicks = 0)
		{
			mState = eActorState::Attack;
			changeAnimation(mDirection, eActorState::Attack, fastForwardTicks);
		}
		inline float GetX() const { return mX; }
		inline float GetY() const { return mY; }
		int64_t GetCharacterId() const { return mCharacterID; }
		eDirection GetDirection() const { return mDirection; }
		inline float GetSpeed() const { return mSpeed; }
		void MoveStart(const __int8 direction, const float x, const float y, const float speed = 1.f);
		void MoveStop(const __int8 direction, const float x, const float y);
		inline bool IsMoveState() const { return mbMove; }
	protected:
		// update
		virtual void actorUpdate() = 0;
		void moveUpdate();

	protected:
		void createVertexBuffer();
		void createConstantBuffer();

		virtual void updateConstantBuffer(const float cameraX, const float cameraY);
		void appendAnimationSprite(const render::eAnimationType& type, const eDirection& direction, const eActorState state, const char* filename
			, const int frameCnt, const int colMax, const int frameX = 128, const int frameY = 128, const int startRow = 0,int32_t speed = 4);
		void changeAnimation(const eDirection direction, const eActorState state, const int fastForwardTicks = 0);
	private:
		struct ID3D11Buffer* mVertexBuffer;
		struct ID3D11Buffer* mConstantBuffer;
		struct ID3D11ShaderResourceView* mSRV;

		__int32 mIndexCount;
		render::Animation* mSpriteSet[GAME_CONFIG_STATE_CNT][GAME_CONFIG_DIRECTION_CNT];
		render::Animation* mSprite;

	protected:

		eActorState mState;
		eDirection mDirection;
		bool mbMove;
		float mSpeed;
	protected:
		//움직이기 시작한 스타트 지점
		int64_t mCharacterID;
		float mStartX;
		float mStartY;
		int64_t mMoveFrame;

		float mX;
		float mY;
		float mScale;
	};
}