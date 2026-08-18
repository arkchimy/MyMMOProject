#include <algorithm>
#include <cmath>
#include <iostream>

#include "Network/Network.h"
#include "FieldScene.h"
#include "../Map/TileMap.h"
#include "../Actors/Actor.h"
#include "../Actors/Player.h"
#include "../Actors/RemotePlayer.h"
#include "../Actors/Monster.h"
#include "../Actors/FieldItemActor.h"
#include "../Render/Camera.h"
#include "../UI/HPBar.h"
#include "utility/Message.h"
#include "Protocol.h"
#include "AttackConfig.h"
#include "../Game.h"
namespace scene
{
	FieldScene::FieldScene(actors::Actor* player)
		:mNextScene(nullptr)
		, mTileMap(nullptr)
		, mLocalPlayer(player)
		, mCamera(nullptr)
	{
		RT_ASSERT(player != nullptr);
		mTileMap = new map::TileMap();
		mCamera = new render::Camera();
		mCamera->SetTarget(player);

		UIArr[0] = new ui::HPBar();
		static_cast<ui::HPBar*>(UIArr[0])->SetTarget(static_cast<actors::Player*>(player));
	}

	FieldScene::~FieldScene()
	{
		delete mTileMap;
		delete mCamera;

		if (mLocalPlayer)
		{
			delete mLocalPlayer;
		}
		for (auto& element : mActors)
		{
			delete element.second;
		}
		for (auto& element : mMonsterActors)
		{
			delete element.second;
		}
		for (auto& element : mItemActors)
		{
			delete element.second;
		}
		mNextScene = nullptr;
		mLocalPlayer = nullptr;

		for (int i = 0; i < FIELD_CONFIG_UI_LEN; ++i)
		{
			if (UIArr[i] != nullptr)
			{
				delete UIArr[i];
			}
		}
	}

	void FieldScene::update()
	{

		for (auto& element : mActors)
		{
			element.second->Update();
		}
		for (auto& element : mMonsterActors)
		{
			element.second->Update();
		}
		for (auto& element : mItemActors)
		{
			element.second->Update();
		}
		RT_ASSERT(mLocalPlayer != nullptr);
		mLocalPlayer->Update();
		updateAttackInput();
		updateLootInput();
		updateRankingInput();

		RT_ASSERT(mCamera != nullptr);
		mCamera->Update();

		for (int i = 0; i < FIELD_CONFIG_UI_LEN; ++i)
		{
			if (UIArr[i] != nullptr)
			{
				UIArr[i]->Update();
			}
		}
	}

	void FieldScene::render()
	{
		RT_ASSERT(mCamera != nullptr);
		float camX = mCamera->GetX();
		float camY = mCamera->GetY();

		RT_ASSERT(mTileMap != nullptr);
		mTileMap->Render(camX, camY);

		// 렌더링용 정렬 리스트 (mActors는 조회용 그대로, 그리는 순서만 따로)
		std::vector<actors::Actor*> renderList;
		renderList.reserve(mActors.size() + mMonsterActors.size() + mItemActors.size() + 1);

		for (auto& element : mActors)
		{
			renderList.push_back(element.second);
		}
		for (auto& element : mMonsterActors)
		{
			renderList.push_back(element.second);
		}

		RT_ASSERT(mLocalPlayer != nullptr);
		renderList.push_back(mLocalPlayer);

		// worldY 오름차순 (Y 작음 = 뒤 = 먼저), 동률이면 characterId로 고정
		std::sort(renderList.begin(), renderList.end(),
			[](const actors::Actor* a, const actors::Actor* b)
			{
				if (a->GetY() != b->GetY())
				{
					return a->GetY() < b->GetY();
				}
				return a->GetCharacterId() < b->GetCharacterId();
			});

		for (actors::Actor* actor : renderList)
		{
			actor->Render(camX, camY);
		}
		// item 은 맨 앞으로 
		for (auto& element : mItemActors)
		{
			actors::Actor* item = element.second;
			item->Render(camX, camY);
		}

		for (int i = 0; i < FIELD_CONFIG_UI_LEN; ++i)
		{
			if (UIArr[i] != nullptr)
			{
				UIArr[i]->Render();
			}
		}
	}

	ISceneBase* FieldScene::getNextSceneOrNull()
	{
		return mNextScene;
	}
	bool FieldScene::PacketProc(utility::Message& msg)
	{
		__int16 type;
		msg >> type;

		switch ((PacketType)type)
		{
		case PacketType::OTHER_CHARACTER_SPAWN:
			onOtherCharacterSpawn(msg);
			break;
		case PacketType::OTHER_CHARACTER_SPAWN_BATCH:
			onOtherCharacterSpawnBatch(msg);
			break;
		case PacketType::CHARACTER_DESPAWN:
			onCharacterDespawn(msg);
			break;
		case PacketType::MOVE_START:
			onCharacterMoveStart(msg);
			break;
		case PacketType::MOVE_STOP:
			onCharacterMoveStop(msg);
			break;
		case PacketType::OTHER_CHARACTER_ATTACK:
			onOtherCharacterAttack(msg);
			break;
		case PacketType::MONSTER_SPAWN:
			onMonsterSpawn(msg);
			break;
		case PacketType::MONSTER_SPAWN_BATCH:
			onMonsterSpawnBatch(msg);
			break;
		case PacketType::MONSTER_DESPAWN:
			onMonsterDespawn(msg);
			break;
		case PacketType::MONSTER_MOVE_START:
			onMonsterMoveStart(msg);
			break;
		case PacketType::MONSTER_MOVE_STOP:
			onMonsterMoveStop(msg);
			break;
		case PacketType::MONSTER_DAMAGED:
			onMonsterDamaged(msg);
			break;
		case PacketType::MONSTER_ATTACK:
			onMonsterAttack(msg);
			break;
		case PacketType::CHARACTER_DAMAGED:
			onCharacterDamaged(msg);
			break;
		case PacketType::ITEM_SPAWN:
			onItemSpawn(msg);
			break;
		case PacketType::ITEM_SPAWN_BATCH:
			onItemSpawnBatch(msg);
			break;
		case PacketType::ITEM_DESPAWN:
			onItemDespawn(msg);
			break;
		case PacketType::LOOT_RES:
			onLootRes(msg);
			break;
		case PacketType::RANKING_RES:
			onRankingRes(msg);
			break;
		default:
			RT_ASSERT(false);
		}
		return true;
	}
	void FieldScene::onOtherCharacterSpawn(utility::Message& msg)
	{
		PacketType::OTHER_CHARACTER_SPAWN;
		__int64 characterId;
		float x, y;
		__int8 characterType;

		int8_t state;
		__int8 direction;
		float speed;

		__int32 animFrame;

		char nickname[20];

		msg >> characterId >> x >> y
			>> state >> direction >> speed
			>> animFrame
			>> characterType;
		msg.GetData(nickname, sizeof(nickname));

		RT_ASSERT(mActors.find(characterId) == mActors.end());
		actors::RemotePlayer* otherPlayer = new actors::RemotePlayer(characterId, x, y, characterType, static_cast<eDirection>(direction));
		RT_ASSERT(otherPlayer != nullptr);
		if (state == static_cast<int8_t>(contents::ePlayerState::Move))
		{
			otherPlayer->MoveStart(direction, x, y, speed);
		}
		else if (state == static_cast<int8_t>(contents::ePlayerState::Attack))
		{
			otherPlayer->Attack(animFrame);
		}
		else if (state == static_cast<int8_t>(contents::ePlayerState::Dead))
		{
			otherPlayer->OnDamaged(0, x, y, static_cast<eDirection>(direction), animFrame);
		}
		else if (state == static_cast<int8_t>(contents::ePlayerState::Stun))
		{
			otherPlayer->OnDamaged(1, x, y, static_cast<eDirection>(direction), animFrame);
		}
		mActors.insert({ characterId, otherPlayer });
	}
	void FieldScene::onOtherCharacterSpawnBatch(utility::Message& msg)
	{
		PacketType::OTHER_CHARACTER_SPAWN_BATCH;
		__int16 cnt;
		msg >> cnt;

		for (__int16 i = 0; i < cnt; ++i)
		{
			__int64 characterId;
			float x, y;
			__int8 characterType;

			int8_t state;
			__int8 direction;
			float speed;

			__int32 animFrame;

			char nickname[20];

			msg >> characterId >> x >> y
				>> state >> direction >> speed
				>> animFrame
				>> characterType;
			msg.GetData(nickname, sizeof(nickname));

			RT_ASSERT(mActors.find(characterId) == mActors.end());
			actors::RemotePlayer* otherPlayer = new actors::RemotePlayer(characterId, x, y, characterType, static_cast<eDirection>(direction));
			RT_ASSERT(otherPlayer != nullptr);
			if (state == static_cast<int8_t>(contents::ePlayerState::Move))
			{
				otherPlayer->MoveStart(direction, x, y, speed);
			}
			else if (state == static_cast<int8_t>(contents::ePlayerState::Attack))
			{
				otherPlayer->Attack(animFrame);
			}
			else if (state == static_cast<int8_t>(contents::ePlayerState::Dead))
			{
				otherPlayer->OnDamaged(0, x, y, static_cast<eDirection>(direction), animFrame);
			}
			else if (state == static_cast<int8_t>(contents::ePlayerState::Stun))
			{
				otherPlayer->OnDamaged(1, x, y, static_cast<eDirection>(direction), animFrame);
			}

			mActors.insert({ characterId, otherPlayer });
		}
	}
	void FieldScene::onCharacterDespawn(utility::Message& msg)
	{
		PacketType::CHARACTER_DESPAWN;
		__int64 characterId;
		msg >> characterId;

		auto it = mActors.find(characterId);
		if (it != mActors.end())
		{
			delete it->second;
			mActors.erase(it);
		}
	}
	void FieldScene::onCharacterMoveStart(utility::Message& msg)
	{
		//TODO : Speed도 서버가 보내줘야할 것 같다.
		PacketType::MOVE_START;
		__int64 characterId;
		float   x;
		float   y;
		__int8  direction;
		float  speed;
		msg >> characterId >> x >> y >> direction >> speed;
		actors::Actor& actor = *mActors.find(characterId)->second;
		actor.MoveStart(direction, x, y, speed);
	}
	void FieldScene::onCharacterMoveStop(utility::Message& msg)
	{
		PacketType::MOVE_STOP;
		__int64 characterId;
		float   x;
		float   y;
		__int8  direction;
		msg >> characterId >> x >> y >> direction;
		if (characterId == mLocalPlayer->GetCharacterId())
		{
			RT_ASSERT(false);
			return;
		}
		auto iter = mActors.find(characterId);
		RT_ASSERT(iter != mActors.end());
		actors::Actor& actor = *iter->second;
		actor.MoveStop(direction, x, y);
	}
	void FieldScene::onOtherCharacterAttack(utility::Message& msg)
	{
		__int64 characterId;
		msg >> characterId;

		auto iter = mActors.find(characterId);
		RT_ASSERT(iter != mActors.end());
		iter->second->Attack();
	}
	void FieldScene::onMonsterMoveStart(utility::Message& msg)
	{
		__int64 monsterId;
		float   x;
		float   y;
		__int8  direction;
		float   speed;
		msg >> monsterId >> x >> y >> direction >> speed;

		auto iter = mMonsterActors.find(monsterId);
		RT_ASSERT(iter != mMonsterActors.end());
		iter->second->MoveStart(direction, x, y, speed);
	}
	void FieldScene::onMonsterMoveStop(utility::Message& msg)
	{
		__int64 monsterId;
		float   x;
		float   y;
		__int8  direction;
		msg >> monsterId >> x >> y >> direction;

		auto iter = mMonsterActors.find(monsterId);
		RT_ASSERT(iter != mMonsterActors.end());
		iter->second->MoveStop(direction, x, y);
	}
	void FieldScene::onMonsterSpawn(utility::Message& msg)
	{
		//  Idle, 0
		//	Move, 1
		//	Chase, 2
		//	Stun, 3
		//	Dead, 4
		//	Return,//  스폰지역하고 매우 멀어질경우 스폰지역으로 되돌아감.

		__int64 monsterId;
		float x, y;
		int8_t state;
		__int8 direction;
		__int32 hp;
		__int8 monsterType;
		__int32 animFrame;
		msg >> monsterId >> x >> y
			>> state
			>> direction >> hp >> monsterType >> animFrame;

		RT_ASSERT(mMonsterActors.find(monsterId) == mMonsterActors.end());
		actors::Monster* monster = new actors::Monster(monsterId, x, y, monsterType, static_cast<eDirection>(direction), hp, animFrame);
		if (state == static_cast<int8_t>(contents::eMonsterState::Move)
			|| state == static_cast<int8_t>(contents::eMonsterState::Chase)
			|| state == static_cast<int8_t>(contents::eMonsterState::Return))
		{
			monster->MoveStart(direction, x, y, monster->GetSpeed());
		}
		else if (state == static_cast<int8_t>(contents::eMonsterState::Stun))
		{
			monster->OnDamaged(hp,x,y, static_cast<eDirection>(direction));
		}
		else if (state == static_cast<int8_t>(contents::eMonsterState::Attack))
		{
			monster->Attack(animFrame);
		}
		mMonsterActors.insert({ monsterId, monster });
	}
	void FieldScene::onMonsterSpawnBatch(utility::Message& msg)
	{
		__int16 cnt;
		msg >> cnt;

		for (__int16 i = 0; i < cnt; ++i)
		{
			__int64 monsterId;
			float x, y;
			int8_t state;
			__int8 direction;
			__int32 hp;
			__int8 monsterType;
			__int32 animFrame;

			msg >> monsterId >> x >> y
				>> state
				>> direction >> hp >> monsterType >> animFrame;

			RT_ASSERT(mMonsterActors.find(monsterId) == mMonsterActors.end());
			actors::Monster* monster = new actors::Monster(monsterId, x, y, monsterType, static_cast<eDirection>(direction), hp, animFrame);
			if (state == static_cast<int8_t>(contents::eMonsterState::Move)
				|| state == static_cast<int8_t>(contents::eMonsterState::Chase)
				|| state == static_cast<int8_t>(contents::eMonsterState::Return))
			{
				monster->MoveStart(direction, x, y,monster->GetSpeed());
			}
			else if (state == static_cast<int8_t>(contents::eMonsterState::Stun))
			{
				monster->OnDamaged(hp, x, y, static_cast<eDirection>(direction));
			}
			else if (state == static_cast<int8_t>(contents::eMonsterState::Attack))
			{
				monster->Attack(animFrame);
			}
			mMonsterActors.insert({ monsterId, monster });
		}
	}
	void FieldScene::onMonsterDamaged(utility::Message& msg)
	{

		__int64 monsterId;
		__int32 hp;
		float x, y;
		__int8 direction;
		msg >> monsterId >> hp >> x >> y >> direction;

		auto iter = mMonsterActors.find(monsterId);
		if (iter == mMonsterActors.end())
		{
			RT_ASSERT(false);
		}
		static_cast<actors::Monster*>(iter->second)->OnDamaged(hp, x, y, static_cast<eDirection>(direction));
		
	}
	void FieldScene::onMonsterAttack(utility::Message& msg)
	{
		__int64 monsterId;
		msg >> monsterId;

		auto iter = mMonsterActors.find(monsterId);
		RT_ASSERT(iter != mMonsterActors.end());
		iter->second->Attack();
	}
	void FieldScene::onCharacterDamaged(utility::Message& msg)
	{
		__int64 characterId;
		__int32 hp;
		float x, y;
		__int8 direction;
		__int64 monsterId;   // 누구한테 맞았는지 — 실제 클라는 유저가 직접 조작하므로 미사용, 프레이밍 유지 위해 읽기만 함
		msg >> characterId >> hp >> x >> y >> direction >> monsterId;

		if (characterId == mLocalPlayer->GetCharacterId())
		{
			static_cast<actors::Player*>(mLocalPlayer)->OnDamaged(hp, x, y, static_cast<eDirection>(direction));
			return;
		}

		auto iter = mActors.find(characterId);
		if (iter == mActors.end())
		{
			return;
		}
		static_cast<actors::RemotePlayer*>(iter->second)->OnDamaged(hp, x, y, static_cast<eDirection>(direction));
	}
	void FieldScene::onItemSpawn(utility::Message& msg)
	{
		__int64 itemUniqueId;
		__int8  itemId;
		float   x, y;
		msg >> itemUniqueId >> itemId >> x >> y;

		RT_ASSERT(mItemActors.find(itemUniqueId) == mItemActors.end());
		actors::FieldItemActor* item = new actors::FieldItemActor(itemUniqueId, x, y, static_cast<eItemId>(itemId));
		mItemActors.insert({ itemUniqueId, item });
	}
	void FieldScene::onItemSpawnBatch(utility::Message& msg)
	{
		__int16 cnt;
		msg >> cnt;

		for (__int16 i = 0; i < cnt; ++i)
		{
			__int64 itemUniqueId;
			__int8  itemId;
			float   x, y;
			msg >> itemUniqueId >> itemId >> x >> y;

			RT_ASSERT(mItemActors.find(itemUniqueId) == mItemActors.end());
			actors::FieldItemActor* item = new actors::FieldItemActor(itemUniqueId, x, y, static_cast<eItemId>(itemId));
			mItemActors.insert({ itemUniqueId, item });
		}
	}
	void FieldScene::onItemDespawn(utility::Message& msg)
	{
		__int64 itemUniqueId;
		msg >> itemUniqueId;

		auto it = mItemActors.find(itemUniqueId);
		if (it != mItemActors.end())
		{
			delete it->second;
			mItemActors.erase(it);
		}
	}
	void FieldScene::onLootRes(utility::Message& msg)
	{
		__int8  result;
		__int8  itemId;
		__int32 count;
		msg >> result >> itemId >> count;

		if (result == 0)
		{
			std::cout << "[Loot] 성공 - itemId:" << static_cast<int>(itemId) << " count:" << count << "\n";
		}
		else
		{
			std::cout << "[Loot] 실패\n";
		}
	}
	void FieldScene::onRankingRes(utility::Message& msg)
	{
		__int16 topCnt;
		msg >> topCnt;

		std::cout << "===== 랭킹 TOP " << topCnt << " =====\n";
		for (int i = 0; i < topCnt; ++i)
		{
			char nickname[20];
			__int64 killCount;
			msg.GetData(nickname, sizeof(nickname));
			msg >> killCount;
			std::cout << (i + 1) << "위  " << nickname << "  " << killCount << "kill\n";
		}

		__int64 myRank;
		__int64 myKillCount;
		msg >> myRank >> myKillCount;

		if (myRank == -1)
		{
			std::cout << "내 순위: 아직 킬 기록 없음\n";
		}
		else
		{
			std::cout << "내 순위: " << myRank << "위  (" << myKillCount << "kill)\n";
		}
	}
	void FieldScene::onMonsterDespawn(utility::Message& msg)
	{
		__int64 monsterId;
		msg >> monsterId;

		auto it = mMonsterActors.find(monsterId);
		if (it != mMonsterActors.end())
		{
			delete it->second;
			mMonsterActors.erase(it);
		}
	}
	void FieldScene::updateAttackInput()
	{
		bool keyDown = (GetForegroundWindow() == Game::GetInstance().GetWindows()) && (GetAsyncKeyState(VK_LBUTTON) & 0x8000);

		if (keyDown && !mbAttackKeyDown)
		{
			if (static_cast<actors::Player*>(mLocalPlayer)->TryAttack())
			{
				constexpr __int8 skillId = 0;
				float rangeLen = 0.f;
				__int16 maxTargetCnt = 0;

				if (getSkillConfig(skillId, rangeLen, maxTargetCnt))
				{
					float playerX = mLocalPlayer->GetX();
					float playerY = mLocalPlayer->GetY();

					std::vector<__int64> candidates;
					for (auto& element : mMonsterActors)
					{
						actors::Monster* monster = static_cast<actors::Monster*>(element.second);
						if (!monster->IsAlive())
						{
							continue;
						}
						if (isInAttackRange(playerX, playerY, monster->GetX(), monster->GetY(), rangeLen))
						{
							candidates.push_back(monster->GetCharacterId());
							if (static_cast<__int16>(candidates.size()) >= maxTargetCnt)
							{
								break;
							}
						}
					}

					Header header{ 0 };
					header.Len = static_cast<int16_t>(sizeof(__int16) + sizeof(skillId) + sizeof(__int16) + sizeof(__int64) * static_cast<int>(candidates.size()));
					header.RandKey = 0;

					utility::Message msg;
					msg.InitMessage(0, 0);
					msg.PutData(&header, sizeof(header));
					msg << static_cast<__int16>(PacketType::PLAYER_ATTACK_REQ);
					msg << skillId;
					msg << static_cast<__int16>(candidates.size());
					for (__int64 id : candidates)
					{
						msg << id;
					}
					RT_ASSERT(g_Network.Send(msg) == true);
				}
			}
		}
		mbAttackKeyDown = keyDown;
	}
	void FieldScene::updateLootInput()
	{
		bool keyDown = (GetForegroundWindow() == Game::GetInstance().GetWindows()) && (GetAsyncKeyState('F') & 0x8000);

		if (keyDown && !mbLootKeyDown)
		{
			float playerX = mLocalPlayer->GetX();
			float playerY = mLocalPlayer->GetY();

			__int64 nearestId = 0;
			float nearestDist = LOOT_RANGE;
			bool bFound = false;

			for (auto& element : mItemActors)
			{
				float dx = element.second->GetX() - playerX;
				float dy = element.second->GetY() - playerY;
				float dist = sqrtf(dx * dx + dy * dy);
				if (dist <= nearestDist)
				{
					nearestDist = dist;
					nearestId = element.first;
					bFound = true;
				}
			}

			if (bFound)
			{
				Header header{ 0 };
				header.Len = static_cast<int16_t>(sizeof(__int16) + sizeof(__int64));
				header.RandKey = 0;

				utility::Message msg;
				msg.InitMessage(0, 0);
				msg.PutData(&header, sizeof(header));
				msg << static_cast<__int16>(PacketType::LOOT_REQ);
				msg << nearestId;
				RT_ASSERT(g_Network.Send(msg) == true);
			}
		}
		mbLootKeyDown = keyDown;
	}
	void FieldScene::updateRankingInput()
	{
		bool keyDown = (GetForegroundWindow() == Game::GetInstance().GetWindows()) && (GetAsyncKeyState('R') & 0x8000);

		if (keyDown && !mbRankingKeyDown)
		{
			Header header{ 0 };
			header.Len = static_cast<int16_t>(sizeof(__int16));
			header.RandKey = 0;

			utility::Message msg;
			msg.InitMessage(0, 0);
			msg.PutData(&header, sizeof(header));
			msg << static_cast<__int16>(PacketType::RANKING_REQ);
			RT_ASSERT(g_Network.Send(msg) == true);
		}
		mbRankingKeyDown = keyDown;
	}
};
