#pragma comment(lib,"winmm.lib")

#include "FieldServer.h"
#include <timeapi.h>
#include "../_Shared/Protocol.h"
#include "../_Shared/AttackConfig.h"
#include "PacketStats.h"
#include "../_lib/CDB/framework.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>

#include "../_lib/MTProfiler_Lib/MTProfiler_Lib.h"
extern thread_local CProfileManager manager;
namespace contents
{
	thread_local std::unordered_map<int64_t, Player*> players;
	thread_local std::unordered_set<Player*> sectors[SECTOR_ROW_CNT][SECTOR_COL_CNT];
	thread_local std::unordered_map<int64_t, Monster*> monsters;
	thread_local std::unordered_set<Monster*> monsterSectors[SECTOR_ROW_CNT][SECTOR_COL_CNT];
	thread_local std::unordered_map<int64_t, FieldItem*> items;
	thread_local std::unordered_set<FieldItem*> itemSectors[SECTOR_ROW_CNT][SECTOR_COL_CNT];

	FieldServer::FieldServer()
		: mbOn(true)
		, mCharacterID(0)
		, mItemUniqueID(0)
		, bMonitorOn(false)
		, mDisconnect_Sync(0)
		, mFrameTime(0)
		, mAuthMessageQCnt(0)
		, mFieldMessageQCnt(0)
		, mDBMessageQCnt(0)
		, mPlayerMessageQCnt(0)
		, mMessageDeQTPS(0)
		, mMessageInQTPS(0)
		, mProcessDelaySum(0)
		, mDisconnect_SendQisFull(0)
	{
		hAuthEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

		for (int i = 0; i < CONFIG_FIELD_SIZE; ++i)
		{
			mMsgQ[i] = new utility::MyRingBuffer();
			mNotifyMsgQ[i] = new utility::MyRingBuffer();
			mDBReqQ[i] = new utility::MyRingBuffer();
			mDBResQ[i] = new utility::MyRingBuffer();
			hDBEvent[i] = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			hDBFinishEvent[i] = CreateEvent(nullptr, true, FALSE, nullptr);
		}
		nearSectorInitalization();
	}
	FieldServer::~FieldServer()
	{
		stop();

		mbOn = false;

		for (int i = 0; i < CONFIG_FIELD_SIZE; ++i)
		{
			// fieldThread가 먼저 완전히 끝나야 함 (종료 직전 마지막으로 mDBReqQ에 넣는 job까지 다 넣고 끝남)
			if (mFieldThread[i].joinable())
			{
				mFieldThread[i].join();
			}

			// fieldThread가 끝난 뒤에야 dbThread를 깨워서 남은 mDBReqQ를 다 비우게 하고 종료
			SetEvent(hDBEvent[i]);
			if (mDBThread[i].joinable())
			{
				mDBThread[i].join();
			}
			CloseHandle(hDBEvent[i]);
			CloseHandle(hDBFinishEvent[i]);
		}

		SetEvent(hAuthEvent);
		if (mAuthThread.joinable())
		{
			mAuthThread.join();
		}	

		for (int i = 0; i < CONFIG_FIELD_SIZE; ++i)
		{
			delete mMsgQ[i];
			delete mNotifyMsgQ[i];
			delete mDBReqQ[i];
			delete mDBResQ[i];
		}
		bMonitorOn = false;
		if (mMonitorThread.joinable())
		{
			mMonitorThread.join();
		}

		CloseHandle(hAuthEvent);
		
	}
	void FieldServer::Start()
	{
		createThread();
		NetworkLib::start();
	}
	void FieldServer::createThread()
	{
		bMonitorOn = true;
		mMonitorThread = std::thread(&FieldServer::monitorThread, this);
		for (int idx = 0; idx < CONFIG_FIELD_SIZE; ++idx)
		{
			mFieldThread[idx] = std::thread(&FieldServer::fieldThread, this, idx);
			std::wstring threadName = L"FieldThread" + std::to_wstring(idx);
			SetThreadDescription(mFieldThread[idx].native_handle(), threadName.c_str());

			// 이름은 밖에서 안 걸고 dbThread 안에서 직접 건다 (CDB 생성자가 GetCurrentThread()로
			// 이름을 읽는데, 밖에서 걸면 새 스레드가 그보다 먼저 CDB를 생성해버리는 레이스가 있음)
			mDBThread[idx] = std::thread(&FieldServer::dbThread, this, idx);
		}
		// dbThread와 같은 이유로 이름은 authThread 안에서 직접 건다 (CDB db; 를 이제 authThread도 만듦)
		mAuthThread = std::thread(&FieldServer::authThread, this);
	}
	void FieldServer::nearSectorInitalization()
	{

		for (int8_t row = 0; row < SECTOR_COL_CNT; ++row)
		{
			for (int8_t col = 0; col < SECTOR_COL_CNT; ++col)
			{
				const map::Sector center{ col,row };
				std::vector<map::Sector> around;
				around.reserve(9);
				{
					int8_t dx[] = { -1,-1,-1,0,0,0, +1,+1,+1 };
					int8_t dy[] = { -1,0,+1,-1,0,+1,-1,+0,+1 };
					for (int i = 0; i < 9; ++i)
					{
						int8_t col = center.x + dx[i];
						int8_t row = center.y + dy[i];
						if (row < 0 || col < 0 || row >= SECTOR_ROW_CNT || col >= SECTOR_COL_CNT)
						{
							continue;
						}
						around.emplace_back(col, row);
					}
				}
				MY_ASSERT(mAroundSectorCache.find(center) == mAroundSectorCache.end(), "중복된 결과 검사");
				mAroundSectorCache.insert({ center,around });

			}

		}
	}
	void FieldServer::onAccept(SOCKADDR_IN& addr, const network::SeqAndIdx& sessionID)
	{
		// 최대치를 초과 시 끊음.
		size_t unAuthSessionCnt = 0;
		{
			std::lock_guard<std::shared_mutex> xlock(mUnAuthLock);
			unAuthSessionCnt = mUnAuthSessions.size();
		}
		if (unAuthSessionCnt >= CONFIG_UNAUTH_MAX_SIZE)
		{
			//TODO : 로깅
			// 연결을 이용한 메모리 공격 방지
			disconnectSession(sessionID);
			return;
		}

		Player* player = new Player(addr, sessionID);
		player->mLastTime = timeGetTime();
		network::Session* const session = FindSessionOrNull(sessionID);
		session->SetmPtr(player);
		{
			std::lock_guard<std::shared_mutex> xlock(mUnAuthLock);
			MY_ASSERT(mUnAuthSessions.find(sessionID.Value) == mUnAuthSessions.end(), "세션아이디Key값이 이미존재한다");
			mUnAuthSessions.insert({ sessionID.Value,player });
			++mPlayerCnt;
		}
		SetEvent(hAuthEvent);
	}
	void FieldServer::onRecv(utility::Message* msg)
	{
		network::SeqAndIdx sessionID{ 0 };
		sessionID.Value = msg->GetOwnerID();
		network::Session* const session = FindSessionOrNull(sessionID);
		{
			Player* player = static_cast<Player*>(session->GetmPtr());
			MY_ASSERT(player != nullptr, "Player 생성자 실패");
			player->EnQueueMsg(msg);

			_InterlockedIncrement64((int64_t*)&mMessageInQTPS);
			_InterlockedIncrement64(&mPlayerMessageQCnt);
			auto playerMsgQCnt = _InterlockedIncrement64(&player->mSendMsgCnt);
			if (CONFIG_SESSION_SENDQ_CAPPACITY <= playerMsgQCnt)
			{
				//std::cout << std::setw(15) << "session_SendQSize : " << std::setw(5) << playerMsgQCnt << " 주변에 세션이 너무 많음.";
				disconnectSession(sessionID);
				_InterlockedIncrement64(&mDisconnect_SendQisFull);

				//RT_ASSERT(false, "주변에 세션이 너무 많으면 끊기.");
			}

			player->mLastTime = timeGetTime();
		}
	}
	void FieldServer::onRelease(const network::SeqAndIdx& sessionID)
	{
		// Thread가 스스로 Player를 제거하도록 blive플래그를 꺼줌
		network::Session* const session = FindSessionOrNull(sessionID);
		{
			Player* player = static_cast<Player*>(session->GetmPtr());
			// 시나리오 : acceptEx가 걸려있는 서버가 session을 할당해제 함.
			if (player != nullptr)
			{
				player->bConnect = false;
			}
		}
	}

	void FieldServer::authThread()
	{
		// CDB 생성자가 GetCurrentThread()로 이름을 읽으므로, CDB db; 보다 반드시 먼저 설정
		SetThreadDescription(GetCurrentThread(), L"AuthThread");

		CDB db;
		if (!db.Connect("localhost", "root", "123123", "stoneage", 3306))
		{
			db.ClearError();
			MY_ASSERT(false, "authThread: DB Connect 실패");
		}

		while (true)
		{
			// 서버가 종료절차를 하고있고, player가 없다면 나가기
			if (mbOn == false && mAccountsHash.size() == 0)
			{
				break;
			}
			WaitForSingleObject(hAuthEvent, 500);
			// mUnAuthSessions에서 새 세션 players에 등록
			{
				std::lock_guard<std::shared_mutex> rlock(mUnAuthLock);
				for (auto& pair : mUnAuthSessions)
				{
					if (players.find(pair.first) == players.end())
					{
						players.insert({ pair.first, pair.second });
					}
				}
			}
			for (auto& pair : players)
			{
				Player& player = *pair.second;
                // auth를 송신 후 Loop를 나가기전에 컨텐츠 메세지가 오면 msg 유실
                if (player.mNextFieldID != CONFIG_AUTH_FIELD_IDX)
                {
                    continue;
                }
				while (utility::Message* msg = player.DeQueueMsgOrNull())
				{
					authPacketProc(*msg, db);
					_InterlockedDecrement64(&mPlayerMessageQCnt);
					_InterlockedDecrement64(&player.mSendMsgCnt);
					MY_DELETE msg;
					// auth를 송신 후 Loop를 나가기전에 컨텐츠 메세지가 오면 msg 유실
					if (player.mNextFieldID != CONFIG_AUTH_FIELD_IDX)
					{
						break;
					}
				}
			}

			// Field →Auth 알림 처리 (추가)
			for (int idx = 0; idx < CONFIG_FIELD_SIZE; ++idx)
			{
				char* f = mNotifyMsgQ[idx]->GetFrontPtr();
				char* r = mNotifyMsgQ[idx]->GetRearPtr();
				int32_t useSize = mNotifyMsgQ[idx]->GetUseSize(f, r);
				while (useSize >= sizeof(size_t))
				{
					utility::Message* msg = nullptr;
					{
						mNotifyMsgQ[idx]->Dequeue(&msg, sizeof(utility::Message*));
					}
					authNotifyPacketProc(*msg);
					_InterlockedDecrement64(&mAuthMessageQCnt);
					MY_DELETE msg;
					useSize -= sizeof(size_t);
				}
			}
			checkDisConnectedAndLeavePlayer(CONFIG_AUTH_FIELD_IDX);
			//TODO: 기본실험 끝나면 하트비트 켜기
			heartBeatForUnAuthSession(CONFIG_AUTH_FIELD_IDX);
		}
	}
	void FieldServer::heartBeatForUnAuthSession(int fieldIdx)
	{
		// TODO: 클라이언트가 하트비트 쏠떄까지 비활성화
		if (fieldIdx != CONFIG_AUTH_FIELD_IDX)
		{
			return;
		}
		DWORD currentTime = timeGetTime();
		for (auto& pair : players)
		{
			Player& player = *pair.second;
			if (player.bConnect)
			{
				// CPU가 달라서 존재하는 경우
				if (currentTime <= player.mLastTime)
				{
					continue;
				}
				if (currentTime - player.mLastTime >= CONFIG_UNAUTH_HEARTBEAT_TIMER)
				{
					disconnectSession(player.mSeqID);
					InterlockedIncrement64(&mDisconnect_HeartBeat);
					MY_ASSERT(FALSE, "Auth에 players가 존재 할 수 가 없음");
				}
			}
		}
	}
	namespace
	{
		// SQL 문자열 리터럴을 못 빠져나가게 ' 와 \ 앞에 \ 를 붙인다.
		// outputSize는 output 버퍼 전체 크기(널 종단 포함).
		void escapeSqlString(const char* input, char* output, size_t outputSize)
		{
			size_t outIdx = 0;
			for (size_t i = 0; input[i] != '\0' && outIdx + 2 < outputSize; ++i)
			{
				if (input[i] == '\'' || input[i] == '\\')
				{
					output[outIdx++] = '\\';
				}
				output[outIdx++] = input[i];
			}
			output[outIdx] = '\0';
		}

		// id는 영문/숫자만 허용 (인젝션 방어 + 빈 문자열 거부)
		bool isValidId(const char* id)
		{
			if (id[0] == '\0')
			{
				return false;
			}
			for (int i = 0; id[i] != '\0'; ++i)
			{
				if (!isalnum(static_cast<unsigned char>(id[i])))
				{
					return false;
				}
			}
			return true;
		}
	}

	void FieldServer::sendAuthFail(const network::SeqAndIdx& sessionID)
	{
		utility::Message* res = MY_NEW utility::Message();
		Header header{ 0 };
		header.Len = sizeof(int16_t) + sizeof(int8_t);
		header.RandKey = 0;
		res->InitMessage(sessionID.Value, 0);

		res->PutData(&header, sizeof(header));

		*res << (int16_t)PacketType::FIELD_AUTH_FAIL;
		*res << (int8_t)0;

		sendPost(sessionID, *res);
		stats::RecordSend(PacketType::FIELD_AUTH_FAIL);
	}

	void FieldServer::handleAuthReq(utility::Message& msg, CDB& db)
	{
		char id[20] = {}; // 널문자 포함 20바이트 — Player::mNickname(char[20])과 크기 일치, id를 그대로 닉네임으로 재사용
		char pw[65] = {};
		msg.GetData(id, 20);
		id[19] = '\0'; // 조작된 클라가 20바이트를 전부 채워 보내도 강제 종료 (OOB read 방지)
		msg.GetData(pw, 64);

		network::SeqAndIdx sessionID{ 0 };
		sessionID.Value = msg.GetOwnerID();

		if (!isValidId(id))
		{
			sendAuthFail(sessionID);
			return;
		}

		char escapedPw[129] = {};
		escapeSqlString(pw, escapedPw, sizeof(escapedPw));

		int64_t accountNo = 0;
		float PosX = 0.f;
		float PosY = 0.f;

		char query[384];
		snprintf(query, sizeof(query),
			"SELECT accountNo, x, y FROM Accounts WHERE id='%s' AND pw='%s'", id, escapedPw);

		stResultSet rs;
		bool bFound = false;
		if (db.Query(query, rs))
		{
			bFound = rs.Fetch();
			if (bFound)
			{
				accountNo = atoll(rs.GetValue("accountNo"));
				PosX = static_cast<float>(atof(rs.GetValue("x")));
				PosY = static_cast<float>(atof(rs.GetValue("y")));
			}
		}
		else
		{
			db.ClearError();
		}

		if (!bFound)
		{
			// 계정 없음 → 자동 가입 (별도 회원가입 플로우 없음, D-8 일정상 간소화)
			char insertQuery[384];
			snprintf(insertQuery, sizeof(insertQuery),
				"INSERT INTO Accounts (id, pw, x, y) VALUES ('%s', '%s', 0, 0)", id, escapedPw);

			if (!db.Execute(insertQuery))
			{
				// id UNIQUE 충돌(동시 가입) 등 — 인증 실패 처리
				db.ClearError();
				sendAuthFail(sessionID);
				return;
			}

			stResultSet rs2;
			char selectQuery[128];
			snprintf(selectQuery, sizeof(selectQuery), "SELECT accountNo FROM Accounts WHERE id='%s'", id);
			if (!db.Query(selectQuery, rs2) || !rs2.Fetch())
			{
				db.ClearError();
				sendAuthFail(sessionID);
				return;
			}
			accountNo = atoll(rs2.GetValue("accountNo"));
			PosX = 0.f;
			PosY = 0.f;
		}

		// 중복 로그인 체크 (O(1))
		auto dupIt = mAccountsHash.find(accountNo);
		if (dupIt != mAccountsHash.end())
		{
			disconnectSession(dupIt->second->mSeqID);
			sendAuthFail(sessionID);
			return;
		}

		//TODO : DB 에서 맵 정하기
		int8_t fieldIdx = 0;
		// UnAuth →Auth 승격
		Player* player = nullptr;
		{
			std::lock_guard<std::shared_mutex> ulock(mUnAuthLock);
			auto it = mUnAuthSessions.find(sessionID.Value);
			if (it == mUnAuthSessions.end())
			{
				return;
			}
			player = it->second;
			player->SetPosition(PosX, PosY);
			mUnAuthSessions.erase(it);
		}
		int64_t characterId = InterlockedIncrement64(&mCharacterID);
		player->InitPlayerInfo(accountNo, characterId, fieldIdx);
		memcpy(player->mNickname, id, sizeof(player->mNickname)); // id를 그대로 닉네임으로 사용 (둘 다 20바이트라 truncation 없음)

		// 인증 성공 →accountNo 등록
		mAccountsHash.insert({ accountNo, player });

		// 성공 응답
		utility::Message* res = MY_NEW utility::Message();
		res->InitMessage(sessionID.Value, 0);
		Header header{ 0 };
		header.Len = sizeof(int16_t) + sizeof(characterId) + sizeof(PosX) + sizeof(PosY);
		header.RandKey = 0;
		res->PutData(&header, sizeof(header));

		*res << (int16_t)PacketType::FIELD_AUTH_RES;
		*res << characterId;
		*res << PosX;
		*res << PosY;
		sendPost(sessionID, *res);
		stats::RecordSend(PacketType::FIELD_AUTH_RES);
	}
	void FieldServer::authPacketProc(utility::Message& msg, CDB& db)
	{
		int16_t type;
		msg >> type;
		stats::RecordRecv((PacketType)type);

		switch ((PacketType)type)
		{
		case PacketType::FIELD_AUTH_REQ:
			handleAuthReq(msg, db);
			break;
		default:
			MY_ASSERT(FALSE, "현재 테스트에서는 일어날 수 없음.클라이언트의 조작된 메세지");
		}
	}
	void FieldServer::enterField(const Player* player, int8_t targetField)
	{
		MY_ASSERT(targetField < CONFIG_FIELD_SIZE, "이거 틀리면 자세히 볼 것.");
		auto it = players.find(player->mSeqID.Value);
		MY_ASSERT(it != players.end(), "현재 스레드가 player를 보유하고있지 않은상황");
		players.erase(it);

		utility::Message* newMsg = MY_NEW utility::Message();
		newMsg->InitMessage(player->mSeqID.Value, 0);
		*newMsg << (int16_t)PacketType::REGISTER_PLAYER;
		newMsg->PutData(&player, sizeof(Player*));
	THREADQ_FULL:
		{
			char* f = mMsgQ[targetField]->GetFrontPtr();
			char* r = mMsgQ[targetField]->GetRearPtr();
			int32_t freeSize = mMsgQ[targetField]->GetFreeSize(f, r);
			if (freeSize < sizeof(size_t))
			{
				goto THREADQ_FULL;
			}
			mMsgQ[targetField]->Enqueue(&newMsg, sizeof(utility::Message*));
		}
		_interlockedincrement64(&mFieldMessageQCnt[targetField]);
	}

	void FieldServer::spawnMonsters(int fieldIdx)
	{
		srand(5);
		int64_t monsterId = 0;
		for (int8_t row = 0; row < SECTOR_ROW_CNT; ++row)
		{
			for (int8_t col = 0; col < SECTOR_COL_CNT; ++col)
			{
				int cnt = rand() % 2; // 섹터당 0~1마리

				for (int i = 0; i < cnt; ++i)
				{
					map::Position pos{
						static_cast<float>(col * SECTOR_WORLD_W + rand() % SECTOR_WORLD_W),
						static_cast<float>(row * SECTOR_WORLD_H + rand() % SECTOR_WORLD_H)
					};
					Monster* monster = MY_NEW Monster(monsterId++, pos, 0);

					map::Sector sector;
					calcSector(pos, sector);
					monster->mSector = sector;
					monster->mDestSector = sector;

					monsters.insert({ monster->mMonsterID, monster });
					monsterSectors[sector.y][sector.x].insert(monster);
				}
			}
		}
	}
	bool FieldServer::possibleChasePlayer(int64_t playerID, map::Position& targetPos)
	{
		auto targetIter = players.find(playerID);
		if (targetIter == players.end())
		{
			return false;
		}
		else if (targetIter->second->IsDead())
		{
			return false;
		}
		targetPos = targetIter->second->mPos;
		return true;
	}
	void FieldServer::processMonsterAttackHit(Monster& monster)
	{
		std::vector<map::Sector> nearSector;
		getAroundSectors(monster.mSector, nearSector);

		Player* closestPlayer = nullptr;
		float closestDist = MONSTER_ATTACK_RANGE;

		for (const map::Sector& sector : nearSector)
		{
			for (Player* player : sectors[sector.y][sector.x])
			{
				if (player->IsDead())
				{
					continue;
				}
				float dist = monster.mPos - player->mPos;
				if (dist <= closestDist)
				{
					closestDist = dist;
					closestPlayer = player;
				}
			}
		}

		if (closestPlayer == nullptr)
		{
			return;   // 사거리 안에 아무도 없음(전부 이탈했거나 죽어있음) — 헛스윙
		}

		closestPlayer->takeDamage(MONSTER_ATTACK_DAMAGE, &monster);

		std::vector<map::Sector> targetNearSector;
		getAroundSectors(closestPlayer->mCurrentSector, targetNearSector);
		broadcastCharacterDamaged(*closestPlayer, monster, targetNearSector);
	}
	void FieldServer::fieldThread(int fieldIdx)
	{
		CProfileRegistry::GetInstance().RegistProfiler(&manager);
		spawnMonsters(fieldIdx);
		DWORD startTime = timeGetTime();
		DWORD beforestartTime = startTime;
		DWORD nextTime = startTime + CONFIG_FRAME_INTERVAL;
		DWORD secondTime = startTime + 1000;
		int32_t FrameCnt = 0;

		while (true)
		{
			++FrameCnt;
			beforestartTime = startTime;
			// Thread 메세지 처리
			{
				char* f = mMsgQ[fieldIdx]->GetFrontPtr();
				char* r = mMsgQ[fieldIdx]->GetRearPtr();


				Profile profile(L"Field_Proc");
				int32_t useSize = mMsgQ[fieldIdx]->GetUseSize(f, r);
				// 서버 종료를 하더라도  메세지를 전부 비우고하자.
				if (useSize == 0)
				{
					if (players.size() == 0 && mbOn == false)
					{
						SetEvent(hDBFinishEvent[fieldIdx]);
						break;
					}
				}
				while (useSize >= sizeof(size_t))
				{
					utility::Message* msg = nullptr;
					{
						mMsgQ[fieldIdx]->Dequeue(&msg, sizeof(utility::Message*));
					}
					fieldPacketProc(fieldIdx, *msg);
					_InterlockedDecrement64(&mFieldMessageQCnt[fieldIdx]);
					MY_DELETE msg;
					useSize -= sizeof(size_t);
				}
			}
			startTime = timeGetTime();
			if (secondTime <= startTime)
			{
				//std::cout << "FrameCnt" << FrameCnt << "\n";
				secondTime += 1000;
				FrameCnt = 0;
			}

			if (nextTime <= startTime)
			{
				DWORD loopStartTime = beforestartTime;
				{
					Profile profile(L"fieldUpdate");
					fieldUpdate(fieldIdx);
				}
				{
					playerProc(fieldIdx, loopStartTime);
					monsterProc();
				}
				nextTime += CONFIG_FRAME_INTERVAL;
			}
			Profile profile(L"Loop_total");
			{
				Profile profile(L"FieldLeave");
				heartBeatForUnAuthSession(fieldIdx);
				checkDisConnectedAndLeavePlayer(fieldIdx);
				dbResultPacketProc(fieldIdx);
			}

			startTime = timeGetTime();
			if (startTime < nextTime)
			{
				Sleep(nextTime - startTime);
			}
		}
	}
	void FieldServer::fieldUpdate(int fieldIdx)
	{
		// Player들의 이동 로직.
		for (auto& element : players)
		{
			Player& player = *element.second;
			player.update();
			if (player.GetSpawnState() == ePlayerState::Attack && player.GetAnimFrame() == PLAYER_ATTACK_AWINDUP_FRAME)
			{
				processPlayerAttackHit(player, fieldIdx);
			}
			updateSectorChechk(player);
		}

		// 이동이 끝난후 Sector를 갱신.
		for (auto& element : players)
		{
			Player& player = *element.second;
			// sector가 바뀌었다면,
			if (player.mCurrentSector != player.mDestSector)
			{
				const map::Sector& oldSector = player.mCurrentSector;
				const map::Sector& newSector = player.mDestSector;

				auto iter = sectors[oldSector.y][oldSector.x].find(&player);
				MY_ASSERT(iter != sectors[oldSector.y][oldSector.x].end(), "이동전 섹터에 player이 없다.");

				std::vector<map::Sector> spawnSector;
				std::vector<map::Sector> despawnSector;

				calcDespawnAndSpawnSector(player, spawnSector, despawnSector);

				broadcastSpawn(player, spawnSector);
				sendSpawnBatch(player, spawnSector);
				sendMonsterSpawnBatch(player, spawnSector);
				sendItemSpawnBatch(player, spawnSector);

				broadcastDespawn(player, despawnSector);
				sendDespawnBatch(player, despawnSector);
				sendMonsterDespawnBatch(player, despawnSector);
				sendItemDespawnBatch(player, despawnSector);

				sectors[oldSector.y][oldSector.x].erase(iter);
				sectors[newSector.y][newSector.x].insert(&player);
				player.mCurrentSector = newSector;
			}
		}
		// Monster 이동 로직 (순찰 또는 추적).
		for (auto& element : monsters)
		{
			Monster& monster = *element.second;
			monster.mBeforeState = monster.mState;
			monster.mBeforeDirection = monster.mDirection;

			if (monster.mState == eMonsterState::Chase)
			{
				// player가 존재하는지. 필드서버만 판단가능.
				bool bChase = possibleChasePlayer(monster.mTargetPlayerID, monster.mTargetPos);

				if (!bChase)
				{
					monster.giveUpChase();
				}

			}
			monster.Update();
			if (monster.mState == eMonsterState::Attack && monster.GetAnimFrame() == MONSTER_ATTACK_WINDUP_FRAME)
			{
				processMonsterAttackHit(monster);
			}
			updateMonsterSectorCheck(monster);
		}

		// Monster Sector 갱신.
		for (auto& element : monsters)
		{
			Monster& monster = *element.second;
			// 죽어있는 상태라면 
			if (monster.mBeforeState != eMonsterState::Dead && monster.mSector != monster.mDestSector)
			{
				const map::Sector& oldSector = monster.mSector;
				const map::Sector& newSector = monster.mDestSector;

				auto iter = monsterSectors[oldSector.y][oldSector.x].find(&monster);
				MY_ASSERT(iter != monsterSectors[oldSector.y][oldSector.x].end(), "이동전 섹터에 monster가 없다.");

				std::vector<map::Sector> spawnSector;
				std::vector<map::Sector> despawnSector;
				calcMonsterDespawnAndSpawnSector(monster, spawnSector, despawnSector);

				broadcastMonsterSpawn(monster, spawnSector);
				broadcastMonsterDespawn(monster, despawnSector);

				monsterSectors[oldSector.y][oldSector.x].erase(iter);
				monsterSectors[newSector.y][newSector.x].insert(&monster);
				monster.mSector = newSector;
			}
		}

		// 아이템 생존시간 처리 (만료된 아이템 제거)
		for (auto iter = items.begin(); iter != items.end();)
		{
			FieldItem& item = *iter->second;
			item.Update();
			if (item.IsExpired())
			{
				std::vector<map::Sector> nearSector;
				getAroundSectors(item.mSector, nearSector);
				broadcastItemDespawn(item, nearSector);

				itemSectors[item.mSector.y][item.mSector.x].erase(&item);
				MY_DELETE& item;
				iter = items.erase(iter);
			}
			else
			{
				++iter;
			}
		}
	}

	void FieldServer::playerProc(int fieldidx, DWORD startTime)
	{
		// player 메세지 처리
		int32_t playerMsgCnt = 0;
		Profile profile(L"Players_Proc");
		for (auto& element : players)
		{
			Player& player = *element.second;
			while (utility::Message* msg = player.DeQueueMsgOrNull())
			{
				DWORD delay = timeGetTime() - msg->GetRecvTick();
				mProcessDelaySum += delay;
				fieldContentsPacketProc(*msg);

				++mMessageDeQTPS;
				_InterlockedDecrement64(&mPlayerMessageQCnt);
				_InterlockedDecrement64(&player.mSendMsgCnt);
				player.mLastTime = startTime;
				++playerMsgCnt;
				MY_DELETE msg;
				if (player.mNextFieldID != fieldidx)
				{
					break;
				}
			}
			if (mbOn == false)
			{
				player.bConnect = false;
			}

		}
		//std::cout << "\tPlayerMsgCnt :" << std::setw(5) << playerMsgCnt
		//	<< std::setw(15) << "\tFrameTime : " << std::setw(8) << timeGetTime()- startTime << "ms \n\n";
	}
	void FieldServer::monsterProc()
	{
		for (auto& element : monsters)
		{
			Monster& monster = *element.second;
			// 움직이는 방향이 바뀌거나 애니메이션이 바뀜.
			if (monster.mState == monster.mBeforeState && monster.mDirection == monster.mBeforeDirection)
			{
				continue;
			}
			switch (monster.mState)
			{
				// 움직이는 로직.
			case eMonsterState::Move:
			case eMonsterState::Chase:
			case eMonsterState::Return:
			{
				std::vector<map::Sector> nearSector;
				getAroundSectors(monster.mSector, nearSector);

				bool wasMoving = (monster.mBeforeState == eMonsterState::Move
					|| monster.mBeforeState == eMonsterState::Chase
					|| monster.mBeforeState == eMonsterState::Return);
				// 상태 전이 직전에도 이미 움직이는 상태였다면(클라가 이미 mbMove==true) 반드시 Stop 먼저 보내기
				if (wasMoving)
				{
					broadcastMonsterMoveStop(monster, nearSector);
					broadcastMonsterMoveStart(monster, nearSector);
				}
				//그게 아니면(Idle/Stun/Dead에서 넘어옴) Start만.
				else
				{
					broadcastMonsterMoveStart(monster, nearSector);
				}
				break;
			}
			case eMonsterState::Attack:
			{
				std::vector<map::Sector> nearSector;
				getAroundSectors(monster.mSector, nearSector);
				broadcastMonsterMoveStop(monster, nearSector);
				broadcastMonsterAttack(monster, nearSector);
				break;
			}
			case eMonsterState::Idle:
			{
				// WHY : 죽은 상태에서 idle로 되돌아간가면 같은섹터에게도 갱신이 필요함.
				if (monster.mBeforeState == eMonsterState::Dead)
				{
					std::vector<map::Sector> oldNearSector;
					getAroundSectors(monster.mSector, oldNearSector);
					broadcastMonsterDespawn(monster, oldNearSector);

					if (monster.mSector != monster.mDestSector)
					{
						auto iter = monsterSectors[monster.mSector.y][monster.mSector.x].find(&monster);
						MY_ASSERT(iter != monsterSectors[monster.mSector.y][monster.mSector.x].end(), "부활 전 섹터에 monster가 없다.");
						monsterSectors[monster.mSector.y][monster.mSector.x].erase(iter);
						monsterSectors[monster.mDestSector.y][monster.mDestSector.x].insert(&monster);
						monster.mSector = monster.mDestSector;
					}

					std::vector<map::Sector> newNearSector;
					getAroundSectors(monster.mSector, newNearSector);
					broadcastMonsterSpawn(monster, newNearSector);
				}
				else
				{
					std::vector<map::Sector> nearSector;
					getAroundSectors(monster.mSector, nearSector);
					broadcastMonsterMoveStop(monster, nearSector);
				}
				break;
			}
			default:
				break;
			}
			monster.mBeforeState = monster.mState;
			monster.mBeforeDirection = monster.mDirection;
		}
	}
	void FieldServer::fieldPacketProc(int fieldIdx, utility::Message& msg)
	{
		network::SeqAndIdx seqID{ 0 };
		seqID.Value = msg.GetOwnerID();

		int16_t type;
		msg >> type;

		switch ((PacketType)type)
		{
		case PacketType::REGISTER_PLAYER:
		{
			handleRegisterPlayer(fieldIdx, msg);
			break;
		}
		default:
			MY_ASSERT(false, "현재 테스트에서 일어나지않음. 클라이언트의 조작된 패킷");
		}
	}
	void FieldServer::handleRegisterPlayer(int fieldIdx, utility::Message& msg)
	{
		Player* enterPlayer = nullptr;
		msg.GetData(&enterPlayer, sizeof(Player*));
		MY_ASSERT(players.find(enterPlayer->mSeqID.Value) == players.end(), "같은 Player가 이미 존재하는 상황");
		players.insert({ enterPlayer->mSeqID.Value, enterPlayer });
		enterPlayer->mCurrentFieldID = fieldIdx;

		enterSector(*enterPlayer);

		std::vector<map::Sector> nearSector;
		getAroundSectors(enterPlayer->mCurrentSector, nearSector);

		// 주변 섹터들에게 Spawn Msg 보내기.
		broadcastSpawn(*enterPlayer, nearSector);
		// newPlayer에게 주변 캐릭터 Spawn 보내기
		sendSpawnBatch(*enterPlayer, nearSector);
		// newPlayer에게 주변 몬스터 Spawn 보내기
		sendMonsterSpawnBatch(*enterPlayer, nearSector);
		// newPlayer에게 주변 아이템 Spawn 보내기
		sendItemSpawnBatch(*enterPlayer, nearSector);
	}
	void FieldServer::fieldContentsPacketProc(utility::Message& msg)
	{
		network::SeqAndIdx seqID{ 0 };
		seqID.Value = msg.GetOwnerID();

		int16_t type;
		msg >> type;
		stats::RecordRecv((PacketType)type);

		switch ((PacketType)type)
		{
		case PacketType::MOVE_START:
		{
			handleMoveStart(msg, seqID.Value);
			break;
		}
		case PacketType::MOVE_STOP:
		{
			handleMoveStop(msg, seqID.Value);
			break;
		}
		case PacketType::PLAYER_ATTACK_REQ:
		{
			handleAttackReq(msg, seqID.Value);
			break;
		}
		case PacketType::LOOT_REQ:
		{
			handleLootReq(msg, seqID.Value);
			break;
		}
		case PacketType::RANKING_REQ:
		{
			handleRankingReq(msg, seqID.Value);
			break;
		}
		default:
			//TODO : 조작된 패킷 or 잘못 짬.
			disconnectSession(seqID);
		}
	}

	void FieldServer::handleMoveStart(utility::Message& msg, int64_t seqID)
	{
		PacketType::MOVE_START;
		int64_t characterId;
		float   x;
		float   y;
		int8_t  direction;
		float   speed;

		msg >> characterId >> x >> y >> direction >> speed;
		DWORD recvTime = msg.GetRecvTick();

		// WHY : 20ms 이상 차이가나서 결과가 달라지는 경우로 인한 문제인가 실측코드
		DWORD startTime = timeGetTime();
		DWORD delayTime = startTime - recvTime;

		std::unordered_map<int64_t, Player*>::iterator iter = players.find(seqID);
		MY_ASSERT(iter != players.end(), "Player가 현재 스레드가 없는 상황이므로 말이 안됨.");
		Player& player = *iter->second;

		switch (player.GetSpawnState())
		{
		case ePlayerState::Stun:
			// # 허용되는 경우임.
			// 서버에서 맞았다고 판단한 시점에. 클라에서 송신 로직을 탄경우.  [폐기]
			// 
			// 결국엔 서버에서 맞은것이 더 옳으므로 결국 클라는 hit판정으로 위치 보정될것
			//MY_ASSERT(false, "서버는 Stun상태이지만 클라에선 Stun이 끝났다고 판단하고 MoveStart를  송신하는 경우가 존재.");
			return;

		case ePlayerState::Dead:
			return;
		case ePlayerState::Attack:
			if (PLAYER_ATTACK_TOTAL_FRAME - player.mAnimFrame >= ALLOW_DELAY_FRAME)
			{
				// 시나리오 : Attack을 돌고 서버는 패킷을 받은 후 돌기때문에 네트워크 지연으로 나올 수 있음.
				// 만일 허용치를 넘는다면 서버의 부하를 의심해보자.  1. SendQ Cap 줄이기
                player.mSyncCnt += PLAYER_ATTACK_TOTAL_FRAME - player.mAnimFrame;
				if (CONFIG_SINK_MAXCOUNT < player.mSyncCnt)
				{
                    // MY_ASSERT(false, "서버는 Attack상태이지만 클라에선 Attack이 끝났다고 판단하고 MoveStart를  송신하는 경우가 존재.");
					_InterlockedIncrement64(&mDisconnect_Sync);
					network::SeqAndIdx disconnectID{};
					disconnectID.Value = seqID;
					disconnectSession(disconnectID);
					return;
				}

				return;
			}
			break;
		case ePlayerState::Move:
		{
			// 시나리오 : 클라이언트는 Move중 임.
			// 서버 : Monster한테 맞음.
			// 클라이언트 : 아직 맞았다는 패킷을 못받았고  MoveStop후 MoveStart를 보냄.
			// 서버 :  stun중에 MoveStop은 무시했음. => Stun상태를 무시하면 안되기 때문.
			// 클라 : 정상적으로 MoveStop후 Start를 보냈고 추후에 Hit메세지를 받음.

			// 조치 : return을 하여  클라이언트가 Hit메세지로 좌표가 서버와 일치하게 유도한다.

			//std::cout << std::setw(10) << "AccountNo : "
			//	<< player.mAccountNo << std::setw(30) << "\t Move 상태에서 MoveStart이 발생.\n";
			//MY_ASSERT(false, "Move상태에서 MoveStart가 도착하는 경우가 존재.");
		}
		return;
		default:
			break;
		}

		const map::Position packetPos{ x,y };

		// 서버와 차이가 큰 차이라면 연결 끊기.
		volatile float distance = player.mPos - packetPos;

		if (syncDistance < distance)
		{
			player.mSyncCnt += static_cast<uint64_t>(distance - syncDistance);
			if (CONFIG_SINK_MAXCOUNT < player.mSyncCnt)
			{
				MY_ASSERT(false, "클라이언트가 보낸 위치가 서버와 다르다.");
				_InterlockedIncrement64(&mDisconnect_Sync);

				network::SeqAndIdx disconnectID{};
				disconnectID.Value = seqID;
				disconnectSession(disconnectID);
				return;
			}
		}
		else
		{
			if (10 < player.mSyncCnt)
			{
				player.mSyncCnt -= 10;
			}
		}
		player.mPos = packetPos;
		player.mDirection = static_cast<eDirection>(direction);
		player.changeState(ePlayerState::Move);

		//TODO : Test 끝나면 DisConnect
		MY_ASSERT(player.mSpeed == speed, "클라이언트가 보낸 스피드가 서버와 다르다.");

		int64_t characterID = player.mCharacterID;
		std::vector<map::Sector> nearSector;
		getAroundSectors(player.mCurrentSector, nearSector);
		broadcastMoveStart(player, nearSector);
	}
	void FieldServer::handleMoveStop(utility::Message& msg, int64_t seqID)
	{
		PacketType::MOVE_STOP;
		int64_t characterId;
		float   x;
		float   y;
		int8_t  direction;

		msg >> characterId >> x >> y >> direction;

		DWORD startTime = timeGetTime();
		DWORD recvTime = msg.GetRecvTick();


		// WHY : 20ms 이상 차이가나서 결과가 달라지는 경우로 인한 문제인가 실측코드
		DWORD delayTime = startTime - recvTime;

		std::unordered_map<int64_t, Player*>::iterator iter = players.find(seqID);
		MY_ASSERT(iter != players.end(), "패킷이 왔는데 현재 쓰레드에 Player가 없는 말이안되는 상황");

		Player& player = *iter->second;

		switch (player.GetSpawnState())
		{
			//{
			//	std::cout << std::setw(10) << "AccountNo : " << player.mAccountNo
			//		<< std::setw(30) << "\tStun 상태에서 MoveStop이 발생.\n";
			//	return;
			//}
		case ePlayerState::Attack:
		case ePlayerState::Dead:
		case ePlayerState::Stun:
			return;
		default:
			break;
		}

		const map::Position packetPos{ x,y };

		// 서버와 차이가 큰 차이라면 연결 끊기.
		volatile float distance = player.mPos - packetPos;

		float serverDelayFrame = delayTime / 20.f;
		//TODO: 조작된 패킷 대응해야하지만. 지금은 서버를 멈춤
		MY_ASSERT(0 <= packetPos.x, "클라이언트가 잘못보냄.");
		MY_ASSERT(0 <= packetPos.y, "클라이언트가 잘못보냄.");
		MY_ASSERT(static_cast<float>(SECTOR_WORLD_W * SECTOR_COL_CNT) >= packetPos.x, "클라이언트가 잘못보냄.");
		MY_ASSERT(static_cast<float>(SECTOR_WORLD_H * SECTOR_ROW_CNT) >= packetPos.y, "클라이언트가 잘못보냄.");

		if (syncDistance < distance)
		{
			player.mSyncCnt += static_cast<uint64_t>(distance - syncDistance);
			if (CONFIG_SINK_MAXCOUNT < player.mSyncCnt)
			{
				_InterlockedIncrement64(&mDisconnect_Sync);

				network::SeqAndIdx disconnectID{};
				disconnectID.Value = seqID;
				disconnectSession(disconnectID);

				//MY_ASSERT(false, "너무 차이많이나는것을 허용안 함.");
				return;
			}
		}
		else
		{
			if (10 < player.mSyncCnt)
			{
				player.mSyncCnt -= 10;
			}
		}

		player.mPos = packetPos;
		player.mDirection = static_cast<eDirection>(direction);

		if (player.mState != ePlayerState::Idle)
		{
			player.changeState(ePlayerState::Idle);
		}

		int64_t characterID = player.mCharacterID;
		std::vector<map::Sector> nearSector;
		getAroundSectors(player.mCurrentSector, nearSector);
		broadcastMoveStop(player, nearSector);
	}
	void FieldServer::handleAttackReq(utility::Message& msg, int64_t seqID)
	{
		int8_t skillId;
		int16_t targetCnt;
		msg >> skillId >> targetCnt;

		float rangeLen = 0.f;
		int16_t maxTargetCnt = 0;
		if (!getSkillConfig(skillId, rangeLen, maxTargetCnt) || targetCnt > maxTargetCnt)
		{
			network::SeqAndIdx disconnectID{};
			disconnectID.Value = seqID;
			disconnectSession(disconnectID);
			return;
		}

		DWORD startTime = timeGetTime();
		DWORD recvTime = msg.GetRecvTick();


		// WHY : 20ms 이상 차이가나서 결과가 달라지는 경우로 인한 문제인가 실측코드
		DWORD delayTime = startTime - recvTime;

		std::unordered_map<int64_t, Player*>::iterator iter = players.find(seqID);
		MY_ASSERT(iter != players.end(), "패킷이 왔는데 현재 쓰레드에 Player가 없는 말이안되는 상황");
		Player& player = *iter->second;

		switch (player.GetSpawnState())
		{
		case ePlayerState::Dead:
		case ePlayerState::Attack:
			return;
			// Why : 지연시간으로 인해 공격이 무시당하는경우가 발생
		case ePlayerState::Stun:
			// 실제로 해당 경우가 나옴을 확인함.
			// 시나리오 : 조건 1 : 서버에서 Stun을 판단했지만 Player는 아직 못받은 경우
			// 시나리오 : 조건 2 : 서버가 느려지며 아직 안끝남을 판단했지만 Player는 정상처리한 경우
			return;
		default:
			break;
		}

		std::vector<map::Sector> nearSector;
		getAroundSectors(player.mCurrentSector, nearSector);
		if (player.mState != ePlayerState::Attack)
		{
			player.changeState(ePlayerState::Attack);
		}
		else
		{
			player.mAnimFrame = 0;
		}
		player.mPendingAttackSkillId = skillId;
		broadcastPlayerAttack(player, nearSector);

		player.mPendingAttackTargets.clear();
		for (int16_t i = 0; i < targetCnt; ++i)
		{
			int64_t monsterId;
			msg >> monsterId;
			player.mPendingAttackTargets.push_back(monsterId);
		}
	}
	void FieldServer::processPlayerAttackHit(Player& player, int fieldIdx)
	{
		float rangeLen = 0.f;
		int16_t maxTargetCnt = 0;
		if (!getSkillConfig(player.mPendingAttackSkillId, rangeLen, maxTargetCnt))
		{
			return;
		}

		// 몬스터의 유효성 체크.
		for (auto iter = player.mPendingAttackTargets.begin(); iter != player.mPendingAttackTargets.end(); )
		{
			auto monsterIter = monsters.find(*iter);
			if (monsterIter == monsters.end())
			{
				//"다른 Player가 Monster를 죽이는 경우 ");
				iter = player.mPendingAttackTargets.erase(iter);
				continue;
			}
			Monster& monster = *monsterIter->second;
			if (monster.mState == eMonsterState::Dead || monster.mState == eMonsterState::Return)
			{
				iter = player.mPendingAttackTargets.erase(iter);
				continue;
			}
			++iter;
		}
		sort(player.mPendingAttackTargets.begin(), player.mPendingAttackTargets.end(), [&](const int64_t a, const int64_t b)
			{
				auto MonsterIterA = monsters.find(a);
				Monster& monsterA = *MonsterIterA->second;
				auto MonsterIterB = monsters.find(b);
				Monster& monsterB = *MonsterIterB->second;
				float disA = player.mPos - monsterA.mPos;
				float disB = player.mPos - monsterB.mPos;
				if (disA == disB)
				{
					return a < b;
				}
				return disA < disB;

			}
		);

		for (int64_t monsterId : player.mPendingAttackTargets)
		{
			auto monsterIter = monsters.find(monsterId);
			if (monsterIter == monsters.end())
			{
				continue;
			}
			Monster& monster = *monsterIter->second;

			if (!isInAttackRange(player.mPos.x, player.mPos.y, monster.mPos.x, monster.mPos.y, rangeLen))
			{
				continue;
			}

			monster.takeDamage(player.mAttackPower, &player, player.mSeqID.Value);

			// Monster주변으로 데미지 패킷을 보냄
			std::vector<map::Sector> monsterNearSector;
			getAroundSectors(monster.mSector, monsterNearSector);
			broadcastMonsterDamaged(monster, monsterNearSector);

			if (monster.mState == eMonsterState::Dead)
			{
				dropItem(monster);
				requestKillCountUpdate(fieldIdx, player, 1);
			}

			--maxTargetCnt;
			if (maxTargetCnt == 0)
			{
				break;
			}
		}
	}

	namespace
	{
		bool ZincrbySync(cpp_redis::client& client, const std::string& key, int64_t incr, const std::string& member)
		{
			std::promise<bool> prom;
			auto fut = prom.get_future();

			client.zincrby(key, static_cast<int>(incr), member, [&prom](cpp_redis::reply& reply)
				{
					prom.set_value(!reply.is_error());
				});

			client.sync_commit();
			return fut.get();
		}

		// ZREVRANGE key 0~stop WITHSCORES 동기 버전 — (member, score) 쌍 목록
		std::vector<std::pair<std::string, double>> ZRevRangeWithScoresSync(cpp_redis::client& client, const std::string& key, int32_t stop)
		{
			std::promise<std::vector<std::pair<std::string, double>>> prom;
			auto fut = prom.get_future();

			client.zrevrange(key, 0, stop, true, [&prom](cpp_redis::reply& reply)
				{
					std::vector<std::pair<std::string, double>> result;
					if (reply.is_array())
					{
						const auto& arr = reply.as_array();
						for (size_t i = 0; i + 1 < arr.size(); i += 2)
						{
							result.emplace_back(arr[i].as_string(), std::stod(arr[i + 1].as_string()));
						}
					}
					prom.set_value(std::move(result));
				});

			client.sync_commit();
			return fut.get();
		}

		// ZREVRANK key member 동기 버전 — 없으면 -1 (0-based)
		int64_t ZRevRankSync(cpp_redis::client& client, const std::string& key, const std::string& member)
		{
			std::promise<int64_t> prom;
			auto fut = prom.get_future();

			client.zrevrank(key, member, [&prom](cpp_redis::reply& reply)
				{
					prom.set_value(reply.is_null() ? -1 : reply.as_integer());
				});

			client.sync_commit();
			return fut.get();
		}

		// ZSCORE key member 동기 버전 — 없으면 0
		int64_t ZScoreSync(cpp_redis::client& client, const std::string& key, const std::string& member)
		{
			std::promise<int64_t> prom;
			auto fut = prom.get_future();

			client.zscore(key, member, [&prom](cpp_redis::reply& reply)
				{
					prom.set_value(reply.is_null() ? 0 : static_cast<int64_t>(std::stod(reply.as_string())));
				});

			client.sync_commit();
			return fut.get();
		}

		// HSET key field value 동기 버전
		bool HSetSync(cpp_redis::client& client, const std::string& key, const std::string& field, const std::string& value)
		{
			std::promise<bool> prom;
			auto fut = prom.get_future();

			client.hset(key, field, value, [&prom](cpp_redis::reply& reply)
				{
					prom.set_value(!reply.is_error());
				});

			client.sync_commit();
			return fut.get();
		}

		// HMGET key field1 field2 ... 동기 버전 — 순서대로 값(없으면 빈 문자열) 반환
		std::vector<std::string> HMGetSync(cpp_redis::client& client, const std::string& key, const std::vector<std::string>& fields)
		{
			std::promise<std::vector<std::string>> prom;
			auto fut = prom.get_future();

			client.hmget(key, fields, [&prom](cpp_redis::reply& reply)
				{
					std::vector<std::string> result;
					if (reply.is_array())
					{
						for (const auto& r : reply.as_array())
						{
							result.push_back(r.is_null() ? "" : r.as_string());
						}
					}
					prom.set_value(std::move(result));
				});

			client.sync_commit();
			return fut.get();
		}
	}

	void FieldServer::dbThread(int fieldIdx)
	{
		// CDB 생성자가 GetCurrentThread()로 이름을 읽으므로, CDB db; 보다 반드시 먼저 설정
		std::wstring threadName = L"DBThread" + std::to_wstring(fieldIdx);
		SetThreadDescription(GetCurrentThread(), threadName.c_str());

		CDB db;
		if (!db.Connect("localhost", "root", "123123", "stoneage", 3306))
		{
			db.ClearError();
			MY_ASSERT(false, "dbThread: DB Connect 실패");
		}

		cpp_redis::client redisClient;
		redisClient.connect(CONFIG_REDIS_IP, CONFIG_REDIS_PORT);

		//
		const HANDLE handles[2] = { hDBEvent[fieldIdx], hDBFinishEvent[fieldIdx] };
		while (true)
		{
			// WaitAll false경우 반환 값에서 WAIT_OBJECT_0 뺀 값은 대기를 충족하는 개체의 배열 인덱스를 나타냅니다.
			DWORD retval =  WaitForMultipleObjects(2, handles, false, INFINITE);

			char* f = mDBReqQ[fieldIdx]->GetFrontPtr();
			char* r = mDBReqQ[fieldIdx]->GetRearPtr();
			int32_t useSize = mDBReqQ[fieldIdx]->GetUseSize(f, r);

			// 담당 필드 쓰레드가 종료되었고 dbQ에 아무것도 남아있지않을떄
			if (retval - WAIT_OBJECT_0 == 1 && useSize == 0)
			{
				break;
			}

			while (useSize >= sizeof(size_t))
			{
				DBJob* job;
				mDBReqQ[fieldIdx]->Dequeue(&job, sizeof(size_t));
				useSize -= sizeof(size_t);
				_InterlockedDecrement64(&mDBMessageQCnt[fieldIdx]);

				eDBJobType jobType = static_cast<eDBJobType>(job->GetTypeID());
				DBResult* result = nullptr;

				switch (jobType)
				{
				case eDBJobType::PositionSave:
				{
					DBPositionSave* positionSaveJob = static_cast<DBPositionSave*>(job);
					const map::Position pos = positionSaveJob->GetPos();

					char query[256];
					snprintf(query, sizeof(query),
						"UPDATE Accounts SET x=%f, y=%f WHERE accountNo=%lld",
						pos.x, pos.y, positionSaveJob->GetAccount());

					bool bSuccess = db.Execute(query);
					if (!bSuccess)
					{
						db.ClearError();
					}
					result = MY_NEW DBResult(jobType, job->GetSeqID(), bSuccess);
					break;
				}
				case eDBJobType::KillCountUpdate:
				{
					DBKillCountUpdate* killCountUpdateJob = static_cast<DBKillCountUpdate*>(job);
					const char* const nickName = killCountUpdateJob->GetNickName();
					int64_t killCnt = killCountUpdateJob->GetKiilCnt();

					char query[256];
					snprintf(query, sizeof(query),
						"UPDATE Accounts SET killCount = killCount + %lld, lastKillTime = NOW() WHERE accountNo=%lld",
						killCnt, killCountUpdateJob->GetAccount());

					bool dbOk = db.Execute(query);
					if (!dbOk)
					{
						db.ClearError();
					}

					bool redisOk = ZincrbySync(redisClient, "ranking:killcount", killCnt, nickName);
					int64_t nowEpoch = static_cast<int64_t>(std::time(nullptr));
					bool redisTimeOk = HSetSync(redisClient, "ranking:lastkilltime", nickName, std::to_string(nowEpoch));

					result = MY_NEW DBResult(jobType, job->GetSeqID(), dbOk && redisOk && redisTimeOk);
					break;
				}
				case eDBJobType::RankingQuery:
				{
					DBRankingQuery* rankingJob = static_cast<DBRankingQuery*>(job);
					const std::string myNickname = rankingJob->GetNickName();

					auto topList = ZRevRangeWithScoresSync(redisClient, "ranking:killcount", DBRankingResult::MAX_TOP_ENTRY - 1);
					int64_t myRankZeroBased = ZRevRankSync(redisClient, "ranking:killcount", myNickname);
					int64_t myKillCount = ZScoreSync(redisClient, "ranking:killcount", myNickname);

					// 동률 구간만 Redis lastKillTime으로 보조정렬 (DB 접속 없음)
					size_t i = 0;
					while (i < topList.size())
					{
						size_t j = i + 1;
						while (j < topList.size() && topList[j].second == topList[i].second)
						{
							++j;
						}
						if (j - i > 1) // [i, j) 구간이 동률
						{
							std::vector<std::string> tiedMembers;
							for (size_t k = i; k < j; ++k)
							{
								tiedMembers.push_back(topList[k].first);
							}

							std::vector<std::string> times = HMGetSync(redisClient, "ranking:lastkilltime", tiedMembers);

							std::unordered_map<std::string, int64_t> lastKillTimeMap;
							for (size_t k = 0; k < tiedMembers.size(); ++k)
							{
								lastKillTimeMap[tiedMembers[k]] = times[k].empty() ? 0 : std::stoll(times[k]);
							}

							std::sort(topList.begin() + i, topList.begin() + j,
								[&lastKillTimeMap](const auto& a, const auto& b)
								{
									return lastKillTimeMap[a.first] < lastKillTimeMap[b.first];
								});
						}
						i = j;
					}

					int32_t topCnt = static_cast<int32_t>(topList.size());
					int64_t myRank = (myRankZeroBased == -1) ? -1 : (myRankZeroBased + 1);

					DBRankingResult* rankingResult = MY_NEW DBRankingResult(job->GetSeqID(), true, topCnt, myRank, myKillCount);
					for (int32_t idx = 0; idx < topCnt; ++idx)
					{
						RankEntry& entry = rankingResult->GetEntry(idx);
						memset(entry.nickname, 0, sizeof(entry.nickname));
						size_t len = (std::min)(topList[idx].first.size(), sizeof(entry.nickname) - 1);
						memcpy(entry.nickname, topList[idx].first.data(), len);
						entry.killCount = static_cast<int64_t>(topList[idx].second);
					}

					result = rankingResult;
					break;
				}
				default:
					MY_ASSERT(false, "dbThread: 정의되지 않은 DBJob 타입");
					break;
				}

				MY_DELETE job; // requestXXX에서 MY_NEW로 만든 job은 여기서 다 쓴 뒤 직접 해제 (안 하면 처리할 때마다 누수)
				mDBResQ[fieldIdx]->Enqueue(&result, sizeof(result));
			}

		}
	}

	void FieldServer::requestPositionSave(int fieldIdx, Player& player)
	{
		MY_ASSERT((player.mDBRequestCnt & DB_POSITION_SAVE_PENDING_BIT) == 0, "이미 저장 요청을 보낸 Player에게 중복 요청");

		DBPositionSave* job = MY_NEW DBPositionSave(player.mSeqID.Value, player.mAccountNo
			, player.mPos);

		mDBReqQ[fieldIdx]->Enqueue(&job, sizeof(job)); // job 자체(객체 내용)가 아니라 job이 가리키는 주소값을 큐에 실어야 함
		_interlockedincrement64(&mDBMessageQCnt[fieldIdx]);
		player.mDBRequestCnt |= DB_POSITION_SAVE_PENDING_BIT;
		SetEvent(hDBEvent[fieldIdx]);
	}

	void FieldServer::requestKillCountUpdate(int fieldIdx, Player& player, int64_t killCnt)
	{
		DBKillCountUpdate* job = MY_NEW DBKillCountUpdate(player.mSeqID.Value, player.mAccountNo, player.mNickname, killCnt);

		mDBReqQ[fieldIdx]->Enqueue(&job, sizeof(job)); // job 자체가 아니라 job이 가리키는 주소값을 큐에 실어야 함
		_interlockedincrement64(&mDBMessageQCnt[fieldIdx]);
		SetEvent(hDBEvent[fieldIdx]);
	}

	void FieldServer::requestRankingQuery(int fieldIdx, Player& player)
	{
		DBRankingQuery* job = MY_NEW DBRankingQuery(player.mSeqID.Value, player.mAccountNo, player.mNickname);

		mDBReqQ[fieldIdx]->Enqueue(&job, sizeof(job)); // job 자체가 아니라 job이 가리키는 주소값을 큐에 실어야 함
		_interlockedincrement64(&mDBMessageQCnt[fieldIdx]);
		SetEvent(hDBEvent[fieldIdx]);
	}

	void FieldServer::dbResultPacketProc(int fieldIdx)
	{
		char* f = mDBResQ[fieldIdx]->GetFrontPtr();
		char* r = mDBResQ[fieldIdx]->GetRearPtr();
		int32_t useSize = mDBResQ[fieldIdx]->GetUseSize(f, r);

		while (useSize >= sizeof(size_t))
		{
			DBResult* result;
			mDBResQ[fieldIdx]->Dequeue(&result, sizeof(size_t));
			useSize -= sizeof(size_t);

			switch (static_cast<eDBJobType>(result->GetTypeID()))
			{
			case eDBJobType::PositionSave:
			{
				auto iter = players.find(result->GetSeqID());
				if (iter == players.end())
				{
					// disconnect 저장 요청 뒤 응답 오기 전엔 players에서 안 지우므로
					// 이론상 항상 찾아져야 함. 방어적으로만 무시.
					break;
				}
				Player& player = *iter->second;

				// despawn/removeFromSector는 checkDisConnectedAndLeavePlayer에서
				// 연결 끊긴 즉시 이미 처리함 — 여기선 계정 정리만.
				player.mDBRequestCnt &= ~DB_POSITION_SAVE_PENDING_BIT;
		
				notifyDisconnect(fieldIdx, player.mAccountNo);

				map::Sector sector = player.mCurrentSector;

				auto sectoriter = sectors[sector.y][sector.x].find(&player);
				MY_ASSERT(sectoriter != sectors[sector.y][sector.x].end(), "못찾으면 말이안 됨.");
				sectors[sector.y][sector.x].erase(sectoriter);

				players.erase(iter);
				
				break;
			}
			case eDBJobType::KillCountUpdate:
			{
				if (!result->IsSuccess())
				{
					std::cout << "[KillCountUpdate] DB/Redis 갱신 실패, seqID=" << result->GetSeqID() << "\n";
				}
				break;
			}
			case eDBJobType::RankingQuery:
			{
				DBRankingResult* rankingResult = static_cast<DBRankingResult*>(result);

				auto iter = players.find(rankingResult->GetSeqID());
				if (iter == players.end())
				{
					// 응답 오기 전 disconnect — 보낼 대상이 없으므로 조용히 무시
					break;
				}
				Player& player = *iter->second;

				int16_t topCnt = static_cast<int16_t>(rankingResult->GetTopCnt());

				Header header{ 0 };
				header.Len = sizeof(int16_t)                                              // type
					+ sizeof(int16_t)                                                     // topCnt
					+ topCnt * static_cast<int16_t>(sizeof(RankEntry::nickname) + sizeof(int64_t)) // entries
					+ sizeof(int64_t) * 2;                                                // myRank + myKillCount

				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player.mSeqID.Value, 0);
				msg->PutData(&header, sizeof(header));

				*msg << (int16_t)PacketType::RANKING_RES;
				*msg << topCnt;
				for (int32_t idx = 0; idx < rankingResult->GetTopCnt(); ++idx)
				{
					const RankEntry& entry = rankingResult->GetEntry(idx);
					msg->PutData(entry.nickname, sizeof(entry.nickname));
					*msg << entry.killCount;
				}
				*msg << rankingResult->GetMyRank();
				*msg << rankingResult->GetMyKillCount();

				sendPost(player.mSeqID, *msg);
				break;
			}
			default:
				MY_ASSERT(false, "dbResultPacketProc: 정의되지 않은 DBResult 타입");
				break;
			}

			MY_DELETE result;
		}
	}

	void FieldServer::dropItem(const Monster& monster)
	{
		eItemId itemId;
		int32_t count;
		if (!rollDrop(itemId, count))
		{
			return;   // 무드랍
		}

		int64_t itemUniqueId = InterlockedIncrement64(&mItemUniqueID);
		FieldItem* item = MY_NEW FieldItem(itemUniqueId, itemId, count, monster.mPos);

		map::Sector sector;
		calcSector(item->mPos, sector);
		item->mSector = sector;

		items.insert({ item->mItemUniqueId, item });
		itemSectors[sector.y][sector.x].insert(item);

		std::vector<map::Sector> nearSector;
		getAroundSectors(sector, nearSector);
		broadcastItemSpawn(*item, nearSector);
	}
	void FieldServer::broadcastItemSpawn(const FieldItem& item, const std::vector<map::Sector>& nearSector)
	{
		Header header{ 0 };
		header.Len = sizeof(int16_t) + sizeof(item.mItemUniqueId)
			+ sizeof(int8_t)   // itemId
			+ sizeof(item.mPos.x) + sizeof(item.mPos.y);

		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player->mSeqID.Value, 0);
				msg->PutData(&header, sizeof(header));
				*msg << static_cast<int16_t>(PacketType::ITEM_SPAWN);
				*msg << item.mItemUniqueId;
				*msg << static_cast<int8_t>(item.mItemId);
				*msg << item.mPos.x;
				*msg << item.mPos.y;
				sendPost(player->mSeqID, *msg);
				stats::RecordSend(PacketType::ITEM_SPAWN);
			}
		}
	}
	void FieldServer::broadcastItemDespawn(const FieldItem& item, const std::vector<map::Sector>& nearSector)
	{
		Header header{ 0 };
		header.Len = sizeof(int16_t) + sizeof(item.mItemUniqueId);

		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player->mSeqID.Value, 0);
				msg->PutData(&header, sizeof(header));
				*msg << static_cast<int16_t>(PacketType::ITEM_DESPAWN);
				*msg << item.mItemUniqueId;
				sendPost(player->mSeqID, *msg);
				stats::RecordSend(PacketType::ITEM_DESPAWN);
			}
		}
	}
	void FieldServer::broadcastMonsterDamaged(const Monster& monster, const std::vector<map::Sector>& nearSector)
	{
		constexpr int32_t RECORD_SIZE = sizeof(int64_t) + sizeof(int32_t) + sizeof(float) + sizeof(float) + sizeof(int8_t);
		Header header{ 0 };
		header.Len = sizeof(int16_t) + RECORD_SIZE;

		for (const map::Sector& sector : nearSector)
		{
			for (const Player* recvPlayer : sectors[sector.y][sector.x])
			{
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(recvPlayer->mSeqID.Value, 0);
				msg->PutData(&header, sizeof(header));
				*msg << static_cast<int16_t>(PacketType::MONSTER_DAMAGED);

				*msg << monster.mMonsterID;
				*msg << monster.mHp;
				*msg << monster.mPos.x;
				*msg << monster.mPos.y;
				*msg << static_cast<int8_t>(monster.mDirection);

				sendPost(recvPlayer->mSeqID, *msg);
				stats::RecordSend(PacketType::MONSTER_DAMAGED);
			}
		}
	}

	void FieldServer::broadcastCharacterDamaged(const Player& damagedPlayer, const Monster& attacker, const std::vector<map::Sector>& nearSector)
	{
		Header header{ 0 };
		header.Len = sizeof(int16_t) + sizeof(damagedPlayer.mCharacterID) + sizeof(damagedPlayer.mHp)
			+ sizeof(damagedPlayer.mPos.x) + sizeof(damagedPlayer.mPos.y) + sizeof(damagedPlayer.mDirection)
			+ sizeof(attacker.mMonsterID);

		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player->mSeqID.Value, 0);
				msg->PutData(&header, sizeof(header));
				*msg << static_cast<int16_t>(PacketType::CHARACTER_DAMAGED);
				*msg << damagedPlayer.mCharacterID;
				*msg << damagedPlayer.mHp;
				*msg << damagedPlayer.mPos.x;
				*msg << damagedPlayer.mPos.y;
				*msg << static_cast<int8_t>(damagedPlayer.mDirection);
				*msg << attacker.mMonsterID;
				sendPost(player->mSeqID, *msg);
				stats::RecordSend(PacketType::CHARACTER_DAMAGED);
			}
		}
	}

	void FieldServer::broadcastMoveStart(const Player& sendPlayer, const std::vector<map::Sector>& nearSector)
	{
		PacketType::MOVE_START;
		Header header{ 0 };

		header.Len = sizeof(int16_t) + sizeof(sendPlayer.mCharacterID)
			+ sizeof(sendPlayer.mPos.x) + sizeof(sendPlayer.mPos.y)
			+ sizeof(sendPlayer.mDirection) + sizeof(sendPlayer.mSpeed);

		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				if (player == &sendPlayer)
				{
					continue;
				}
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player->mSeqID.Value, 0);
				msg->PutData(&header, sizeof(header));
				*msg << static_cast<int16_t>(PacketType::MOVE_START);
				*msg << sendPlayer.mCharacterID;
				*msg << sendPlayer.mPos.x;
				*msg << sendPlayer.mPos.y;
				*msg << static_cast<int8_t>(sendPlayer.mDirection);
				*msg << sendPlayer.mSpeed;
				sendPost(player->mSeqID, *msg);
				stats::RecordSend(PacketType::MOVE_START);
			}
		}

	}
	void FieldServer::broadcastMoveStop(const Player& sendPlayer, const std::vector<map::Sector>& nearSector)
	{
		PacketType::MOVE_STOP;
		Header header{ 0 };

		header.Len = sizeof(int16_t) + sizeof(sendPlayer.mCharacterID) + sizeof(sendPlayer.mPos.x) + sizeof(sendPlayer.mPos.y)
			+ sizeof(sendPlayer.mDirection);

		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				if (player == &sendPlayer)
				{
					continue;
				}

				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player->mSeqID.Value, 0);
				msg->PutData(&header, sizeof(header));
				*msg << static_cast<int16_t>(PacketType::MOVE_STOP);
				*msg << sendPlayer.mCharacterID;
				*msg << sendPlayer.mPos.x;
				*msg << sendPlayer.mPos.y;
				*msg << static_cast<int8_t>(sendPlayer.mDirection);
				sendPost(player->mSeqID, *msg);
				stats::RecordSend(PacketType::MOVE_STOP);
			}
		}
	}
	void FieldServer::broadcastPlayerAttack(const Player& sendPlayer, const std::vector<map::Sector>& nearSector)
	{
		Header header{ 0 };
		header.Len = sizeof(int16_t) + sizeof(sendPlayer.mCharacterID);

		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				if (player == &sendPlayer)
				{
					continue;
				}
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player->mSeqID.Value, 0);
				msg->PutData(&header, sizeof(header));
				*msg << static_cast<int16_t>(PacketType::OTHER_CHARACTER_ATTACK);
				*msg << sendPlayer.mCharacterID;
				sendPost(player->mSeqID, *msg);
				stats::RecordSend(PacketType::OTHER_CHARACTER_ATTACK);
			}
		}
	}
	void FieldServer::broadcastMonsterMoveStart(const Monster& monster, const std::vector<map::Sector>& nearSector)
	{
		Header header{ 0 };
		header.Len = sizeof(int16_t) + sizeof(monster.mMonsterID)
			+ sizeof(monster.mPos.x) + sizeof(monster.mPos.y)
			+ sizeof(monster.mDirection) + sizeof(monster.mSpeed);

		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player->mSeqID.Value, 0);
				msg->PutData(&header, sizeof(header));
				*msg << static_cast<int16_t>(PacketType::MONSTER_MOVE_START);
				*msg << monster.mMonsterID;
				*msg << monster.mPos.x;
				*msg << monster.mPos.y;
				*msg << static_cast<int8_t>(monster.mDirection);
				*msg << monster.mSpeed;
				sendPost(player->mSeqID, *msg);
				stats::RecordSend(PacketType::MONSTER_MOVE_START);
			}
		}
	}
	void FieldServer::broadcastMonsterMoveStop(const Monster& monster, const std::vector<map::Sector>& nearSector)
	{
		Header header{ 0 };
		header.Len = sizeof(int16_t) + sizeof(monster.mMonsterID)
			+ sizeof(monster.mPos.x) + sizeof(monster.mPos.y)
			+ sizeof(monster.mDirection);

		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player->mSeqID.Value, 0);
				msg->PutData(&header, sizeof(header));
				*msg << static_cast<int16_t>(PacketType::MONSTER_MOVE_STOP);
				*msg << monster.mMonsterID;
				*msg << monster.mPos.x;
				*msg << monster.mPos.y;
				*msg << static_cast<int8_t>(monster.mDirection);
				sendPost(player->mSeqID, *msg);
				stats::RecordSend(PacketType::MONSTER_MOVE_STOP);
			}
		}
	}
	void FieldServer::broadcastMonsterAttack(const Monster& monster, const std::vector<map::Sector>& nearSector)
	{
		Header header{ 0 };
		header.Len = sizeof(int16_t) + sizeof(monster.mMonsterID);

		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player->mSeqID.Value, 0);
				msg->PutData(&header, sizeof(header));
				*msg << static_cast<int16_t>(PacketType::MONSTER_ATTACK);
				*msg << monster.mMonsterID;
				sendPost(player->mSeqID, *msg);
				stats::RecordSend(PacketType::MONSTER_ATTACK);
			}
		}
	}
	void FieldServer::broadcastMonsterSpawn(const Monster& monster, const std::vector<map::Sector>& nearSector)
	{
		Header header{ 0 };
		header.Len = sizeof(int16_t) + sizeof(monster.mMonsterID)
			+ sizeof(monster.mPos.x) + sizeof(monster.mPos.y)
			+ sizeof(monster.mState)
			+ sizeof(monster.mDirection) + sizeof(monster.mHp) + sizeof(monster.mMonsterType)
			+ sizeof(int32_t);   // animFrame

		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player->mSeqID.Value, 0);
				msg->PutData(&header, sizeof(header));
				*msg << static_cast<int16_t>(PacketType::MONSTER_SPAWN);
				*msg << monster.mMonsterID;
				*msg << monster.mPos.x;
				*msg << monster.mPos.y;
				*msg << static_cast<int8_t>(monster.mState);

				*msg << static_cast<int8_t>(monster.mDirection);
				*msg << monster.mHp;
				*msg << monster.mMonsterType;
				*msg << monster.GetAnimFrame();
				sendPost(player->mSeqID, *msg);
				stats::RecordSend(PacketType::MONSTER_SPAWN);
			}
		}
	}
	void FieldServer::broadcastMonsterDespawn(const Monster& monster, const std::vector<map::Sector>& nearSector)
	{
		Header header{ 0 };
		header.Len = sizeof(int16_t) + sizeof(monster.mMonsterID);

		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player->mSeqID.Value, 0);
				msg->PutData(&header, sizeof(header));
				*msg << static_cast<int16_t>(PacketType::MONSTER_DESPAWN);
				*msg << monster.mMonsterID;
				sendPost(player->mSeqID, *msg);
				stats::RecordSend(PacketType::MONSTER_DESPAWN);
			}
		}
	}
	void FieldServer::checkDisConnectedAndLeavePlayer(int fieldIdx)
	{
		int64_t disconnectSession[CONFIG_DISCONNECT_AND_LEAVE_CAP]{ 0 };
		int disconnectCnt = 0;
		int64_t leaveFieldSession[CONFIG_DISCONNECT_AND_LEAVE_CAP]{ 0 };
		int leaveCnt = 0;

		for (auto iter = players.begin(); iter != players.end(); ++iter)
		{
			Player& player = *iter->second;
			int64_t seqID = player.mSeqID.Value;
			if (!player.bConnect)
			{
				disconnectSession[disconnectCnt++] = seqID;
			}
			else if (player.mNextFieldID != fieldIdx)
			{
				leaveFieldSession[leaveCnt++] = seqID;
			}
			if (disconnectCnt == CONFIG_DISCONNECT_AND_LEAVE_CAP || leaveCnt == CONFIG_DISCONNECT_AND_LEAVE_CAP)
			{
				break;
			}
		}
		// 연결 끊김이 감지된 session을 DB에 위치를 저장 후 끊기.
		for (int i = 0; i < disconnectCnt; ++i)
		{
			//Field 쓰레드라면 주변 세션에게 제거 메세지.
			auto iter = players.find(disconnectSession[i]);
			MY_ASSERT(iter != players.end(), "현재 스레드가 소유하지않은 Player");

			Player& player = *iter->second;

			if (fieldIdx != CONFIG_AUTH_FIELD_IDX)
			{
				if (player.mDBRequestCnt & DB_POSITION_SAVE_PENDING_BIT)
				{
					// 이미 저장 요청 보냄, 응답 대기중 — 이번 틱은 스킵 (players에서도 안 지움)
					continue;
				}
				requestPositionSave(fieldIdx, player);
				continue;
			}
			else
			{
				//Auth에서만 Player delete 수행
				{
					std::lock_guard<std::shared_mutex> xlock(mUnAuthLock);
					auto iter = mUnAuthSessions.find(player.mSeqID.Value);
					if (iter != mUnAuthSessions.end())
					{
						mUnAuthSessions.erase(iter);
					}
					else
					{
						mAccountsHash.erase(player.mAccountNo);
					}
					--mPlayerCnt;
				}
				while (utility::Message* msg = player.DeQueueMsgOrNull())
				{
					MY_DELETE msg;
					_interlockeddecrement64(&mPlayerMessageQCnt);
				}
				delete& player;
			}
			players.erase(disconnectSession[i]);
		}
		for (int i = 0; i < leaveCnt; ++i)
		{
			//Field 쓰레드라면 주변 세션에게 제거 메세지.
			auto iter = players.find(leaveFieldSession[i]);
			MY_ASSERT(iter != players.end(), "현재 스레드가 소유하지않은 Player");

			Player& player = *iter->second;
			if (player.mDBRequestCnt != 0)
			{
				continue;
			}
			if (fieldIdx != CONFIG_AUTH_FIELD_IDX)
			{

				std::vector<map::Sector> nearSector;
				getAroundSectors(player.mCurrentSector, nearSector);
				broadcastDespawn(player, nearSector);
				removeFromSector(player);
				// 다른 필드로 이동한다면
				if (player.mNextFieldID != CONFIG_AUTH_FIELD_IDX)
				{
					notifyMoveField(fieldIdx, player.mAccountNo, player.mNextFieldID);
					players.erase(iter);
					return;
				}
			}
			// 인증 쓰레드에서 필드 스레드로 이동하는 경우.
			enterField(&player, player.mNextFieldID);
		}
	}
	void FieldServer::notifyDisconnect(int fieldIdx, int64_t accountNo)
	{
		utility::Message* msg = MY_NEW utility::Message();
		msg->InitMessage(0, 0);
		*msg << (int16_t)PacketType::PLAYER_DISCONNECTED;
		*msg << accountNo;

		_InterlockedIncrement64(&mAuthMessageQCnt);
		mNotifyMsgQ[fieldIdx]->Enqueue(&msg, sizeof(utility::Message*));
		SetEvent(hAuthEvent);
	}
	void FieldServer::notifyMoveField(int fieldIdx, int64_t accountNo, int32_t targetFieldIdx)
	{
		utility::Message* msg = MY_NEW utility::Message();
		msg->InitMessage(0, 0);
		*msg << (int16_t)PacketType::PLAYER_MOVEFILED;
		*msg << accountNo;
		*msg << targetFieldIdx;

		_InterlockedIncrement64(&mAuthMessageQCnt);
		mNotifyMsgQ[fieldIdx]->Enqueue(&msg, sizeof(utility::Message*));
		SetEvent(hAuthEvent);
	}
	void FieldServer::authNotifyPacketProc(utility::Message& msg)
	{
		int16_t type;
		msg >> type;
		switch ((PacketType)type)
		{
		case PacketType::PLAYER_DISCONNECTED:
		{
			int64_t accountNo;
			msg >> accountNo;
			{
				auto iter = mAccountsHash.find(accountNo);
				MY_ASSERT(iter != mAccountsHash.end(), "로그 아웃을 했는데. AccountNo가 이미 없는 말도안되는 상황");
				players.insert({ iter->second->mSeqID.Value, iter->second });
				mAccountsHash.erase(accountNo);
			}
			break;
		}
		case PacketType::PLAYER_MOVEFILED:
		{
			int64_t accountNo;
			int32_t targetFieldIdx;
			msg >> accountNo;
			msg >> targetFieldIdx;
			{
				auto iter = mAccountsHash.find(accountNo);
				MY_ASSERT(iter != mAccountsHash.end(), "필드를 옮겼는데. AccountNo가 이미 없는 말도안되는 상황");
				// authThread에 player 추가.
				players.insert({ iter->second->mSeqID.Value, iter->second });
			}
			break;
		}
		default:
			MY_ASSERT(FALSE, "필드 서버의 스레드가 잘못된 패킷을 넣음");
		}
	}

	void FieldServer::calcSector(const map::Position& pos, __out map::Sector& newSector)
	{
		int col = (int)(pos.x / SECTOR_WORLD_W);
		int row = (int)(pos.y / SECTOR_WORLD_H);

		col = std::clamp(col, 0, SECTOR_COL_CNT - 1);
		row = std::clamp(row, 0, SECTOR_ROW_CNT - 1);

		newSector.x = static_cast<int8_t>(col);
		newSector.y = static_cast<int8_t>(row);
	}
	void FieldServer::enterSector(Player& player)
	{
		map::Sector newSector;
		calcSector(player.mPos, newSector);

		player.mDestSector = newSector;
		player.mCurrentSector = newSector;
		auto& sector = sectors[newSector.y][newSector.x];
		MY_ASSERT(sector.find(&player) == sector.end(), "섹터에 넣으려고했더니. 이미 존재함.");
		sector.insert(&player);
	}
	void FieldServer::updateSectorChechk(Player& player)
	{
		map::Sector newSector;
		calcSector(player.mPos, newSector);

		if (newSector == player.mCurrentSector)
		{
			return;
		}
		player.mDestSector = newSector;
	}
	void FieldServer::updateMonsterSectorCheck(Monster& monster)
	{
		map::Sector newSector;
		calcSector(monster.mPos, newSector);

		if (newSector == monster.mSector)
		{
			return;
		}
		monster.mDestSector = newSector;
	}
	void FieldServer::removeFromSector(Player& player)
	{
		auto& sector = sectors[player.mCurrentSector.y][player.mCurrentSector.x];
		auto iter = sector.find(&player);
		MY_ASSERT(iter != sector.end(), "섹터에 제거하려고 했더니. 없음");
		sector.erase(&player);
	}

	void FieldServer::getAroundSectors(const map::Sector& center, __out std::vector<map::Sector>& out) const
	{
		MY_ASSERT(mAroundSectorCache.find(center) != mAroundSectorCache.end(), "캐싱된 주변 섹터를 찾지못함.");
		out = mAroundSectorCache.find(center)->second;
	}

	void FieldServer::calcDespawnAndSpawnSector(const Player& player, __out std::vector<map::Sector>& spawnSector, __out std::vector<map::Sector>& despawnSector) const
	{
		spawnSector.reserve(9);
		despawnSector.reserve(9);

		getAroundSectors(player.mDestSector, spawnSector);
		getAroundSectors(player.mCurrentSector, despawnSector);

		for (auto currentIter = spawnSector.begin(); currentIter != spawnSector.end();)
		{
			bool bErase = false;
			map::Sector& currentSector = *currentIter;
			for (auto destIter = despawnSector.begin(); destIter != despawnSector.end();)
			{
				map::Sector& destSector = *destIter;
				if (currentSector == destSector)
				{
					destIter = despawnSector.erase(destIter);
					bErase = true;
					break;
				}
				++destIter;
			}
			if (bErase)
			{
				currentIter = spawnSector.erase(currentIter);
				continue;
			}
			++currentIter;
		}

	}

	void FieldServer::calcMonsterDespawnAndSpawnSector(const Monster& monster, __out std::vector<map::Sector>& spawnSector, __out std::vector<map::Sector>& despawnSector) const
	{
		spawnSector.reserve(9);
		despawnSector.reserve(9);

		getAroundSectors(monster.mDestSector, spawnSector);
		getAroundSectors(monster.mSector, despawnSector);

		for (auto currentIter = spawnSector.begin(); currentIter != spawnSector.end();)
		{
			bool bErase = false;
			map::Sector& currentSector = *currentIter;
			for (auto destIter = despawnSector.begin(); destIter != despawnSector.end();)
			{
				map::Sector& destSector = *destIter;
				if (currentSector == destSector)
				{
					destIter = despawnSector.erase(destIter);
					bErase = true;
					break;
				}
				++destIter;
			}
			if (bErase)
			{
				currentIter = spawnSector.erase(currentIter);
				continue;
			}
			++currentIter;
		}
	}

	void FieldServer::broadcastSpawn(const Player& sendPlayer, const std::vector<map::Sector>& nearSector)
	{
		bool bChk = false;
		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				// Echo는 기본적으로 안함.
				if (player == &sendPlayer)
				{
					bChk = true;
					continue;
				}
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player->mSeqID.Value, 0);

				Header header{ 0 };
				header.Len = sizeof(int16_t) + sizeof(int64_t) + sizeof(float) + sizeof(float)
					+ sizeof(int8_t) + sizeof(int8_t) + sizeof(sendPlayer.mSpeed)
					+ sizeof(int32_t)
					+ sizeof(int8_t) + sizeof(sendPlayer.mNickname);
				header.RandKey = 0;
				msg->PutData(&header, sizeof(header));

				*msg << (int16_t)PacketType::OTHER_CHARACTER_SPAWN;
				*msg << sendPlayer.mCharacterID;
				*msg << sendPlayer.mPos.x;
				*msg << sendPlayer.mPos.y;

				*msg << static_cast<int8_t>(sendPlayer.GetSpawnState());
				*msg << static_cast<int8_t>(sendPlayer.mDirection);
				*msg << sendPlayer.mSpeed;

				*msg << sendPlayer.GetAnimFrame();

				*msg << sendPlayer.mCharacterType;
				msg->PutData(sendPlayer.mNickname, sizeof(sendPlayer.mNickname));

				sendPost(player->mSeqID, *msg);
				stats::RecordSend(PacketType::OTHER_CHARACTER_SPAWN);
			}
		}
	}
	void FieldServer::sendSpawnBatch(const Player& targetPlayer, const std::vector<map::Sector>& nearSector)
	{
		bool bChk = false;
		int16_t cnt = 0;
		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				// Echo는 기본적으로 안함.
				if (player == &targetPlayer)
				{
					bChk = true;
					continue;
				}
				++cnt;
			}
		}

		if (cnt == 0)
		{
			return;
		}

		constexpr int32_t RECORD_SIZE = sizeof(int64_t) + sizeof(float) + sizeof(float)
			+ sizeof(int8_t) + sizeof(int8_t) + sizeof(targetPlayer.mSpeed)
			+ sizeof(int32_t)
			+ sizeof(int8_t) + sizeof(targetPlayer.mNickname);
		constexpr int32_t maxBatch = static_cast<int32_t>(utility::eBufferSize::BufferSize - sizeof(int16_t) - sizeof(int16_t) - sizeof(Header)) / RECORD_SIZE;
		utility::Message* msg = nullptr;

		while (cnt != 0)
		{
			Header header{ 0 };
			int16_t PlayerCnt = 0;
			for (const map::Sector& sector : nearSector)
			{
				for (const Player* player : sectors[sector.y][sector.x])
				{
					// Echo는 기본적으로 안함.
					if (player == &targetPlayer)
					{
						continue;
					}
					if (PlayerCnt == 0)
					{
						if (msg != nullptr)
						{
							sendPost(targetPlayer.mSeqID, *msg);
							stats::RecordSend(PacketType::OTHER_CHARACTER_SPAWN_BATCH);
							msg = nullptr;
						}
						if (cnt < maxBatch)
						{
							PlayerCnt = cnt;
						}
						else
						{
							PlayerCnt = maxBatch;
						}
						msg = MY_NEW utility::Message();
						msg->InitMessage(targetPlayer.mSeqID.Value, 0);

						header.Len = sizeof(int16_t) + sizeof(int16_t) + RECORD_SIZE * PlayerCnt;
						header.RandKey = 0;
						msg->PutData(&header, sizeof(header));

						*msg << (int16_t)PacketType::OTHER_CHARACTER_SPAWN_BATCH;
						*msg << PlayerCnt;
					}

					--cnt;
					--PlayerCnt;
					*msg << player->mCharacterID;
					*msg << player->mPos.x;
					*msg << player->mPos.y;

					*msg << static_cast<int8_t>(player->GetSpawnState());
					*msg << static_cast<int8_t>(player->mDirection);
					*msg << player->mSpeed;

					*msg << player->GetAnimFrame();

					*msg << player->mCharacterType;
					msg->PutData(player->mNickname, sizeof(player->mNickname));
				}
			}

		}
		if (msg != nullptr)
		{
			sendPost(targetPlayer.mSeqID, *msg);
			stats::RecordSend(PacketType::OTHER_CHARACTER_SPAWN_BATCH);
			msg = nullptr;
		}
	}

	void FieldServer::sendMonsterSpawnBatch(const Player& targetPlayer, const std::vector<map::Sector>& nearSector)
	{
		int16_t cnt = 0;
		for (const map::Sector& sector : nearSector)
		{
			cnt += static_cast<int16_t>(monsterSectors[sector.y][sector.x].size());
		}

		if (cnt == 0)
		{
			return;
		}

		constexpr int32_t RECORD_SIZE = sizeof(int64_t)
			+ sizeof(float) + sizeof(float)
			+ sizeof(int8_t)
			+ sizeof(int8_t) + sizeof(int32_t) + sizeof(int8_t) + sizeof(int32_t);

		constexpr int32_t maxBatch = static_cast<int32_t>(utility::eBufferSize::BufferSize - sizeof(int16_t) - sizeof(int16_t) - sizeof(Header)) / RECORD_SIZE;
		utility::Message* msg = nullptr;

		while (cnt != 0)
		{
			Header header{ 0 };
			int16_t monsterCnt = 0;
			for (const map::Sector& sector : nearSector)
			{
				for (const Monster* pMonster : monsterSectors[sector.y][sector.x])
				{
					const Monster& monster = *pMonster;
					if (monsterCnt == 0)
					{
						if (msg != nullptr)
						{
							sendPost(targetPlayer.mSeqID, *msg);
							stats::RecordSend(PacketType::MONSTER_SPAWN_BATCH);
							msg = nullptr;
						}
						monsterCnt = (cnt < maxBatch) ? cnt : maxBatch;

						msg = MY_NEW utility::Message();
						msg->InitMessage(targetPlayer.mSeqID.Value, 0);

						header.Len = sizeof(int16_t) + sizeof(int16_t) + RECORD_SIZE * monsterCnt;
						header.RandKey = 0;
						msg->PutData(&header, sizeof(header));

						*msg << (int16_t)PacketType::MONSTER_SPAWN_BATCH;
						*msg << monsterCnt;
					}

					--cnt;
					--monsterCnt;
					*msg << monster.mMonsterID;
					*msg << monster.mPos.x;
					*msg << monster.mPos.y;
					*msg << static_cast<int8_t>(monster.mState);
					*msg << static_cast<int8_t>(monster.mDirection);
					*msg << monster.mHp;
					*msg << monster.mMonsterType;
					*msg << monster.GetAnimFrame();

				}
			}
		}
		if (msg != nullptr)
		{
			sendPost(targetPlayer.mSeqID, *msg);
			stats::RecordSend(PacketType::MONSTER_SPAWN_BATCH);
			msg = nullptr;
		}
	}
	void FieldServer::sendMonsterDespawnBatch(const Player& targetPlayer, const std::vector<map::Sector>& nearSector)
	{
		for (const map::Sector& sector : nearSector)
		{
			for (const Monster* monster : monsterSectors[sector.y][sector.x])
			{
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(targetPlayer.mSeqID.Value, 0);

				Header header{ 0 };
				header.Len = sizeof(int16_t) + sizeof(int64_t);
				header.RandKey = 0;
				msg->PutData(&header, sizeof(header));

				*msg << (int16_t)PacketType::MONSTER_DESPAWN;
				*msg << monster->mMonsterID;

				sendPost(targetPlayer.mSeqID, *msg);
				stats::RecordSend(PacketType::MONSTER_DESPAWN);
			}
		}
	}
	void FieldServer::sendItemSpawnBatch(const Player& targetPlayer, const std::vector<map::Sector>& nearSector)
	{
		int16_t cnt = 0;
		for (const map::Sector& sector : nearSector)
		{
			cnt += static_cast<int16_t>(itemSectors[sector.y][sector.x].size());
		}

		if (cnt == 0)
		{
			return;
		}

		constexpr int32_t RECORD_SIZE = sizeof(int64_t)
			+ sizeof(int8_t)
			+ sizeof(float) + sizeof(float);

		constexpr int32_t maxBatch = static_cast<int32_t>(utility::eBufferSize::BufferSize - sizeof(int16_t) - sizeof(int16_t) - sizeof(Header)) / RECORD_SIZE;
		utility::Message* msg = nullptr;

		while (cnt != 0)
		{
			Header header{ 0 };
			int16_t itemCnt = 0;
			for (const map::Sector& sector : nearSector)
			{
				for (const FieldItem* pItem : itemSectors[sector.y][sector.x])
				{
					const FieldItem& item = *pItem;
					if (itemCnt == 0)
					{
						if (msg != nullptr)
						{
							sendPost(targetPlayer.mSeqID, *msg);
							stats::RecordSend(PacketType::ITEM_SPAWN_BATCH);
							msg = nullptr;
						}
						itemCnt = (cnt < maxBatch) ? cnt : maxBatch;

						msg = MY_NEW utility::Message();
						msg->InitMessage(targetPlayer.mSeqID.Value, 0);

						header.Len = sizeof(int16_t) + sizeof(int16_t) + RECORD_SIZE * itemCnt;
						header.RandKey = 0;
						msg->PutData(&header, sizeof(header));

						*msg << (int16_t)PacketType::ITEM_SPAWN_BATCH;
						*msg << itemCnt;
					}

					--cnt;
					--itemCnt;
					*msg << item.mItemUniqueId;
					*msg << static_cast<int8_t>(item.mItemId);
					*msg << item.mPos.x;
					*msg << item.mPos.y;
				}
			}
		}
		if (msg != nullptr)
		{
			sendPost(targetPlayer.mSeqID, *msg);
			stats::RecordSend(PacketType::ITEM_SPAWN_BATCH);
			msg = nullptr;
		}
	}
	void FieldServer::sendItemDespawnBatch(const Player& targetPlayer, const std::vector<map::Sector>& nearSector)
	{
		for (const map::Sector& sector : nearSector)
		{
			for (const FieldItem* item : itemSectors[sector.y][sector.x])
			{
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(targetPlayer.mSeqID.Value, 0);

				Header header{ 0 };
				header.Len = sizeof(int16_t) + sizeof(int64_t);
				header.RandKey = 0;
				msg->PutData(&header, sizeof(header));

				*msg << (int16_t)PacketType::ITEM_DESPAWN;
				*msg << item->mItemUniqueId;

				sendPost(targetPlayer.mSeqID, *msg);
				stats::RecordSend(PacketType::ITEM_DESPAWN);
			}
		}
	}
	void FieldServer::handleLootReq(utility::Message& msg, int64_t seqID)
	{
		int64_t itemUniqueId;
		msg >> itemUniqueId;

		std::unordered_map<int64_t, Player*>::iterator iter = players.find(seqID);
		MY_ASSERT(iter != players.end(), "패킷이 왔는데 현재 쓰레드에 Player가 없는 말이안되는 상황");
		Player& player = *iter->second;

		//switch (player.GetSpawnState())
		//{
		//case ePlayerState::Dead:
		//case ePlayerState::Stun:
		//	return;
		//default:
		//	break;
		//}

		auto itemIter = items.find(itemUniqueId);
		if (itemIter == items.end())
		{
			sendLootRes(player, false, eItemId::Max, 0);   // 실패: 이미 없어짐
			return;
		}

		FieldItem& item = *itemIter->second;

		if (LOOT_RANGE < (player.mPos - item.mPos))
		{
			sendLootRes(player, false, eItemId::Max, 0);   // 실패: 거리초과
			return;
		}

		player.mInventory[static_cast<int8_t>(item.mItemId)] += item.mCount;

		std::vector<map::Sector> nearSector;
		getAroundSectors(item.mSector, nearSector);
		broadcastItemDespawn(item, nearSector);

		sendLootRes(player, true, item.mItemId, item.mCount);
		InterlockedIncrement64(&mTotalRootItemCount);
		MY_ASSERT(itemSectors[item.mSector.y][item.mSector.x].find(&item) != itemSectors[item.mSector.y][item.mSector.x].end(), "섹터에 아이템이 없음");
		itemSectors[item.mSector.y][item.mSector.x].erase(&item);
		items.erase(itemIter);
		MY_DELETE& item;
	}
	void FieldServer::sendLootRes(const Player& targetPlayer, bool bSuccess, eItemId itemId, int32_t count)
	{
		Header header{ 0 };
		header.Len = sizeof(int16_t) + sizeof(int8_t) + sizeof(int8_t) + sizeof(int32_t);

		utility::Message* msg = MY_NEW utility::Message();
		msg->InitMessage(targetPlayer.mSeqID.Value, 0);
		msg->PutData(&header, sizeof(header));

		int8_t result = bSuccess ? 0 : 1;
		*msg << static_cast<int16_t>(PacketType::LOOT_RES);
		*msg << result;
		*msg << static_cast<int8_t>(itemId);
		*msg << count;

		sendPost(targetPlayer.mSeqID, *msg);
		stats::RecordSend(PacketType::LOOT_RES);
	}
	void FieldServer::handleRankingReq(utility::Message& msg, int64_t seqID)
	{
		std::unordered_map<int64_t, Player*>::iterator iter = players.find(seqID);
		MY_ASSERT(iter != players.end(), "패킷이 왔는데 현재 쓰레드에 Player가 없는 말이안되는 상황");
		Player& player = *iter->second;

		requestRankingQuery(player.mCurrentFieldID, player);
	}
	void FieldServer::broadcastDespawn(const Player& targetPlayer, const std::vector<map::Sector>& nearSector)
	{
		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				//echo 안함
				if (player->mCharacterID == targetPlayer.mCharacterID)
				{
					continue;
				}
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(player->mSeqID.Value, 0);

				Header header{ 0 };
				header.Len = sizeof(int16_t) + sizeof(int64_t);
				header.RandKey = 0;
				msg->PutData(&header, sizeof(header));

				*msg << (int16_t)PacketType::CHARACTER_DESPAWN;
				*msg << targetPlayer.mCharacterID;

				sendPost(player->mSeqID, *msg);
				stats::RecordSend(PacketType::CHARACTER_DESPAWN);
			}
		}
	}
	void FieldServer::sendDespawnBatch(const Player& targetPlayer, const std::vector<map::Sector>& nearSector)
	{
		for (const map::Sector& sector : nearSector)
		{
			for (const Player* player : sectors[sector.y][sector.x])
			{
				// 자신 캐릭터 삭제는 제외 
				if (player->mCharacterID == targetPlayer.mCharacterID)
				{
					continue;
				}
				utility::Message* msg = MY_NEW utility::Message();
				msg->InitMessage(targetPlayer.mSeqID.Value, 0);

				Header header{ 0 };
				header.Len = sizeof(int16_t) + sizeof(int64_t);
				header.RandKey = 0;
				msg->PutData(&header, sizeof(header));

				*msg << (int16_t)PacketType::CHARACTER_DESPAWN;
				*msg << player->mCharacterID;

				sendPost(targetPlayer.mSeqID, *msg);
				stats::RecordSend(PacketType::CHARACTER_DESPAWN);
			}
		}
	}
	void FieldServer::monitorThread() const
	{
		HWND hwnd = GetConsoleWindow();
		SetConsoleOutputCP(CP_UTF8);
		constexpr int x = 50;
		constexpr int y = 50;
		MoveWindow(hwnd, x, y, 40, 150, TRUE);
		system(" mode  con lines=40   cols=150 ");

		DWORD currentTime = timeGetTime();
		DWORD nextTime = currentTime + 1000;
		while (bMonitorOn)
		{
			currentTime = timeGetTime();
			if (nextTime <= currentTime)
			{
				std::cout >> *this;
				nextTime += 1000;
			}
			else
			{
				Sleep(nextTime - currentTime);
			}
		}
		std::cout >> *this;
	}
	//UTF-8 한글(3바이트, 화면 2칸)과 setw()의 바이트 기준 패딩이 어긋나는 걸 보정.
	//한글 글자 수만큼 setw에 더해줘야 화면상 실제 폭이 width에 맞춰짐.
	static int KoreanPad(const char* str, int width)
	{
		int extra = 0;
		for (const unsigned char* p = reinterpret_cast<const unsigned char*>(str); *p; ++p)
		{
			if ((*p & 0xF0) == 0xE0)	//UTF-8 3바이트 시퀀스 시작 바이트 (한글 음절 범위)
			{
				++extra;
			}
		}
		return width + extra;
	}

	//box-drawing 문자(─│┌┬┐├┼┤└┴┘)는 UTF-8로 3바이트지만 화면 폭은 1칸이라 KoreanPad 보정이 필요 없음.
	static std::string HLine(int width, const char* seg)
	{
		std::string line;
		for (int i = 0; i < width; ++i)
		{
			line += seg;
		}
		return line;
	}

	static std::string BuildBorder(const int* widths, int colCount, const char* left, const char* mid, const char* right)
	{
		std::string line = left;
		for (int i = 0; i < colCount; ++i)
		{
			line += HLine(widths[i], "─");
			line += (i + 1 < colCount ? mid : right);
		}
		return line;
	}

	struct KVRow
	{
		std::string label;
		std::string value;
	};

	//4개 박스를 한 줄에 나란히 배치하기 위해 폭을 고정값으로 공유 (박스마다 폭이 다르면 옆으로 못 붙임)
	constexpr int kKVLabelWidth = 18;
	constexpr int kKVValueWidth = 12;
	constexpr int kKVBoxWidth = kKVLabelWidth + kKVValueWidth + 3;	//│라벨│값│

	//"항목/값" 2열 박스 테이블을 한 줄씩 문자열로 만들어 반환 (직접 출력하지 않음 - 옆 박스와 나란히 찍기 위해)
	static std::vector<std::string> BuildKVBox(const char* title, const std::vector<KVRow>& rows)
	{
		const int widths[2] = { kKVLabelWidth, kKVValueWidth };
		std::vector<std::string> lines;

		std::ostringstream titleLine;
		titleLine << std::left << std::setw(KoreanPad(title, kKVBoxWidth)) << title;
		lines.push_back(titleLine.str());

		lines.push_back(BuildBorder(widths, 2, "┌", "┬", "┐"));

		std::ostringstream header;
		header << "│" << std::left << std::setw(KoreanPad("항목", kKVLabelWidth)) << "항목"
			<< "│" << std::setw(KoreanPad("값", kKVValueWidth)) << "값" << "│";
		lines.push_back(header.str());

		lines.push_back(BuildBorder(widths, 2, "├", "┼", "┤"));

		for (const KVRow& row : rows)
		{
			std::ostringstream rowStream;
			rowStream << "│" << std::left << std::setw(KoreanPad(row.label.c_str(), kKVLabelWidth)) << row.label
				<< "│" << std::right << std::setw(kKVValueWidth) << row.value << "│";
			lines.push_back(rowStream.str());
		}

		lines.push_back(BuildBorder(widths, 2, "└", "┴", "┘"));
		return lines;
	}

	//여러 박스(BuildKVBox 결과)를 가로로 나란히 출력. 줄 수가 다른 박스는 빈 칸으로 높이를 맞춤.
	//boxWidth: blocks 안 모든 줄의 공통 시각적 폭. 박스 스타일마다 폭이 달라서(KV박스 33, 타입박스 66)
	//호출자가 자기 폭을 넘겨줘야 함 - 안 맞으면 먼저 끝난 박스의 빈 자리 패딩이 짧아져 다음 박스가 밀림.
	static void PrintBoxesSideBySide(std::ostream& out, const std::vector<std::vector<std::string>>& blocks, int boxWidth)
	{
		size_t maxLines = 0;
		for (const std::vector<std::string>& block : blocks)
		{
			maxLines = (block.size() > maxLines) ? block.size() : maxLines;
		}

		for (size_t r = 0; r < maxLines; ++r)
		{
			for (const std::vector<std::string>& block : blocks)
			{
				if (r < block.size())
				{
					out << block[r];
				}
				else
				{
					out << std::string(boxWidth, ' ');	//이미 끝난 박스 자리는 공백으로 채워 다음 박스 위치를 맞춤
				}
				out << "  ";	//박스 사이 여백
			}
			out << "\n";
		}
		out << "\n";
	}

	std::ostream& operator>>(std::ostream& out, const FieldServer& server)
	{
		//초당값 계산용 이전 누적치 (호출자가 monitorThread 하나뿐이므로 static으로 충분)
		static int64_t prevSend = 0;
		static int64_t prevRecv = 0;

		int64_t AcceptCnt = server.GetAcceptCount();
		int64_t SendCount = server.GetSendCount();
		int64_t RecvCount = server.GetRecvCount();
		int64_t DisConnect = server.GetDisConnectCount();
		int64_t SessionCnt = server.GetSessionCount();
		int64_t playerCnt = server.mPlayerCnt;

		int64_t sendPerSec = SendCount - prevSend;
		int64_t recvPerSec = RecvCount - prevRecv;
		prevSend = SendCount;
		prevRecv = RecvCount;

		SYSTEMTIME st;
		GetLocalTime(&st);

		static uint64_t prevMessageIn = 0;
		static uint64_t prevMessageDeQ = 0;
		uint64_t messageInTPS = server.mMessageInQTPS - prevMessageIn;
		uint64_t messageDeQTPS = server.mMessageDeQTPS - prevMessageDeQ;
		prevMessageIn = server.mMessageInQTPS;
		prevMessageDeQ = server.mMessageDeQTPS;

		static uint64_t prevDelaySum = 0;
		uint64_t deltaDelaySum = server.mProcessDelaySum - prevDelaySum;
		prevDelaySum = server.mProcessDelaySum;
		double delayAvg = (messageDeQTPS > 0) ? (double)deltaDelaySum / (double)messageDeQTPS : 0.0;
		std::ostringstream delayStream;
		delayStream << std::fixed << std::setprecision(2) << delayAvg << "ms";

		out << "==== FieldServer [" << std::setfill('0')
			<< std::setw(2) << st.wHour << ":" << std::setw(2) << st.wMinute << ":" << std::setw(2) << st.wSecond
			<< "] ====\n" << std::setfill(' ');

		std::vector<KVRow> msgQRows = {
			{ "초당메시지유입", std::to_string(messageInTPS) },
			{ "초당메시지처리", std::to_string(messageDeQTPS) },
			{ "인증큐", std::to_string(server.mAuthMessageQCnt) },
			{ "플레이어큐", std::to_string(server.mPlayerMessageQCnt) },
			{ "평균처리지연", delayStream.str() },
		};
		for (int i = 0; i < CONFIG_FIELD_SIZE; ++i)
		{
			msgQRows.push_back({ "필드큐[" + std::to_string(i) + "]", std::to_string(server.mFieldMessageQCnt[i]) });
		}
		for (int i = 0; i < CONFIG_FIELD_SIZE; ++i)
		{
			msgQRows.push_back({ "DB큐[" + std::to_string(i) + "]", std::to_string(server.mDBMessageQCnt[i]) });
		}

		PrintBoxesSideBySide(out, {
			BuildKVBox("자원", {
				{ "세션", std::to_string(SessionCnt) },
				{ "플레이어", std::to_string(playerCnt) },
				{ "누적 접속", std::to_string(AcceptCnt) },
			}),
			BuildKVBox("Disconnect", {
				{ "접속종료", std::to_string(DisConnect) },
				{ "동기화강제종료", std::to_string(server.mDisconnect_Sync) },
				{ "하트비트강제종료", std::to_string(server.mDisconnect_HeartBeat) },
				{ "송신큐강제종료", std::to_string(server.mDisconnect_SendQisFull) },
			}),
			BuildKVBox("TPS", {
				{ "초당송신", std::to_string(sendPerSec) },
				{ "초당수신", std::to_string(recvPerSec) },
				{ "송신누적", std::to_string(SendCount) },
				{ "수신누적", std::to_string(RecvCount) },
			}),
			BuildKVBox("MsgQ", msgQRows),
			}, kKVBoxWidth);

		static const struct { PacketType type; const char* name; int group; } kTrackedTypes[] =
		{
			{ PacketType::FIELD_AUTH_REQ, "인증 요청", 0 },
			{ PacketType::FIELD_AUTH_RES, "인증 성공", 0 },
			{ PacketType::FIELD_AUTH_FAIL, "인증 실패", 0 },
			{ PacketType::OTHER_CHARACTER_SPAWN, "캐릭터 스폰", 1 },
			{ PacketType::OTHER_CHARACTER_SPAWN_BATCH, "캐릭터 스폰(배치)", 1 },
			{ PacketType::CHARACTER_DESPAWN, "캐릭터 디스폰", 1 },
			{ PacketType::MOVE_START, "이동 시작", 1 },
			{ PacketType::MOVE_STOP, "이동 정지", 1 },
			{ PacketType::PLAYER_ATTACK_REQ, "공격 요청", 1 },
			{ PacketType::OTHER_CHARACTER_ATTACK, "공격 브로드캐스트", 1 },
			{ PacketType::CHARACTER_DAMAGED, "플레이어 피격", 1 },
			{ PacketType::MONSTER_SPAWN, "몬스터 스폰", 2 },
			{ PacketType::MONSTER_SPAWN_BATCH, "몬스터 스폰(배치)", 2 },
			{ PacketType::MONSTER_DESPAWN, "몬스터 디스폰", 2 },
			{ PacketType::MONSTER_MOVE_START, "몬스터 이동 시작", 2 },
			{ PacketType::MONSTER_MOVE_STOP, "몬스터 이동 정지", 2 },
			{ PacketType::MONSTER_DAMAGED, "몬스터 피격", 2 },
			{ PacketType::MONSTER_ATTACK, "몬스터 공격", 2 },
			{ PacketType::ITEM_SPAWN, "아이템 스폰", 3 },
			{ PacketType::ITEM_SPAWN_BATCH, "아이템 스폰(배치)", 3 },
			{ PacketType::ITEM_DESPAWN, "아이템 디스폰", 3 },
			{ PacketType::LOOT_REQ, "루팅 요청", 3 },
			{ PacketType::LOOT_RES, "루팅 응답", 3 },
		};
		constexpr int kTrackedCount = sizeof(kTrackedTypes) / sizeof(kTrackedTypes[0]);
		static const char* kGroupNames[] = { "인증", "캐릭터", "몬스터", "아이템" };
		static int64_t prevTypeSend[kTrackedCount] = {};
		static int64_t prevTypeRecv[kTrackedCount] = {};
		constexpr int kItemGroup = 3;	//kGroupNames[3] == "아이템"

		constexpr int kTypeColWidth = 20;
		constexpr int kNumColWidth = 10;
		const int kColWidths[5] = { kTypeColWidth, kNumColWidth, kNumColWidth, kNumColWidth, kNumColWidth };
		constexpr int kTypeBoxWidth = kTypeColWidth + kNumColWidth * 4 + 6;	//│종류│송신│초당송신│수신│초당수신│ (파이프 6개)

		//그룹 하나(인증/캐릭터/몬스터/아이템)를 박스 테이블 문자열 줄 배열로 생성 (kTrackedTypes를 그룹별로 필터링)
		auto buildGroupLines = [&](int groupId, const char* title) -> std::vector<std::string>
			{
				std::vector<std::string> lines;

				std::ostringstream titleLine;
				titleLine << std::left << std::setw(KoreanPad(title, kTypeBoxWidth)) << title;
				lines.push_back(titleLine.str());

				lines.push_back(BuildBorder(kColWidths, 5, "┌", "┬", "┐"));

				std::ostringstream header;
				header << "│" << std::left << std::setw(KoreanPad("종류", kTypeColWidth)) << "종류"
					<< "│" << std::setw(KoreanPad("송신", kNumColWidth)) << "송신"
					<< "│" << std::setw(KoreanPad("초당송신", kNumColWidth)) << "초당송신"
					<< "│" << std::setw(KoreanPad("수신", kNumColWidth)) << "수신"
					<< "│" << std::setw(KoreanPad("초당수신", kNumColWidth)) << "초당수신" << "│";
				lines.push_back(header.str());

				lines.push_back(BuildBorder(kColWidths, 5, "├", "┼", "┤"));

				for (int i = 0; i < kTrackedCount; ++i)
				{
					if (kTrackedTypes[i].group != groupId)
					{
						continue;
					}
					int64_t sendCnt = stats::GetSendCount(kTrackedTypes[i].type);
					int64_t recvCnt = stats::GetRecvCount(kTrackedTypes[i].type);
					std::ostringstream row;
					row << "│" << std::left << std::setw(KoreanPad(kTrackedTypes[i].name, kTypeColWidth)) << kTrackedTypes[i].name
						<< "│" << std::right << std::setw(kNumColWidth) << sendCnt
						<< "│" << std::setw(kNumColWidth) << (sendCnt - prevTypeSend[i])
						<< "│" << std::setw(kNumColWidth) << recvCnt
						<< "│" << std::setw(kNumColWidth) << (recvCnt - prevTypeRecv[i]) << "│";
					lines.push_back(row.str());
					prevTypeSend[i] = sendCnt;
					prevTypeRecv[i] = recvCnt;
				}

				lines.push_back(BuildBorder(kColWidths, 5, "└", "┴", "┘"));
				if (groupId == kItemGroup)
				{
					lines.push_back("루팅 성공 누적 : " + std::to_string(server.mTotalRootItemCount));
				}
				return lines;
			};

		//아이템을 인증 옆으로, 몬스터를 캐릭터 옆으로 붙여서 세로 길이를 줄임
		PrintBoxesSideBySide(out, { buildGroupLines(0, kGroupNames[0]), buildGroupLines(3, kGroupNames[3]) }, kTypeBoxWidth);
		PrintBoxesSideBySide(out, { buildGroupLines(1, kGroupNames[1]), buildGroupLines(2, kGroupNames[2]) }, kTypeBoxWidth);

		return out;
	}
}