#pragma once
#include "ISceneBase.h"
#include <unordered_map>
#include <cstdint>
namespace contents
{
	enum class eMonsterState : int8_t
	{
		Idle,
		Move,
		Chase,
		Attack,
		Stun,
		Dead,
		Return,//  스폰지역하고 매우 멀어질경우 스폰지역으로 되돌아감.
	};

	enum class ePlayerState : int8_t
	{
		Idle,
		Move,
		Attack,
		Dead,
		Stun,
	};
}

namespace ui
{
	class UIBase;
}
namespace map
{
	class TileMap;
}
namespace actors
{
	class Actor;
}
namespace render
{
	class Camera;
}
namespace utility
{
	class Message;
}
namespace scene
{
	enum Field_Config
	{
		FIELD_CONFIG_UI_LEN = 1
	};
	class FieldScene :public ISceneBase
	{
	public:
		FieldScene(actors::Actor* player);
		~FieldScene();

		FieldScene(const FieldScene& other) = delete;
		FieldScene(const FieldScene&& rvalue) = delete;
		const FieldScene& operator = (const FieldScene& rhs) = delete;
		const FieldScene& operator = (const FieldScene&& rhs) = delete;

	private:
		virtual void update();
		virtual void render();
		virtual ISceneBase* getNextSceneOrNull();
		bool PacketProc(utility::Message& msg);

		void onOtherCharacterSpawn(utility::Message& msg);
		void onOtherCharacterSpawnBatch(utility::Message& msg);
		void onCharacterDespawn(utility::Message& msg);
		void onCharacterMoveStart(utility::Message& msg);
		void onCharacterMoveStop(utility::Message& msg);
		void onOtherCharacterAttack(utility::Message& msg);
		void onMonsterSpawn(utility::Message& msg);
		void onMonsterSpawnBatch(utility::Message& msg);
		void onMonsterDespawn(utility::Message& msg);
		void onMonsterMoveStart(utility::Message& msg);
		void onMonsterMoveStop(utility::Message& msg);
		void onMonsterDamaged(utility::Message& msg);
		void onMonsterAttack(utility::Message& msg);
		void onCharacterDamaged(utility::Message& msg);
		void onItemSpawn(utility::Message& msg);
		void onItemSpawnBatch(utility::Message& msg);
		void onItemDespawn(utility::Message& msg);
		void onLootRes(utility::Message& msg);
		void onRankingRes(utility::Message& msg);

		void updateAttackInput();
		void updateLootInput();
		void updateRankingInput();

	private:
		ISceneBase* mNextScene;
		map::TileMap* mTileMap;
		std::unordered_map<__int64, actors::Actor*> mActors;
		std::unordered_map<__int64, actors::Actor*> mMonsterActors;
		std::unordered_map<__int64, actors::Actor*> mItemActors;
		actors::Actor* mLocalPlayer;
		render::Camera* mCamera;

		ui::UIBase* UIArr[FIELD_CONFIG_UI_LEN];
		bool mbAttackKeyDown = false;
		bool mbLootKeyDown = false;
		bool mbRankingKeyDown = false;
	};
}